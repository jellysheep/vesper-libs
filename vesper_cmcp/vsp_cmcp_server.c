/**
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#include "vsp_cmcp_server.h"
#include "vsp_cmcp_common.h"

#include <vesper_util/vsp_error.h>
#include <vesper_util/vsp_random.h>
#include <vesper_util/vsp_time.h>
#include <vesper_util/vsp_util.h>
#include <nanomsg/nn.h>
#include <nanomsg/pubsub.h>
#include <pthread.h>

/** Regular time in seconds between two heartbeat signals (type double). */
#define VSP_CMCP_SERVER_HEARTBEAT_TIME 1.0

/** vsp_cmcp_server finite state machine flag. */
typedef enum {
    /** Sockets are not initialized and not bound. */
    VSP_CMCP_SERVER_UNINITIALIZED,
    /** Sockets are initialized and bound. */
    VSP_CMCP_SERVER_INITIALIZED,
    /** Message reception thread was started. */
    VSP_CMCP_SERVER_STARTING,
    /** Message reception thread is running. */
    VSP_CMCP_SERVER_RUNNING,
    /** Message reception thread was stopped. */
    VSP_CMCP_SERVER_STOPPING
} vsp_cmcp_server_state;

/** State and other data used for network connection. */
struct vsp_cmcp_server {
    /** Finite state machine flag. */
    volatile vsp_cmcp_server_state state;
    /** ID identifying this server in the network.
     * Even number, must not equal broadcast topic ID. */
    uint16_t id;
    /** nanomsg socket number to publish messages. */
    int publish_socket;
    /** nanomsg socket number to receive messages. */
    int subscribe_socket;
    /** Reception thread. */
    pthread_t thread;
    /** State mutex. */
    pthread_mutex_t mutex;
    /** Condition variable used for safe thread start. */
    pthread_cond_t condition;
    /** Real time of next heartbeat. */
    double time_next_heartbeat;
};

/** Start message reception thread and wait until thread has started.
 * Returns non-zero and sets vsp_error_num() if failed. */
static int vsp_cmcp_server_start(vsp_cmcp_server *cmcp_server);

/** Stop message reception thread and wait until thread has finished and joined.
 * Returns non-zero and sets vsp_error_num() if thread or this method failed. */
static int vsp_cmcp_server_stop(vsp_cmcp_server *cmcp_server);

/** Event loop for message reception running in its own thread. */
static void *vsp_cmcp_server_run(void *param);

/** Check current time and send heartbeat if necessary. */
static int vsp_cmcp_server_heartbeat(vsp_cmcp_server *cmcp_server);

vsp_cmcp_server *vsp_cmcp_server_create(void)
{
    vsp_cmcp_server *cmcp_server;
    /* allocate memory */
    VSP_ALLOC(cmcp_server, vsp_cmcp_server, return NULL);
    /* initialize struct data */
    cmcp_server->state = VSP_CMCP_SERVER_UNINITIALIZED;
    /* generate server ID that is even (first bit cleared)
     * but does not equal broadcast topic ID */
    do {
        cmcp_server->id = (uint16_t) (vsp_random_get() << 1);
    } while (cmcp_server->id == VSP_CMCP_BROADCAST_TOPIC_ID);
    cmcp_server->publish_socket = -1;
    cmcp_server->subscribe_socket = -1;
    cmcp_server->time_next_heartbeat = vsp_time_real();
    pthread_mutex_init(&cmcp_server->mutex, NULL);
    pthread_cond_init(&cmcp_server->condition, NULL);
    /* return struct pointer */
    return cmcp_server;
}

int vsp_cmcp_server_free(vsp_cmcp_server *cmcp_server)
{
    int ret;
    int success;

    success = 0;

    /* check parameter */
    VSP_ASSERT(cmcp_server != NULL, vsp_error_set_num(EINVAL); return -1);

    if (cmcp_server->state > VSP_CMCP_SERVER_INITIALIZED) {
        /* worker thread is running, stop it */
        ret = vsp_cmcp_server_stop(cmcp_server);
        /* check for error */
        VSP_ASSERT(ret == 0, success = -1);
    }

    if (cmcp_server->state > VSP_CMCP_SERVER_UNINITIALIZED) {
        /* close publish socket */
        ret = nn_close(cmcp_server->publish_socket);
        /* check error set by nanomsg */
        VSP_ASSERT(ret == 0, success = -1);

        /* close subscribe socket */
        ret = nn_close(cmcp_server->subscribe_socket);
        /* check error set by nanomsg */
        VSP_ASSERT(ret == 0, success = -1);
    }
    pthread_mutex_destroy(&cmcp_server->mutex);
    pthread_cond_destroy(&cmcp_server->condition);
    /* free memory */
    VSP_FREE(cmcp_server);
    return success;
}

int vsp_cmcp_server_bind(vsp_cmcp_server *cmcp_server,
    const char *publish_address, const char *subscribe_address)
{
    int ret;

    /* check parameters */
    VSP_ASSERT(cmcp_server != NULL && publish_address != NULL
        && subscribe_address != NULL, vsp_error_set_num(EINVAL); return -1);
    /* check sockets not yet initialized */
    VSP_ASSERT(cmcp_server->state == VSP_CMCP_SERVER_UNINITIALIZED,
        vsp_error_set_num(EALREADY); return -1);

    /* initialize sockets */
    cmcp_server->publish_socket = nn_socket(AF_SP, NN_PUB);
    cmcp_server->subscribe_socket = nn_socket(AF_SP, NN_SUB);

    /* bind sockets */
    ret = nn_bind(cmcp_server->publish_socket, publish_address);
    /* check error set by nanomsg */
    VSP_ASSERT(ret >= 0, return -1);

    ret = nn_bind(cmcp_server->subscribe_socket, subscribe_address);
    /* check error set by nanomsg */
    VSP_ASSERT(ret >= 0, return -1);

    /* set state */
    cmcp_server->state = VSP_CMCP_SERVER_INITIALIZED;

    /* start worker thread */
    ret = vsp_cmcp_server_start(cmcp_server);
    /* check error */
    VSP_ASSERT(ret == 0, return -1);

    /* sockets successfully bound */
    return 0;
}

int vsp_cmcp_server_start(vsp_cmcp_server *cmcp_server)
{
    int ret;

    /* check parameter */
    VSP_ASSERT(cmcp_server != NULL, vsp_error_set_num(EINVAL); return -1);

    /* check sockets are initialized and thread is not running */
    VSP_ASSERT(cmcp_server->state == VSP_CMCP_SERVER_INITIALIZED,
        vsp_error_set_num(EINVAL); return -1);

    /* lock state mutex */
    pthread_mutex_lock(&cmcp_server->mutex);

    /* mark thread as starting */
    cmcp_server->state = VSP_CMCP_SERVER_STARTING;

    /* start reception thread */
    ret = pthread_create(&cmcp_server->thread, NULL, vsp_cmcp_server_run,
        (void*) cmcp_server);

    /* check for pthread errors */
    VSP_ASSERT(ret == 0, pthread_mutex_unlock(&cmcp_server->mutex);
        vsp_error_set_num(ret); return -1);

    /* wait until thread is running */
    ret = pthread_cond_wait(&cmcp_server->condition, &cmcp_server->mutex);
    /* unlock mutex again */
    pthread_mutex_unlock(&cmcp_server->mutex);
    /* check for condition waiting errors */
    VSP_ASSERT(ret == 0, vsp_error_set_num(ret); return -1);

    /* check thread is running */
    VSP_ASSERT(cmcp_server->state == VSP_CMCP_SERVER_RUNNING,
        vsp_error_set_num(EINVAL); return -1);

    /* success */
    return 0;
}

int vsp_cmcp_server_stop(vsp_cmcp_server *cmcp_server)
{
    int ret;

    /* check parameter */
    VSP_ASSERT(cmcp_server != NULL, vsp_error_set_num(EINVAL); return -1);

    /* check thread is running */
    VSP_ASSERT(cmcp_server->state == VSP_CMCP_SERVER_RUNNING,
        vsp_error_set_num(EINVAL); return -1);

    /* stop reception thread */
    cmcp_server->state = VSP_CMCP_SERVER_STOPPING;

    /* wait for thread to join */
    ret = pthread_join(cmcp_server->thread, NULL);

    /* check thread has successfully stopped */
    VSP_ASSERT(ret == 0, vsp_error_set_num(EINTR); return -1);

    /* check state */
    VSP_ASSERT(cmcp_server->state == VSP_CMCP_SERVER_INITIALIZED,
        vsp_error_set_num(EINTR); return -1);

    /* success */
    return 0;
}

void *vsp_cmcp_server_run(void *param)
{
    vsp_cmcp_server *cmcp_server;
    int ret;

    /* check parameter */
    VSP_ASSERT(param != NULL, vsp_error_set_num(EINVAL); return (void*) -1);

    cmcp_server = (vsp_cmcp_server*) param;

    /* check sockets are initialized and thread was started */
    VSP_ASSERT(cmcp_server->state == VSP_CMCP_SERVER_STARTING,
        vsp_error_set_num(EINVAL); return (void*) -1);

    /* lock mutex */
    pthread_mutex_lock(&cmcp_server->mutex);
    /* mark thread as running */
    cmcp_server->state = VSP_CMCP_SERVER_RUNNING;
    /* signal waiting thread */
    pthread_cond_signal(&cmcp_server->condition);
    /* unlock mutex */
    pthread_mutex_unlock(&cmcp_server->mutex);

    /* reception loop */
    while (cmcp_server->state == VSP_CMCP_SERVER_RUNNING) {
        ret = vsp_cmcp_server_heartbeat(cmcp_server);
        /* check error */
        VSP_ASSERT(ret == 0, return (void*) -1);
    }

    /* check thread was requested to stop */
    VSP_ASSERT(cmcp_server->state == VSP_CMCP_SERVER_STOPPING,
        vsp_error_set_num(EINTR); return (void*) -1);

    /* signal that thread has stopped */
    cmcp_server->state = VSP_CMCP_SERVER_INITIALIZED;

    /* success */
    return (void*) 0;
}

int vsp_cmcp_server_heartbeat(vsp_cmcp_server *cmcp_server)
{
    double time_now;

    /* check parameter */
    VSP_ASSERT(cmcp_server != NULL, vsp_error_set_num(EINVAL); return -1);

    time_now = vsp_time_real();
    if (time_now < cmcp_server->time_next_heartbeat) {
        /* not sending heartbeat yet; nothing to fail, hence success */
        return 0;
    }

    /* update time of next heartbeat */
    cmcp_server->time_next_heartbeat =
        time_now + VSP_CMCP_SERVER_HEARTBEAT_TIME;

    /* send heartbeat */
    /* ... */

    return 0;
}
