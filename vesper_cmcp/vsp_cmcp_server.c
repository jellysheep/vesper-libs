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
#include <pthread.h>

/** Wall clock time in milliseconds between two server heartbeat signals. */
#define VSP_CMCP_SERVER_HEARTBEAT_TIME 1000

/** State and other data used for network connection. */
struct vsp_cmcp_server {
    /** Basic node and finite-state machine data. */
    vsp_cmcp_node *cmcp_node;
    /** Real time of next heartbeat. */
    double time_next_heartbeat;
};

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

    /* check parameter */
    VSP_CHECK(cmcp_server != NULL, vsp_error_set_num(EINVAL); return -1);

    /* bind sockets */
    ret = vsp_cmcp_node_connect(cmcp_server->cmcp_node,
        publish_address, subscribe_address);
    /* check error */
    VSP_CHECK(ret == 0, return -1);

    /* start worker thread */
    ret = vsp_cmcp_node_start(cmcp_server->cmcp_node, vsp_cmcp_server_run,
        cmcp_server);
    /* check error */
    VSP_ASSERT(ret == 0, return -1);

    /* sockets successfully bound */
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
    VSP_ASSERT(cmcp_server->cmcp_node->state == VSP_CMCP_NODE_STARTING,
        vsp_error_set_num(EINVAL); return (void*) -1);

    /* lock mutex */
    pthread_mutex_lock(&cmcp_server->cmcp_node->mutex);
    /* mark thread as running */
    cmcp_server->cmcp_node->state = VSP_CMCP_NODE_RUNNING;
    /* signal waiting thread */
    pthread_cond_signal(&cmcp_server->cmcp_node->condition);
    /* unlock mutex */
    pthread_mutex_unlock(&cmcp_server->cmcp_node->mutex);

    /* reception loop */
    while (cmcp_server->cmcp_node->state == VSP_CMCP_NODE_RUNNING) {
        ret = vsp_cmcp_server_heartbeat(cmcp_server);
        /* check error */
        VSP_ASSERT(ret == 0, return (void*) -1);
    }

    /* check thread was requested to stop */
    VSP_ASSERT(cmcp_server->cmcp_node->state == VSP_CMCP_NODE_STOPPING,
        vsp_error_set_num(EINTR); return (void*) -1);

    /* signal that thread has stopped */
    cmcp_server->cmcp_node->state = VSP_CMCP_NODE_INITIALIZED;

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
        time_now + (VSP_CMCP_SERVER_HEARTBEAT_TIME / 1000.0);

    /* send heartbeat */
    ret = vsp_cmcp_node_create_send_message(cmcp_server->cmcp_node,
        0, cmcp_server->cmcp_node->id, VSP_CMCP_COMMAND_HEARTBEAT, NULL);
    /* check for error */
    VSP_ASSERT(ret == 0, return -1);

    return 0;
}
