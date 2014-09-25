/**
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#include "vsp_cmcp_node.h"
#include "vsp_cmcp_command.h"
#include "vsp_cmcp_state.h"

#include <vesper_util/vsp_error.h>
#include <vesper_util/vsp_random.h>
#include <vesper_util/vsp_time.h>
#include <vesper_util/vsp_util.h>
#include <nanomsg/nn.h>
#include <nanomsg/pubsub.h>
#include <pthread.h>

/** Wall clock time in milliseconds between two heartbeat signals.
 * This is also the timeout of a receive call of a node. */
const int VSP_CMCP_NODE_HEARTBEAT_TIME = 500;

/** vsp_cmcp_node finite state machine flag. */
typedef enum {
    /** Sockets are not initialized and not connected. */
    VSP_CMCP_NODE_UNINITIALIZED,
    /** Sockets are initialized and connected. */
    VSP_CMCP_NODE_INITIALIZED,
    /** Message reception thread was started. */
    VSP_CMCP_NODE_STARTING,
    /** Message reception thread was stopped. */
    VSP_CMCP_NODE_STOPPING,
    /** Message reception thread is running. */
    VSP_CMCP_NODE_RUNNING
} vsp_cmcp_node_state;

/** State and other data used for network connection.
 * Base type for vsp_cmcp_server and vsp_cmcp_client. */
struct vsp_cmcp_node {
    /** Node type. */
    vsp_cmcp_node_type node_type;
    /** ID identifying this node in the network.
     * Must not equal broadcast topic ID. */
    uint16_t id;
    /** Finite state machine struct. */
    vsp_cmcp_state *state;
    /** nanomsg socket number to publish messages. */
    int publish_socket;
    /** nanomsg socket number to receive messages. */
    int subscribe_socket;
    /** Reception thread. */
    pthread_t thread;
    /** Real time of next heartbeat. */
    double time_next_heartbeat;
    /** Message callback function. */
    void (*message_callback)(void*, vsp_cmcp_message*);
    /** Regular callback function. */
    void (*regular_callback)(void*);
    /** Message callback parameter. */
    void *callback_param;
};

/**
 * Send previously created message to the specified socket.
 * Blocks until message could be sent.
 * Returns non-zero and sets vsp_error_num() if failed.
 */
static int vsp_cmcp_node_send_message(int socket,
    vsp_cmcp_message *cmcp_message);

/** Event loop for message reception running in its own thread. */
static void *vsp_cmcp_node_run(void *param);

/** Check current time and send heartbeat if necessary. */
static void vsp_cmcp_node_heartbeat(vsp_cmcp_node *cmcp_node);

vsp_cmcp_node *vsp_cmcp_node_create(vsp_cmcp_node_type node_type,
    void (*message_callback)(void*, vsp_cmcp_message*),
    void (*regular_callback)(void*), void *callback_param)
{
    vsp_cmcp_node *cmcp_node;
    /* check parameters */
    VSP_ASSERT((node_type == VSP_CMCP_NODE_SERVER
        || node_type == VSP_CMCP_NODE_CLIENT) && message_callback != NULL);
    /* allocate memory */
    VSP_ALLOC(cmcp_node, vsp_cmcp_node);
    /* initialize struct data */
    cmcp_node->node_type = node_type;
    cmcp_node->state = vsp_cmcp_state_create(VSP_CMCP_NODE_UNINITIALIZED);
    /* in case of failure vsp_error_num() is already set */
    VSP_ASSERT(cmcp_node->state != NULL);
    /* generate node ID */
    vsp_cmcp_node_generate_id(cmcp_node);
    cmcp_node->publish_socket = -1;
    cmcp_node->subscribe_socket = -1;
    cmcp_node->time_next_heartbeat = vsp_time_real_double();
    cmcp_node->message_callback = message_callback;
    cmcp_node->regular_callback = regular_callback;
    cmcp_node->callback_param = callback_param;
    /* return struct pointer */
    return cmcp_node;
}

void vsp_cmcp_node_free(vsp_cmcp_node *cmcp_node)
{
    int ret;

    /* check parameter */
    VSP_ASSERT(cmcp_node != NULL);

    /* clean up socket connections */
    if (vsp_cmcp_state_get(cmcp_node->state) > VSP_CMCP_NODE_INITIALIZED) {
        /* worker thread is running, stop it */
        vsp_cmcp_node_stop(cmcp_node);
    }

    if (vsp_cmcp_state_get(cmcp_node->state) > VSP_CMCP_NODE_UNINITIALIZED) {
        /* close publish socket */
        ret = nn_close(cmcp_node->publish_socket);
        /* check for errors set by nanomsg */
        VSP_ASSERT(ret == 0);

        /* close subscribe socket */
        ret = nn_close(cmcp_node->subscribe_socket);
        /* check for errors set by nanomsg */
        VSP_ASSERT(ret == 0);
    }

    /* clean up state struct */
    vsp_cmcp_state_free(cmcp_node->state);

    /* free memory */
    VSP_FREE(cmcp_node);
}

void vsp_cmcp_node_generate_id(vsp_cmcp_node *cmcp_node)
{
    /* check parameter */
    VSP_ASSERT(cmcp_node != NULL);

    /* generate node ID that does not equal broadcast topic ID */
    if (cmcp_node->node_type == VSP_CMCP_NODE_SERVER) {
        /* server node: ID is even (first bit cleared) */
        do {
            cmcp_node->id = (uint16_t) (vsp_random_get() << 1);
        } while (cmcp_node->id == VSP_CMCP_SERVER_BROADCAST_TOPIC_ID);
    } else {
        /* client node: ID is odd (first bit set) */
        do {
            cmcp_node->id = (uint16_t) (vsp_random_get() | 1);
        } while (cmcp_node->id == VSP_CMCP_CLIENT_BROADCAST_TOPIC_ID);
    }
}

uint16_t vsp_cmcp_node_get_id(vsp_cmcp_node *cmcp_node)
{
    /* check parameter */
    VSP_ASSERT(cmcp_node != NULL);

    return cmcp_node->id;
}

int vsp_cmcp_node_connect(vsp_cmcp_node *cmcp_node,
    const char *publish_address, const char *subscribe_address)
{
    int ret;

    /* check parameters */
    VSP_ASSERT(cmcp_node != NULL && publish_address != NULL
        && subscribe_address != NULL);
    /* check if sockets are not yet initialized */
    VSP_CHECK(vsp_cmcp_state_get(cmcp_node->state)
        == VSP_CMCP_NODE_UNINITIALIZED, vsp_error_set_num(EALREADY); return -1);

    /* initialize and connect publish socket */
    cmcp_node->publish_socket = nn_socket(AF_SP, NN_PUB);
    /* vsp_error_num() is set by nn_socket() */
    VSP_CHECK(cmcp_node->publish_socket != -1, return -1);
    /* connect or bind socket */
    if (cmcp_node->node_type == VSP_CMCP_NODE_SERVER) {
        ret = nn_bind(cmcp_node->publish_socket, publish_address);
    } else {
        ret = nn_connect(cmcp_node->publish_socket, publish_address);
    }
    /* vsp_error_num() is set by nn_bind() or nn_connect() */
    VSP_CHECK(ret >= 0, nn_close(cmcp_node->publish_socket);
        return -1);

    /* initialize and connect subscribe socket */
    cmcp_node->subscribe_socket = nn_socket(AF_SP, NN_SUB);
    /* vsp_error_num() is set by nn_socket() */
    /* cleanup publish socket if failed */
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
    /* vsp_error_num() is set by nn_bind() or nn_connect() */
    /* cleanup both sockets if failed */
    VSP_CHECK(ret >= 0, nn_close(cmcp_node->publish_socket);
        nn_close(cmcp_node->subscribe_socket); return -1);
    /* set receive timeout */
    ret = nn_setsockopt(cmcp_node->subscribe_socket,
        NN_SOL_SOCKET, NN_RCVTIMEO, &VSP_CMCP_NODE_HEARTBEAT_TIME, sizeof(int));
    /* vsp_error_num() is set by nn_setsockopt() */
    /* cleanup both sockets if failed */
    VSP_CHECK(ret >= 0, nn_close(cmcp_node->publish_socket);
        nn_close(cmcp_node->subscribe_socket); return -1);

    /* set state */
    vsp_cmcp_state_set(cmcp_node->state, VSP_CMCP_NODE_INITIALIZED);

    /* subscribe to broadcast ID and node ID */
    if (cmcp_node->node_type == VSP_CMCP_NODE_SERVER) {
        vsp_cmcp_node_subscribe(cmcp_node, VSP_CMCP_SERVER_BROADCAST_TOPIC_ID);
    } else {
        vsp_cmcp_node_subscribe(cmcp_node, VSP_CMCP_CLIENT_BROADCAST_TOPIC_ID);
    }
    vsp_cmcp_node_subscribe(cmcp_node, cmcp_node->id);

    /* sockets successfully bound */
    return 0;
}

void vsp_cmcp_node_start(vsp_cmcp_node *cmcp_node)
{
    int ret;

    /* check parameter */
    VSP_ASSERT(cmcp_node != NULL);

    /* check if sockets are initialized and thread is not running */
    VSP_ASSERT(vsp_cmcp_state_get(cmcp_node->state)
        == VSP_CMCP_NODE_INITIALIZED);

    /* mark thread as starting */
    vsp_cmcp_state_set(cmcp_node->state, VSP_CMCP_NODE_STARTING);

    /* lock state mutex */
    vsp_cmcp_state_lock(cmcp_node->state);

    /* start reception thread */
    ret = pthread_create(&cmcp_node->thread, NULL,
        vsp_cmcp_node_run, cmcp_node);

    /* check for pthread errors */
    VSP_ASSERT(ret == 0);

    /* wait until thread is running */
    ret = vsp_cmcp_state_await_state(cmcp_node->state,
        VSP_CMCP_NODE_RUNNING, NULL);
    /* unlock state mutex */
    vsp_cmcp_state_unlock(cmcp_node->state);
    /* check for condition waiting errors */
    VSP_ASSERT(ret == 0);
}

void vsp_cmcp_node_stop(vsp_cmcp_node *cmcp_node)
{
    int ret;

    /* check parameter */
    VSP_ASSERT(cmcp_node != NULL);

    /* check if thread is running */
    VSP_ASSERT(vsp_cmcp_state_get(cmcp_node->state) == VSP_CMCP_NODE_RUNNING);

    /* stop reception thread */
    vsp_cmcp_state_set(cmcp_node->state, VSP_CMCP_NODE_STOPPING);

    /* wait for thread to join */
    ret = pthread_join(cmcp_node->thread, NULL);

    /* check if thread has successfully stopped */
    VSP_ASSERT(ret == 0);

    /* check state */
    VSP_ASSERT(vsp_cmcp_state_get(cmcp_node->state)
        == VSP_CMCP_NODE_INITIALIZED);
}

int vsp_cmcp_node_send_message(int socket, vsp_cmcp_message *cmcp_message)
{
    int ret;
    int data_length;
    void *data_buffer;

    /* get message data length */
    data_length = vsp_cmcp_message_get_data_length(cmcp_message);
    /* check for errors */
    VSP_ASSERT(data_length > 0);

    /* allocate zero-copy message buffer */
    data_buffer = nn_allocmsg(data_length, 0);
    /* check for errors */
    VSP_ASSERT(data_buffer != NULL);

    /* write message data to buffer */
    vsp_cmcp_message_get_data(cmcp_message, data_buffer);

    /* actually send message to socket */
    ret = nn_send(socket, &data_buffer, NN_MSG, 0);
    /* vsp_error_num() is set by nn_send() */
    VSP_CHECK(ret >= 0, goto error_exit);

    /* success */
    return 0;

    error_exit:
        /* cleanup */
        ret = nn_freemsg(data_buffer);
        VSP_ASSERT(ret == 0);
        /* vsp_error_num() is already set */
        return -1;
}

int vsp_cmcp_node_create_send_message(vsp_cmcp_node *cmcp_node,
    vsp_cmcp_message_type message_type,
    uint16_t topic_id, uint16_t sender_id, uint16_t command_id,
    vsp_cmcp_datalist *cmcp_datalist)
{
    int ret;
    int success;
    vsp_cmcp_message *cmcp_message;

    /* clear state, no errors yet */
    success = 0;

    /* check parameters; data list may be NULL */
    VSP_ASSERT(cmcp_node != NULL);

    /* generate message */
    cmcp_message = vsp_cmcp_message_create(message_type,
        topic_id, sender_id, command_id, cmcp_datalist);
    /* check for errors */
    VSP_ASSERT(cmcp_message != NULL);

    /* check if sockets are initialized */
    VSP_ASSERT(vsp_cmcp_state_get(cmcp_node->state)
        >= VSP_CMCP_NODE_INITIALIZED);
    /* send message */
    ret = vsp_cmcp_node_send_message(cmcp_node->publish_socket, cmcp_message);
    /* vsp_error_num() is set by nn_send() */
    /* check for error, but do not directly return to avoid memory leaks */
    VSP_CHECK(ret == 0, success = -1);

    /* free message */
    vsp_cmcp_message_free(cmcp_message);

    /* vsp_error_num() is already set */
    return success;
}

void vsp_cmcp_node_subscribe(vsp_cmcp_node *cmcp_node, uint16_t topic_id)
{
    int ret;

    /* check parameter */
    VSP_ASSERT(cmcp_node != NULL);

    /* check if sockets are initialized */
    VSP_ASSERT(vsp_cmcp_state_get(cmcp_node->state)
        >= VSP_CMCP_NODE_INITIALIZED);

    /* subscribe to topic ID */
    ret = nn_setsockopt(cmcp_node->subscribe_socket, NN_SUB, NN_SUB_SUBSCRIBE,
        &topic_id, sizeof(topic_id));
    /* check for errors */
    VSP_ASSERT(ret == 0);
}

void vsp_cmcp_node_unsubscribe(vsp_cmcp_node *cmcp_node, uint16_t topic_id)
{
    int ret;

    /* check parameter */
    VSP_ASSERT(cmcp_node != NULL);

    /* check if sockets are initialized */
    VSP_ASSERT(vsp_cmcp_state_get(cmcp_node->state)
        >= VSP_CMCP_NODE_INITIALIZED);

    /* unsubscribe from topic ID */
    ret = nn_setsockopt(cmcp_node->subscribe_socket, NN_SUB, NN_SUB_UNSUBSCRIBE,
        &topic_id, sizeof(topic_id));
    /* check for errors */
    VSP_ASSERT(ret == 0);
}

void *vsp_cmcp_node_run(void *param)
{
    vsp_cmcp_node *cmcp_node;
    int ret;
    int data_length;
    void *message_buffer;
    vsp_cmcp_message *cmcp_message;
    uint16_t sender_id;

    /* check parameter */
    VSP_ASSERT(param != NULL);

    /* initialize local variables */
    cmcp_node = (vsp_cmcp_node*) param;
    message_buffer = NULL;
    cmcp_message = NULL;

    /* check if sockets are initialized and thread was started */
    VSP_ASSERT(vsp_cmcp_state_get(cmcp_node->state) == VSP_CMCP_NODE_STARTING);

    /* mark thread as running */
    vsp_cmcp_state_set(cmcp_node->state, VSP_CMCP_NODE_RUNNING);

    /* reception loop */
    while (vsp_cmcp_state_get(cmcp_node->state) == VSP_CMCP_NODE_RUNNING) {
        /* send heartbeat */
        vsp_cmcp_node_heartbeat(cmcp_node);

        /* invoke regular callback function */
        cmcp_node->regular_callback(cmcp_node->callback_param);

        /* try to receive message */
        data_length = nn_recv(cmcp_node->subscribe_socket,
            &message_buffer, NN_MSG, 0);
        /* check error: in case of failure just retry */
        VSP_CHECK(data_length > 0, goto cleanup);
        /* parse message data */
        cmcp_message = vsp_cmcp_message_create_parse(data_length,
            message_buffer);
        /* check error: in case of failure clean up and retry */
        VSP_CHECK(cmcp_message != NULL, goto cleanup);

        /* filter out invalid messages; check if message has valid sender */
        sender_id = vsp_cmcp_message_get_sender_id(cmcp_message);
        VSP_CHECK(sender_id != VSP_CMCP_SERVER_BROADCAST_TOPIC_ID
            && sender_id != VSP_CMCP_CLIENT_BROADCAST_TOPIC_ID, goto cleanup);

        /* message successfully received; invoke callback function */
        cmcp_node->message_callback(cmcp_node->callback_param, cmcp_message);

        cleanup:
            /* clean up */
            if (cmcp_message != NULL) {
                vsp_cmcp_message_free(cmcp_message);
                cmcp_message = NULL;
            }
            if (message_buffer != NULL) {
                ret = nn_freemsg(message_buffer);
                VSP_ASSERT(ret == 0);
                message_buffer = NULL;
            }
            continue;
    }

    /* check if thread was requested to stop */
    VSP_ASSERT(vsp_cmcp_state_get(cmcp_node->state) == VSP_CMCP_NODE_STOPPING);

    /* signal that thread has stopped */
    vsp_cmcp_state_set(cmcp_node->state, VSP_CMCP_NODE_INITIALIZED);

    /* success */
    return (void*) 0;
}

void vsp_cmcp_node_heartbeat(vsp_cmcp_node *cmcp_node)
{
    double time_now;
    int ret;
    uint16_t topic_id, command_id;

    time_now = vsp_time_real_double();
    if (time_now < cmcp_node->time_next_heartbeat) {
        /* not sending heartbeat yet; nothing to fail, hence success */
        return;
    }

    /* update time of next heartbeat */
    cmcp_node->time_next_heartbeat =
        time_now + (VSP_CMCP_NODE_HEARTBEAT_TIME / 1000.0);

    /* send server heartbeat to all clients, client heartbeat to all servers */
    if (cmcp_node->node_type == VSP_CMCP_NODE_SERVER) {
        topic_id = VSP_CMCP_CLIENT_BROADCAST_TOPIC_ID;
        command_id = VSP_CMCP_COMMAND_SERVER_HEARTBEAT;
    } else {
        topic_id = VSP_CMCP_SERVER_BROADCAST_TOPIC_ID;
        command_id = VSP_CMCP_COMMAND_CLIENT_HEARTBEAT;
    }
    ret = vsp_cmcp_node_create_send_message(cmcp_node,
        VSP_CMCP_MESSAGE_TYPE_CONTROL,
        topic_id, cmcp_node->id, command_id, NULL);
    /* check for errors */
    VSP_ASSERT(ret == 0);
}
