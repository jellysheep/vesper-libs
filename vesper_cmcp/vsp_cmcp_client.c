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
#include "vsp_cmcp_state.h"

#include <vesper_util/vsp_error.h>
#include <vesper_util/vsp_random.h>
#include <vesper_util/vsp_time.h>
#include <vesper_util/vsp_util.h>

/** vsp_cmcp_node finite state machine flag. */
typedef enum {
    /** Client is not connected to server. */
    VSP_CMCP_CLIENT_DISCONNECTED,
    /** Client is trying to connect to server. */
    VSP_CMCP_CLIENT_TRYING_TO_CONNECT,
    /** Client received heartbeat signal from server,
     * so reception socket is working. */
    VSP_CMCP_CLIENT_HEARTBEAT_RECEIVED,
    /** Connection was established successfully. */
    VSP_CMCP_CLIENT_CONNECTED
} vsp_cmcp_client_state;

/** State and other data used for network connection. */
struct vsp_cmcp_client {
    /** Basic node and finite-state machine data. */
    vsp_cmcp_node *cmcp_node;
    /** ID identifying this client in the network. */
    uint16_t id;
    /** Node ID of the connected server. */
    uint16_t server_id;
    /** Client finite state machine flag, not related to node state. */
    vsp_cmcp_state *state;
    /** Randomly generated nonce used for temporary node indentification. */
    uint64_t nonce;
    /** The time when the connection to server times out. */
    struct timespec time_connection_timeout;
};

/** Connect to server and establish connection using handshake.
 * Connection establishment is specified by CMCP protocol.
 * Returns non-zero and sets vsp_error_num() if failed. */
static int vsp_cmcp_client_establish_connection(vsp_cmcp_client *cmcp_client);

/** Send an announcement message to server.
 * Returns non-zero and sets vsp_error_num() if failed. */
static int vsp_cmcp_client_send_announcement(vsp_cmcp_client *cmcp_client);

/** Regular callback function invoked by cmcp_node.
 * This function will be called regularly. */
static void vsp_cmcp_client_regular_callback(void *param);

/** Message callback function invoked by cmcp_node.
 * This function will be called for every received message. */
static void vsp_cmcp_client_message_callback(void *param,
    vsp_cmcp_message *cmcp_message);

/** Handle an internal CMCP control message. */
void vsp_cmcp_client_handle_control_message(vsp_cmcp_client *cmcp_client,
    uint16_t sender_id, uint16_t command_id, vsp_cmcp_datalist *cmcp_datalist);

vsp_cmcp_client *vsp_cmcp_client_create(void)
{
    vsp_cmcp_client *cmcp_client;
    /* allocate memory */
    VSP_ALLOC(cmcp_client, vsp_cmcp_client);
    /* initialize base type */
    cmcp_client->cmcp_node = vsp_cmcp_node_create(VSP_CMCP_NODE_CLIENT,
        vsp_cmcp_client_message_callback, vsp_cmcp_client_regular_callback,
        cmcp_client);
    /* vsp_error_num() is set by vsp_cmcp_node_create() */
    VSP_CHECK(cmcp_client->cmcp_node != NULL,
        VSP_FREE(cmcp_client); return NULL);
    /* get node ID */
    cmcp_client->id = vsp_cmcp_node_get_id(cmcp_client->cmcp_node);
    /* initialize state struct */
    cmcp_client->state = vsp_cmcp_state_create(VSP_CMCP_CLIENT_DISCONNECTED);
    /* vsp_error_num() is set by vsp_cmcp_state_create() */
    VSP_CHECK(cmcp_client->state != NULL, VSP_FREE(cmcp_client); return NULL);
    /* return struct pointer */
    return cmcp_client;
}

void vsp_cmcp_client_free(vsp_cmcp_client *cmcp_client)
{
    int ret;

    /* check parameter */
    VSP_CHECK(cmcp_client != NULL, return);

    /* check if connected */
    if (vsp_cmcp_state_get(cmcp_client->state) == VSP_CMCP_CLIENT_CONNECTED) {
        /* disconnect client from server */
        ret = vsp_cmcp_node_create_send_message(cmcp_client->cmcp_node,
            VSP_CMCP_MESSAGE_TYPE_CONTROL, cmcp_client->server_id,
            cmcp_client->id, VSP_CMCP_COMMAND_CLIENT_DISCONNECT, NULL);
        /* check for errors */
        VSP_CHECK(ret == 0, /* failures are silently ignored */);
    }

    /* free node base type */
    vsp_cmcp_node_free(cmcp_client->cmcp_node);

    /* free state struct */
    vsp_cmcp_state_free(cmcp_client->state);

    /* free memory */
    VSP_FREE(cmcp_client);
}

int vsp_cmcp_client_connect(vsp_cmcp_client *cmcp_client,
    const char *publish_address, const char *subscribe_address)
{
    int ret;

    /* check parameters */
    VSP_CHECK(cmcp_client != NULL && publish_address != NULL
        && subscribe_address != NULL, vsp_error_set_num(EINVAL); return -1);

    /* connect sockets; the node state is checked by this function */
    ret = vsp_cmcp_node_connect(cmcp_client->cmcp_node,
        publish_address, subscribe_address);
    /* check for errors */
    VSP_CHECK(ret == 0, return -1);

    /* start worker thread */
    vsp_cmcp_node_start(cmcp_client->cmcp_node);

    /* establish connection */
    ret = vsp_cmcp_client_establish_connection(cmcp_client);
    /* check for errors */
    VSP_CHECK(ret == 0, vsp_error_set_num(ENOTCONN); return -1);

    /* successfully connected and waiting for messages */
    return 0;
}

int vsp_cmcp_client_send(vsp_cmcp_client *cmcp_client,
    uint16_t command_id, vsp_cmcp_datalist *cmcp_datalist)
{
    int ret;

    /* check parameters */
    VSP_CHECK(cmcp_client != NULL, vsp_error_set_num(EINVAL); return -1);

    /* check connection state */
    VSP_CHECK(vsp_cmcp_state_get(cmcp_client->state)
        == VSP_CMCP_CLIENT_CONNECTED, vsp_error_set_num(ENOTCONN); return -1);

    /* send message */
    ret = vsp_cmcp_node_create_send_message(cmcp_client->cmcp_node,
        VSP_CMCP_MESSAGE_TYPE_DATA, cmcp_client->id, cmcp_client->id,
        command_id, cmcp_datalist);

    /* vsp_error_num() is set by vsp_cmcp_node_create_send_message() */
    VSP_CHECK(ret == 0, return -1);

    /* message sent successfully */
    return 0;
}

int vsp_cmcp_client_establish_connection(vsp_cmcp_client *cmcp_client)
{
    int ret;
    int success;
    int state;
    struct timespec time_connection_timeout;

    /* initialize local variables */
    success = 0;

    /* start measuring time for heartbeat timeout */
    vsp_time_real_timespec_from_now(&time_connection_timeout,
        VSP_CMCP_NODE_CONNECTION_TIMEOUT);

    state = vsp_cmcp_state_get(cmcp_client->state);

    /* wait until connected or waiting timed out */
    do {
        if (state == VSP_CMCP_CLIENT_DISCONNECTED) {
            /* enable connecting to server */
            vsp_cmcp_state_set(cmcp_client->state,
                VSP_CMCP_CLIENT_TRYING_TO_CONNECT);
        }
        vsp_cmcp_state_lock(cmcp_client->state);
        ret = vsp_cmcp_state_wait(cmcp_client->state, &time_connection_timeout);
        state = vsp_cmcp_state_get(cmcp_client->state);
    } while (ret == 0 && state != VSP_CMCP_CLIENT_CONNECTED);

    /* check if connected */
    VSP_CHECK(state == VSP_CMCP_CLIENT_CONNECTED,
        vsp_error_set_num(ENOTCONN); success = -1);

    return success;
}

void vsp_cmcp_client_regular_callback(void *param)
{
    vsp_cmcp_client *cmcp_client;

    /* check parameters; failures are silently ignored */
    VSP_CHECK(param != NULL, return);

    cmcp_client = (vsp_cmcp_client*) param;

    if (vsp_cmcp_state_get(cmcp_client->state) == VSP_CMCP_CLIENT_CONNECTED
        && vsp_time_real_timespec_passed(
        &cmcp_client->time_connection_timeout) == 0) {
        /* connection to server timed out */
        vsp_cmcp_state_set(cmcp_client->state,
            VSP_CMCP_CLIENT_DISCONNECTED);
        /* connection establishment will not be automatically retried */
        /* TODO: inform about lost connection through API */
    }
}

void vsp_cmcp_client_message_callback(void *param,
    vsp_cmcp_message *cmcp_message)
{
    vsp_cmcp_client *cmcp_client;
    vsp_cmcp_datalist *cmcp_datalist;
    uint16_t topic_id, sender_id, command_id;
    int state;

    /* check parameters; failures are silently ignored */
    VSP_CHECK(param != NULL && cmcp_message != NULL, return);

    cmcp_client = (vsp_cmcp_client*) param;

    /* get message IDs */
    topic_id = vsp_cmcp_message_get_topic_id(cmcp_message);
    sender_id = vsp_cmcp_message_get_sender_id(cmcp_message);
    command_id = vsp_cmcp_message_get_command_id(cmcp_message);

    /* only receive server messages */
    VSP_CHECK((sender_id & 1) == 0, return);

    /* get data list */
    cmcp_datalist = vsp_cmcp_message_get_datalist(cmcp_message);
    /* check data list; failures are silently ignored */
    VSP_CHECK(cmcp_datalist != NULL, return);

    /* get current state */
    state = vsp_cmcp_state_get(cmcp_client->state);

    /* check if message is from connected server */
    if (state == VSP_CMCP_CLIENT_CONNECTED
        && sender_id == cmcp_client->server_id) {
        /* reset timeout time */
        vsp_time_real_timespec_from_now(&cmcp_client->time_connection_timeout,
            VSP_CMCP_NODE_CONNECTION_TIMEOUT);
    }

    /* check if internal control message received */
    if (vsp_cmcp_message_get_type(cmcp_message)
        == VSP_CMCP_MESSAGE_TYPE_CONTROL) {
        /* handle control message */
        vsp_cmcp_client_handle_control_message(cmcp_client, sender_id,
            command_id, cmcp_datalist);
    } else {
        /* check if message is directed to this node */
        VSP_CHECK(topic_id == cmcp_client->id, return);
        /* handle data message */
        /* ... */
    }
}

void vsp_cmcp_client_handle_control_message(vsp_cmcp_client *cmcp_client,
    uint16_t sender_id, uint16_t command_id, vsp_cmcp_datalist *cmcp_datalist)
{
    int ret, state;
    uint64_t *client_nonce;

    /* get current state */
    state = vsp_cmcp_state_get(cmcp_client->state);

    /* process control message */
    if (state == VSP_CMCP_CLIENT_TRYING_TO_CONNECT) {
        /* check if server heartbeat received */
        VSP_CHECK(command_id == VSP_CMCP_COMMAND_SERVER_HEARTBEAT, return);
        /* store server node ID */
        cmcp_client->server_id = sender_id;
        /* server heartbeat received */
        vsp_cmcp_state_set(cmcp_client->state,
            VSP_CMCP_CLIENT_HEARTBEAT_RECEIVED);
        /* send announcement message */
        ret = vsp_cmcp_client_send_announcement(cmcp_client);
        /* check for errors; failures are silently ignored */
        VSP_CHECK(ret == 0, return);
    } else if (state == VSP_CMCP_CLIENT_HEARTBEAT_RECEIVED) {
        /* check if (negative) acknowledge received from connected server */
        VSP_CHECK(sender_id == cmcp_client->server_id
            && (command_id == VSP_CMCP_COMMAND_SERVER_ACK_CLIENT
            || command_id == VSP_CMCP_COMMAND_SERVER_NACK_CLIENT), return);
        /* get client nonce */
        client_nonce = vsp_cmcp_datalist_get_data_item(cmcp_datalist,
            VSP_CMCP_PARAMETER_NONCE, sizeof(uint64_t));
        /* check data list item (nonce); failures are silently ignored */
        VSP_CHECK(client_nonce != NULL, return);
        /* ignore message if nonce does not match the nonce of this node */
        VSP_CHECK(*client_nonce == cmcp_client->nonce, return);

        if (command_id == VSP_CMCP_COMMAND_SERVER_ACK_CLIENT) {
            /* acknowledge received, connected */
            vsp_cmcp_state_set(cmcp_client->state, VSP_CMCP_CLIENT_CONNECTED);
            /* initialize timeout time */
            vsp_time_real_timespec_from_now(
                &cmcp_client->time_connection_timeout,
                VSP_CMCP_NODE_CONNECTION_TIMEOUT);
        } else if (command_id == VSP_CMCP_COMMAND_SERVER_NACK_CLIENT) {
            /* negative acknowledge received, rejected */
            vsp_cmcp_state_set(cmcp_client->state,
                VSP_CMCP_CLIENT_DISCONNECTED);
            /* client ID is already registered to server; regenerate node ID */
            vsp_cmcp_node_generate_id(cmcp_client->cmcp_node);
            cmcp_client->id = vsp_cmcp_node_get_id(cmcp_client->cmcp_node);
            /* connection establishment will be automatically retried when next
             * server heartbeat signal is received */
        }
    }
}

int vsp_cmcp_client_send_announcement(vsp_cmcp_client *cmcp_client)
{
    int ret;
    int success;
    vsp_cmcp_datalist *cmcp_datalist;

    /* initialize local variables */
    success = 0;
    cmcp_datalist = NULL;

    /* create identification nonce */
    cmcp_client->nonce = vsp_random_get();

    /* create announcement command message */
    cmcp_datalist = vsp_cmcp_datalist_create();
    /* check for errors */
    VSP_ASSERT(cmcp_datalist != NULL);
    /* add nonce parameter as a data list item */
    ret = vsp_cmcp_datalist_add_item(cmcp_datalist, VSP_CMCP_PARAMETER_NONCE,
        sizeof(uint64_t), &cmcp_client->nonce);
    /* vsp_error_num() is set by vsp_cmcp_datalist_add_item() */
    VSP_CHECK(ret == 0, goto error_exit);
    /* send message */
    ret = vsp_cmcp_node_create_send_message(cmcp_client->cmcp_node,
        VSP_CMCP_MESSAGE_TYPE_CONTROL, cmcp_client->server_id, cmcp_client->id,
        VSP_CMCP_COMMAND_CLIENT_ANNOUNCE, cmcp_datalist);
    /* vsp_error_num() is set by vsp_cmcp_node_create_send_message() */
    VSP_CHECK(ret == 0, goto error_exit);

    /* success: clean up and exit */
    goto cleanup_exit;

    error_exit:
        /* vsp_error_num() is already set */
        success = -1;
        /* fall through to cleanup_exit */

    cleanup_exit:
        /* clean up */
        if (cmcp_datalist != NULL) {
            vsp_cmcp_datalist_free(cmcp_datalist);
            cmcp_datalist = NULL;
        }
        return success;
}
