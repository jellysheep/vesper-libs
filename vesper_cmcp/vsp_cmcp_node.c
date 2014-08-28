/**
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#include "vsp_cmcp_node.h"
#include "vsp_cmcp_command.h"

#include <vesper_util/vsp_error.h>
#include <vesper_util/vsp_random.h>
#include <vesper_util/vsp_time.h>
#include <vesper_util/vsp_util.h>
#include <nanomsg/nn.h>
#include <nanomsg/pubsub.h>

/** Wall clock time in milliseconds until server receive times out.
 * Heartbeats can only be sent as often as the receive call times out,
 * so this has to be set to at most VSP_CMCP_NODE_HEARTBEAT_TIME. */
static const int VSP_CMCP_NODE_TIMEOUT = 1000;

/** Wall clock time in milliseconds between two heartbeat signals. */
#define VSP_CMCP_NODE_HEARTBEAT_TIME 1000

/**
 * Send previously created message to the specified socket.
 * Blocks until message could be sent.
 * Returns non-zero and sets vsp_error_num() if failed.
 */
static int vsp_cmcp_node_send_message(int socket,
    vsp_cmcp_message *cmcp_message);

/**
 * Subscribe the node to the specified topic ID.
 * Returns non-zero and sets vsp_error_num() if failed.
 */
static int vsp_cmcp_node_subscribe(vsp_cmcp_node *cmcp_node, uint16_t topic_id);

/** Event loop for message reception running in its own thread. */
static void *vsp_cmcp_node_run(void *param);

/** Check current time and send heartbeat if necessary. */
static int vsp_cmcp_node_heartbeat(vsp_cmcp_node *cmcp_node);

vsp_cmcp_node *vsp_cmcp_node_create(vsp_cmcp_node_type node_type,
    void (*message_callback)(void*, vsp_cmcp_message*), void *callback_param)
{
    vsp_cmcp_node *cmcp_node;
    /* check parameters */
    VSP_CHECK((node_type == VSP_CMCP_NODE_SERVER
        || node_type == VSP_CMCP_NODE_CLIENT) && message_callback != NULL,
        vsp_error_set_num(EINVAL); return NULL);
    /* allocate memory */
    VSP_ALLOC(cmcp_node, vsp_cmcp_node, return NULL);
    /* initialize struct data */
    cmcp_node->node_type = node_type;
    cmcp_node->state = vsp_cmcp_state_create(VSP_CMCP_NODE_UNINITIALIZED);
    /* in case of failure vsp_error_num() is already set */
    VSP_ASSERT(cmcp_node->state != NULL, VSP_FREE(cmcp_node); return NULL);
    /* generate node ID that does not equal broadcast topic ID */
    if (cmcp_node->node_type == VSP_CMCP_NODE_SERVER) {
        /* node type: ID is even (first bit cleared) */
        do {
            cmcp_node->id = (uint16_t) (vsp_random_get() << 1);
        } while (cmcp_node->id == VSP_CMCP_BROADCAST_TOPIC_ID);
    } else {
        /* node type: ID is odd (first bit set) */
        do {
            cmcp_node->id = (uint16_t) (vsp_random_get() | 1);
        } while (cmcp_node->id == VSP_CMCP_BROADCAST_TOPIC_ID);
    }
    cmcp_node->publish_socket = -1;
    cmcp_node->subscribe_socket = -1;
    cmcp_node->time_next_heartbeat = vsp_time_real_double();
    cmcp_node->message_callback = message_callback;
    cmcp_node->callback_param = callback_param;
    /* return struct pointer */
    return cmcp_node;
}

int vsp_cmcp_node_free(vsp_cmcp_node *cmcp_node)
{
    int ret;
    int success;

    /* check parameter */
    VSP_CHECK(cmcp_node != NULL, vsp_error_set_num(EINVAL); return -1);

    success = 0;

    /* clean up socket connections */
    if (vsp_cmcp_state_get(cmcp_node->state) > VSP_CMCP_NODE_INITIALIZED) {
        /* worker thread is running, stop it */
        ret = vsp_cmcp_node_stop(cmcp_node);
        /* check for error */
        VSP_ASSERT(ret == 0, success = -1);
    }

    if (vsp_cmcp_state_get(cmcp_node->state) > VSP_CMCP_NODE_UNINITIALIZED) {
        /* close publish socket */
        ret = nn_close(cmcp_node->publish_socket);
        /* check error set by nanomsg */
        VSP_ASSERT(ret == 0, success = -1);

        /* close subscribe socket */
        ret = nn_close(cmcp_node->subscribe_socket);
        /* check error set by nanomsg */
        VSP_ASSERT(ret == 0, success = -1);
    }

    /* clean up state struct */
    ret = vsp_cmcp_state_free(cmcp_node->state);
    /* check for error */
    VSP_ASSERT(ret == 0, success = -1);

    /* free memory */
    VSP_FREE(cmcp_node);
    return success;
}

int vsp_cmcp_node_connect(vsp_cmcp_node *cmcp_node,
    const char *publish_address, const char *subscribe_address)
{
    int ret;

    /* check parameters */
    VSP_CHECK(cmcp_node != NULL && publish_address != NULL
        && subscribe_address != NULL, vsp_error_set_num(EINVAL); return -1);
    /* check sockets not yet initialized */
    VSP_CHECK(vsp_cmcp_state_get(cmcp_node->state)
        == VSP_CMCP_NODE_UNINITIALIZED, vsp_error_set_num(EALREADY); return -1);

    /* initialize and connect publish socket */
    cmcp_node->publish_socket = nn_socket(AF_SP, NN_PUB);
    /* check error set by nanomsg */
    VSP_CHECK(cmcp_node->publish_socket != -1, return -1);
    /* connect or bind socket */
    if (cmcp_node->node_type == VSP_CMCP_NODE_SERVER) {
        ret = nn_bind(cmcp_node->publish_socket, publish_address);
    } else {
        ret = nn_connect(cmcp_node->publish_socket, publish_address);
    }
    /* check error set by nanomsg */
    VSP_CHECK(ret >= 0, nn_close(cmcp_node->publish_socket);
        return -1);

    /* initialize and connect subscribe socket */
    cmcp_node->subscribe_socket = nn_socket(AF_SP, NN_SUB);
    /* check error set by nanomsg, cleanup publish socket if failed */
    VSP_CHECK(cmcp_node->subscribe_socket != -1,
        nn_close(cmcp_node->publish_socket); return -1);
    /* connect or bind socket */
    if (cmcp_node->node_type == VSP_CMCP_NODE_SERVER) {
        ret = nn_bind(cmcp_node->subscribe_socket,
            subscribe_address);
    } else {
        ret = nn_connect(cmcp_node->subscribe_socket,
            subscribe_address);
    }
    /* check error set by nanomsg, cleanup both sockets if failed */
    VSP_CHECK(ret >= 0, nn_close(cmcp_node->publish_socket);
        nn_close(cmcp_node->subscribe_socket); return -1);
    /* set receive timeout */
    ret = nn_setsockopt(cmcp_node->subscribe_socket,
        NN_SOL_SOCKET, NN_RCVTIMEO,
        &VSP_CMCP_NODE_TIMEOUT, sizeof(VSP_CMCP_NODE_TIMEOUT));
    /* check error set by nanomsg, cleanup both sockets if failed */
    VSP_CHECK(ret >= 0, nn_close(cmcp_node->publish_socket);
        nn_close(cmcp_node->subscribe_socket); return -1);
    /* subscribe to broadcast ID */
    ret = vsp_cmcp_node_subscribe(cmcp_node, VSP_CMCP_BROADCAST_TOPIC_ID);
    /* check error set by nanomsg, cleanup both sockets if failed */
    VSP_CHECK(ret == 0, nn_close(cmcp_node->publish_socket);
        nn_close(cmcp_node->subscribe_socket); return -1);

    /* set state */
    vsp_cmcp_state_set(cmcp_node->state, VSP_CMCP_NODE_INITIALIZED);

    /* sockets successfully bound */
    return 0;
}

int vsp_cmcp_node_start(vsp_cmcp_node *cmcp_node)
{
    int ret;

    /* check parameter */
    VSP_ASSERT(cmcp_node != NULL, vsp_error_set_num(EINVAL); return -1);

    /* check sockets are initialized and thread is not running */
    VSP_ASSERT(vsp_cmcp_state_get(cmcp_node->state)
        == VSP_CMCP_NODE_INITIALIZED, vsp_error_set_num(EINVAL); return -1);

    /* mark thread as starting */
    vsp_cmcp_state_set(cmcp_node->state, VSP_CMCP_NODE_STARTING);

    /* lock state mutex */
    vsp_cmcp_state_lock(cmcp_node->state);

    /* start reception thread */
    ret = pthread_create(&cmcp_node->thread, NULL,
        vsp_cmcp_node_run, cmcp_node);

    /* check for pthread errors */
    VSP_ASSERT(ret == 0, vsp_cmcp_state_unlock(cmcp_node->state);
        vsp_error_set_num(ret); return -1);

    /* wait until thread is running; state is unlocked automatically */
    ret = vsp_cmcp_state_wait(cmcp_node->state, NULL);
    /* check for condition waiting errors */
    VSP_ASSERT(ret == 0, vsp_error_set_num(ret); return -1);

    /* check thread is running */
    VSP_ASSERT(vsp_cmcp_state_get(cmcp_node->state) == VSP_CMCP_NODE_RUNNING,
        vsp_error_set_num(EINVAL); return -1);

    /* success */
    return 0;
}

int vsp_cmcp_node_stop(vsp_cmcp_node *cmcp_node)
{
    int ret;

    /* check parameter */
    VSP_ASSERT(cmcp_node != NULL, vsp_error_set_num(EINVAL); return -1);

    /* check thread is running */
    VSP_ASSERT(vsp_cmcp_state_get(cmcp_node->state) == VSP_CMCP_NODE_RUNNING,
        vsp_error_set_num(EINVAL); return -1);

    /* stop reception thread */
    vsp_cmcp_state_set(cmcp_node->state, VSP_CMCP_NODE_STOPPING);

    /* wait for thread to join */
    ret = pthread_join(cmcp_node->thread, NULL);

    /* check thread has successfully stopped */
    VSP_ASSERT(ret == 0, vsp_error_set_num(EINTR); return -1);

    /* check state */
    VSP_ASSERT(vsp_cmcp_state_get(cmcp_node->state)
        == VSP_CMCP_NODE_INITIALIZED, vsp_error_set_num(EINTR); return -1);

    /* success */
    return 0;
}

int vsp_cmcp_node_send_message(int socket, vsp_cmcp_message *cmcp_message)
{
    int ret;
    int data_length;
    void *data_buffer;

    /* check parameters */
    VSP_ASSERT(socket != -1, vsp_error_set_num(EINVAL); return -1);
    VSP_ASSERT(cmcp_message != NULL, vsp_error_set_num(EINVAL); return -1);

    /* get message data length */
    data_length = vsp_cmcp_message_get_data_length(cmcp_message);
    /* check for error */
    VSP_ASSERT(data_length > 0, return -1);

    /* allocate zero-copy message buffer */
    data_buffer = nn_allocmsg(data_length, 0);
    /* check for error */
    VSP_ASSERT(data_buffer != NULL, return -1);

    /* write message data to buffer */
    ret = vsp_cmcp_message_get_data(cmcp_message, data_buffer);
    /* check for error */
    VSP_ASSERT(ret == 0, return -1);

    /* actually send message to socket */
    ret = nn_send(socket, &data_buffer, NN_MSG, 0);
    /* check for error */
    VSP_ASSERT(ret != -1, return -1);

    /* success */
    return 0;
}

int vsp_cmcp_node_create_send_message(vsp_cmcp_node *cmcp_node,
    uint16_t topic_id, uint16_t sender_id, uint16_t command_id,
    vsp_cmcp_datalist *cmcp_datalist)
{
    int ret;
    int success;
    vsp_cmcp_message *cmcp_message;

    /* clear state, no errors yet */
    success = 0;

    /* check parameters; data list may be NULL */
    VSP_ASSERT(cmcp_node != NULL, vsp_error_set_num(EINVAL); return -1);

    /* generate message */
    cmcp_message = vsp_cmcp_message_create(topic_id, sender_id, command_id,
        cmcp_datalist);
    /* check for error */
    VSP_ASSERT(cmcp_message != NULL, return -1);

    /* send message */
    ret = vsp_cmcp_node_send_message(cmcp_node->publish_socket, cmcp_message);
    /* check for error, but do not directly return because of memory leak */
    VSP_ASSERT(ret == 0, success = -1);

    /* free message */
    ret = vsp_cmcp_message_free(cmcp_message);
    /* check for error */
    VSP_ASSERT(ret == 0, return -1);

    /* success */
    return success;
}

int vsp_cmcp_node_recv_message(vsp_cmcp_node *cmcp_node, void **message_buffer)
{
    int data_length;

    /* check parameters */
    VSP_ASSERT(cmcp_node != NULL, vsp_error_set_num(EINVAL); return -1);
    VSP_ASSERT(message_buffer != NULL, vsp_error_set_num(EINVAL); return -1);

    /* try to receive message */
    data_length = nn_recv(cmcp_node->subscribe_socket,
        message_buffer, NN_MSG, 0);
    /* check for error */
    VSP_CHECK(data_length > 0, *message_buffer = NULL; return -1);

    /* success: return data length */
    return data_length;
}

int vsp_cmcp_node_subscribe(vsp_cmcp_node *cmcp_node, uint16_t topic_id)
{
    int ret;

    /* check parameter */
    VSP_ASSERT(cmcp_node != NULL, vsp_error_set_num(EINVAL); return -1);

    /* subscribe to topic ID */
    ret = nn_setsockopt(cmcp_node->subscribe_socket, NN_SUB, NN_SUB_SUBSCRIBE,
        &topic_id, sizeof(topic_id));
    /* check for error */
    VSP_ASSERT(ret == 0, return -1);

    /* success */
    return 0;
}

void *vsp_cmcp_node_run(void *param)
{
    vsp_cmcp_node *cmcp_node;
    int ret;
    int data_length;
    void *message_buffer;
    vsp_cmcp_message *cmcp_message;

    /* check parameter */
    VSP_ASSERT(param != NULL, vsp_error_set_num(EINVAL); return (void*) -1);

    /* initialize local variables */
    cmcp_node = (vsp_cmcp_node*) param;
    message_buffer = NULL;
    cmcp_message = NULL;

    /* check sockets are initialized and thread was started */
    VSP_ASSERT(vsp_cmcp_state_get(cmcp_node->state) == VSP_CMCP_NODE_STARTING,
        vsp_error_set_num(EINVAL); return (void*) -1);

    /* mark thread as running */
    vsp_cmcp_state_set(cmcp_node->state, VSP_CMCP_NODE_RUNNING);

    /* reception loop */
    while (vsp_cmcp_state_get(cmcp_node->state) == VSP_CMCP_NODE_RUNNING) {
        /* send heartbeat */
        ret = vsp_cmcp_node_heartbeat(cmcp_node);
        /* check error */
        VSP_ASSERT(ret == 0, return (void*) -1);

        /* try to receive message */
        data_length = vsp_cmcp_node_recv_message(cmcp_node, &message_buffer);
        /* check error: in case of failure just retry */
        VSP_CHECK(data_length > 0, goto cleanup);
        /* parse message data */
        cmcp_message = vsp_cmcp_message_create_parse(data_length,
            message_buffer);
        /* check error: in case of failure clean up and retry */
        VSP_CHECK(cmcp_message != NULL, goto cleanup);

        /* message successfully received; invoke callback function */
        cmcp_node->message_callback(cmcp_node->callback_param, cmcp_message);

        cleanup:
            /* clean up */
            if (cmcp_message != NULL) {
                ret = vsp_cmcp_message_free(cmcp_message);
                VSP_ASSERT(ret == 0,
                    /* failures are silently ignored in release build */);
                cmcp_message = NULL;
            }
            if (message_buffer != NULL) {
                ret = nn_freemsg(message_buffer);
                VSP_ASSERT(ret == 0,
                    /* failures are silently ignored in release build */);
                message_buffer = NULL;
            }
            continue;
    }

    /* check thread was requested to stop */
    VSP_ASSERT(vsp_cmcp_state_get(cmcp_node->state) == VSP_CMCP_NODE_STOPPING,
        vsp_error_set_num(EINTR); return (void*) -1);

    /* signal that thread has stopped */
    vsp_cmcp_state_set(cmcp_node->state, VSP_CMCP_NODE_INITIALIZED);

    /* success */
    return (void*) 0;
}

int vsp_cmcp_node_heartbeat(vsp_cmcp_node *cmcp_node)
{
    double time_now;
    int ret;
    uint16_t command_id;

    /* check parameter */
    VSP_ASSERT(cmcp_node != NULL, vsp_error_set_num(EINVAL); return -1);

    time_now = vsp_time_real_double();
    if (time_now < cmcp_node->time_next_heartbeat) {
        /* not sending heartbeat yet; nothing to fail, hence success */
        return 0;
    }

    /* update time of next heartbeat */
    cmcp_node->time_next_heartbeat =
        time_now + (VSP_CMCP_NODE_HEARTBEAT_TIME / 1000.0);

    /* send heartbeat */

    if (cmcp_node->node_type == VSP_CMCP_NODE_SERVER) {
        command_id = VSP_CMCP_COMMAND_SERVER_HEARTBEAT;
    } else {
        command_id = VSP_CMCP_COMMAND_CLIENT_HEARTBEAT;
    }
    ret = vsp_cmcp_node_create_send_message(cmcp_node,
        VSP_CMCP_BROADCAST_TOPIC_ID, cmcp_node->id, command_id, NULL);
    /* check for error */
    VSP_ASSERT(ret == 0, return -1);

    return 0;
}
