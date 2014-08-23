/**
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#include "vsp_cmcp_client.h"
#include "vsp_cmcp_command.h"
#include "vsp_cmcp_node.h"

#include <vesper_util/vsp_error.h>
#include <vesper_util/vsp_random.h>
#include <vesper_util/vsp_time.h>
#include <vesper_util/vsp_util.h>
#include <pthread.h>

/** Wall clock time in milliseconds between two client heartbeat signals. */
#define VSP_CMCP_CLIENT_HEARTBEAT_TIME 1000

/** Wall clock time in milliseconds between two client heartbeat signals. */
#define VSP_CMCP_CLIENT_CONNECTION_TIMEOUT 10000

/** State and other data used for network connection. */
struct vsp_cmcp_client {
    /** Basic node and finite-state machine data. */
    vsp_cmcp_node *cmcp_node;
};

/** Connect to server and establish connection using handshake.
 * Connection establishment is specified by CMCP protocol.
 * Returns non-zero and sets vsp_error_num() if failed. */
static int vsp_cmcp_client_establish_connection(vsp_cmcp_client *cmcp_client);

/** Event loop for message reception running in its own thread. */
static void *vsp_cmcp_client_run(void *param);

vsp_cmcp_client *vsp_cmcp_client_create(void)
{
    vsp_cmcp_client *cmcp_client;
    /* allocate memory */
    VSP_ALLOC(cmcp_client, vsp_cmcp_client, return NULL);
    /* initialize base type */
    cmcp_client->cmcp_node = vsp_cmcp_node_create(VSP_CMCP_NODE_CLIENT);
    /* in case of failure vsp_error_num() is already set */
    VSP_ASSERT(cmcp_client->cmcp_node != NULL,
        VSP_FREE(cmcp_client); return NULL);
    /* return struct pointer */
    return cmcp_client;
}

int vsp_cmcp_client_free(vsp_cmcp_client *cmcp_client)
{
    int ret;
    int success;

    success = 0;

    /* check parameter */
    VSP_CHECK(cmcp_client != NULL, vsp_error_set_num(EINVAL); return -1);

    /* free node base type */
    ret = vsp_cmcp_node_free(cmcp_client->cmcp_node);
    VSP_ASSERT(ret == 0,
        /* failures are silently ignored in release build */);

    /* free memory */
    VSP_FREE(cmcp_client);
    return success;
}

int vsp_cmcp_client_connect(vsp_cmcp_client *cmcp_client,
    const char *publish_address, const char *subscribe_address)
{
    int ret;

    /* check parameter */
    VSP_CHECK(cmcp_client != NULL, vsp_error_set_num(EINVAL); return -1);

    /* connect sockets; the node's state is checked by this function */
    ret = vsp_cmcp_node_connect(cmcp_client->cmcp_node,
        publish_address, subscribe_address);
    /* check error */
    VSP_CHECK(ret == 0, return -1);

    /* establish connection */
    ret = vsp_cmcp_client_establish_connection(cmcp_client);
    /* check error */
    VSP_CHECK(ret == 0, return -1);

    /* start worker thread */
    ret = vsp_cmcp_node_start(cmcp_client->cmcp_node, vsp_cmcp_client_run,
        cmcp_client);
    /* check error */
    VSP_ASSERT(ret == 0, return -1);

    /* successfully connected and waiting for messages */
    return 0;
}

int vsp_cmcp_client_establish_connection(vsp_cmcp_client *cmcp_client)
{
    int ret;
    vsp_cmcp_message *cmcp_message;
    double time_now, time_connection_timeout;
    uint16_t topic_id, sender_id, command_id;

    /* check parameter */
    VSP_CHECK(cmcp_client != NULL, vsp_error_set_num(EINVAL); return -1);

    /* initialize IDs */
    topic_id = 0;
    sender_id = 0;
    command_id = 0;

    /* start measuring time for heartbeat timeout */
    time_now = vsp_time_real();
    time_connection_timeout =
        time_now + (VSP_CMCP_CLIENT_CONNECTION_TIMEOUT / 1000.0);

    do {
        /* try to receive message */
        cmcp_message = vsp_cmcp_node_recv_message(cmcp_client->cmcp_node);
        /* measure passed time */
        time_now = vsp_time_real();
        /* check error: in case of failure just retry */
        VSP_CHECK(cmcp_message != NULL, continue);
        /* get and check message data; in case of failure just retry */
        ret = vsp_cmcp_message_get_id(cmcp_message,
            VSP_CMCP_MESSAGE_TOPIC_ID, &topic_id);
        VSP_CHECK(ret == 0, continue);
        ret = vsp_cmcp_message_get_id(cmcp_message,
            VSP_CMCP_MESSAGE_SENDER_ID, &sender_id);
        VSP_CHECK(ret == 0, continue);
        ret = vsp_cmcp_message_get_id(cmcp_message,
            VSP_CMCP_MESSAGE_COMMAND_ID, &command_id);
        VSP_CHECK(ret == 0, continue);
        /* check if server heartbeat received, else retry */
        VSP_CHECK(topic_id == VSP_CMCP_BROADCAST_TOPIC_ID
            && sender_id != 0 && (sender_id & 1) == 0
            && command_id == VSP_CMCP_COMMAND_HEARTBEAT, continue);
        /* success, do not retry */
        break;
    } while (time_now < time_connection_timeout);

    /* check for error */
    VSP_CHECK(cmcp_message != NULL, vsp_error_set_num(ENOTCONN); return -1);

    /* server heartbeat received */

    /* success */
    return 0;
}

void *vsp_cmcp_client_run(void *param)
{
    vsp_cmcp_client *cmcp_client;

    /* check parameter */
    VSP_ASSERT(param != NULL, vsp_error_set_num(EINVAL); return (void*) -1);

    cmcp_client = (vsp_cmcp_client*) param;

    /* check sockets are initialized and thread was started */
    VSP_ASSERT(cmcp_client->cmcp_node->state == VSP_CMCP_NODE_STARTING,
        vsp_error_set_num(EINVAL); return (void*) -1);

    /* lock mutex */
    pthread_mutex_lock(&cmcp_client->cmcp_node->mutex);
    /* mark thread as running */
    cmcp_client->cmcp_node->state = VSP_CMCP_NODE_RUNNING;
    /* signal waiting thread */
    pthread_cond_signal(&cmcp_client->cmcp_node->condition);
    /* unlock mutex */
    pthread_mutex_unlock(&cmcp_client->cmcp_node->mutex);

    /* reception loop */
    while (cmcp_client->cmcp_node->state == VSP_CMCP_NODE_RUNNING) {

    }

    /* check thread was requested to stop */
    VSP_ASSERT(cmcp_client->cmcp_node->state == VSP_CMCP_NODE_STOPPING,
        vsp_error_set_num(EINTR); return (void*) -1);

    /* signal that thread has stopped */
    cmcp_client->cmcp_node->state = VSP_CMCP_NODE_INITIALIZED;

    /* success */
    return (void*) 0;
}
