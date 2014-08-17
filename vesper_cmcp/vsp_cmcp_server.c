/**
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#include "vsp_cmcp_server.h"
#include "vsp_cmcp_command.h"
#include "vsp_cmcp_node.h"

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
    /** Basic node and finite-state machine data. */
    vsp_cmcp_node *cmcp_node;
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
    /* initialize base type */
    cmcp_server->cmcp_node = vsp_cmcp_node_create(VSP_CMCP_NODE_SERVER);
    /* in case of failure vsp_error_num() is already set */
    VSP_ASSERT(cmcp_server->cmcp_node != NULL,
        VSP_FREE(cmcp_server); return NULL);
    /* initialize other struct data */
    cmcp_server->time_next_heartbeat = vsp_time_real();
    /* return struct pointer */
    return cmcp_server;
}

int vsp_cmcp_server_free(vsp_cmcp_server *cmcp_server)
{
    int ret;
    int success;

    success = 0;

    /* check parameter */
    VSP_CHECK(cmcp_server != NULL, vsp_error_set_num(EINVAL); return -1);

    if (cmcp_server->cmcp_node->state > VSP_CMCP_SERVER_INITIALIZED) {
        /* worker thread is running, stop it */
        ret = vsp_cmcp_server_stop(cmcp_server);
        /* check for error */
        VSP_ASSERT(ret == 0, success = -1);
    }

    if (cmcp_server->cmcp_node->state > VSP_CMCP_SERVER_UNINITIALIZED) {
        /* close publish socket */
        ret = nn_close(cmcp_server->cmcp_node->publish_socket);
        /* check error set by nanomsg */
        VSP_ASSERT(ret == 0, success = -1);

        /* close subscribe socket */
        ret = nn_close(cmcp_server->cmcp_node->subscribe_socket);
        /* check error set by nanomsg */
        VSP_ASSERT(ret == 0, success = -1);
    }
    /* free node base type */
    ret = vsp_cmcp_node_free(cmcp_server->cmcp_node);
    VSP_ASSERT(ret == 0,
        /* failures are silently ignored in release build */);

    /* free memory */
    VSP_FREE(cmcp_server);
    return success;
}

int vsp_cmcp_server_bind(vsp_cmcp_server *cmcp_server,
    const char *publish_address, const char *subscribe_address)
{
    int ret;

    /* check parameters */
    VSP_CHECK(cmcp_server != NULL && publish_address != NULL
        && subscribe_address != NULL, vsp_error_set_num(EINVAL); return -1);
    /* check sockets not yet initialized */
    VSP_CHECK(cmcp_server->cmcp_node->state == VSP_CMCP_SERVER_UNINITIALIZED,
        vsp_error_set_num(EALREADY); return -1);

    /* initialize and bind publish socket */
    cmcp_server->cmcp_node->publish_socket = nn_socket(AF_SP, NN_PUB);
    /* check error set by nanomsg */
    VSP_CHECK(cmcp_server->cmcp_node->publish_socket != -1, return -1);
    /* bind socket */
    ret = nn_bind(cmcp_server->cmcp_node->publish_socket, publish_address);
    /* check error set by nanomsg */
    VSP_CHECK(ret >= 0, nn_close(cmcp_server->cmcp_node->publish_socket);
        return -1);

    /* initialize and bind subscribe socket */
    cmcp_server->cmcp_node->subscribe_socket = nn_socket(AF_SP, NN_SUB);
    /* check error set by nanomsg, cleanup publish socket if failed */
    VSP_CHECK(cmcp_server->cmcp_node->subscribe_socket != -1,
        nn_close(cmcp_server->cmcp_node->publish_socket); return -1);
    /* bind socket */
    ret = nn_bind(cmcp_server->cmcp_node->subscribe_socket, subscribe_address);
    /* check error set by nanomsg, cleanup both sockets if failed */
    VSP_CHECK(ret >= 0, nn_close(cmcp_server->cmcp_node->publish_socket);
        nn_close(cmcp_server->cmcp_node->subscribe_socket); return -1);

    /* set state */
    cmcp_server->cmcp_node->state = VSP_CMCP_SERVER_INITIALIZED;

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
    VSP_ASSERT(cmcp_server->cmcp_node->state == VSP_CMCP_SERVER_INITIALIZED,
        vsp_error_set_num(EINVAL); return -1);

    /* lock state mutex */
    pthread_mutex_lock(&cmcp_server->cmcp_node->mutex);

    /* mark thread as starting */
    cmcp_server->cmcp_node->state = VSP_CMCP_SERVER_STARTING;

    /* start reception thread */
    ret = pthread_create(&cmcp_server->cmcp_node->thread, NULL,
        vsp_cmcp_server_run, (void*) cmcp_server);

    /* check for pthread errors */
    VSP_ASSERT(ret == 0, pthread_mutex_unlock(&cmcp_server->cmcp_node->mutex);
        vsp_error_set_num(ret); return -1);

    /* wait until thread is running */
    ret = pthread_cond_wait(&cmcp_server->cmcp_node->condition,
        &cmcp_server->cmcp_node->mutex);
    /* unlock mutex again */
    pthread_mutex_unlock(&cmcp_server->cmcp_node->mutex);
    /* check for condition waiting errors */
    VSP_ASSERT(ret == 0, vsp_error_set_num(ret); return -1);

    /* check thread is running */
    VSP_ASSERT(cmcp_server->cmcp_node->state == VSP_CMCP_SERVER_RUNNING,
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
    VSP_ASSERT(cmcp_server->cmcp_node->state == VSP_CMCP_SERVER_RUNNING,
        vsp_error_set_num(EINVAL); return -1);

    /* stop reception thread */
    cmcp_server->cmcp_node->state = VSP_CMCP_SERVER_STOPPING;

    /* wait for thread to join */
    ret = pthread_join(cmcp_server->cmcp_node->thread, NULL);

    /* check thread has successfully stopped */
    VSP_ASSERT(ret == 0, vsp_error_set_num(EINTR); return -1);

    /* check state */
    VSP_ASSERT(cmcp_server->cmcp_node->state == VSP_CMCP_SERVER_INITIALIZED,
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
    VSP_ASSERT(cmcp_server->cmcp_node->state == VSP_CMCP_SERVER_STARTING,
        vsp_error_set_num(EINVAL); return (void*) -1);

    /* lock mutex */
    pthread_mutex_lock(&cmcp_server->cmcp_node->mutex);
    /* mark thread as running */
    cmcp_server->cmcp_node->state = VSP_CMCP_SERVER_RUNNING;
    /* signal waiting thread */
    pthread_cond_signal(&cmcp_server->cmcp_node->condition);
    /* unlock mutex */
    pthread_mutex_unlock(&cmcp_server->cmcp_node->mutex);

    /* reception loop */
    while (cmcp_server->cmcp_node->state == VSP_CMCP_SERVER_RUNNING) {
        ret = vsp_cmcp_server_heartbeat(cmcp_server);
        /* check error */
        VSP_ASSERT(ret == 0, return (void*) -1);
    }

    /* check thread was requested to stop */
    VSP_ASSERT(cmcp_server->cmcp_node->state == VSP_CMCP_SERVER_STOPPING,
        vsp_error_set_num(EINTR); return (void*) -1);

    /* signal that thread has stopped */
    cmcp_server->cmcp_node->state = VSP_CMCP_SERVER_INITIALIZED;

    /* success */
    return (void*) 0;
}

int vsp_cmcp_server_heartbeat(vsp_cmcp_server *cmcp_server)
{
    double time_now;
    int ret;

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
    ret = vsp_cmcp_node_create_send_message(cmcp_server->cmcp_node,
        0, cmcp_server->cmcp_node->id, VSP_CMCP_COMMAND_HEARTBEAT, NULL);
    /* check for error */
    VSP_ASSERT(ret == 0, return -1);

    return 0;
}
