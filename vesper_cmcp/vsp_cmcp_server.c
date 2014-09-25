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
#include <vesper_util/vsp_time.h>
#include <vesper_util/vsp_util.h>

#define VSP_CMCP_SERVER_MAX_PEERS 16

/** Data of a network node connected to server. */
struct vsp_cmcp_server_peer {
    /** The time when the connection to this peer times out. */
    struct timespec time_connection_timeout;
};

/** Define type vsp_cmcp_server_peer to avoid 'struct' keyword. */
typedef struct vsp_cmcp_server_peer vsp_cmcp_server_peer;

/** State and other data used for network connection. */
struct vsp_cmcp_server {
    /** Basic node and finite-state machine data. */
    vsp_cmcp_node *cmcp_node;
    /** ID identifying this server in the network. */
    uint16_t id;
    /** Number of registered client peers. */
    uint16_t client_count;
    /** Client peer IDs. */
    uint16_t client_ids[VSP_CMCP_SERVER_MAX_PEERS];
    /** Client peer data. */
    vsp_cmcp_server_peer *client_data[VSP_CMCP_SERVER_MAX_PEERS];
    /** Callback function parameter. */
    void *callback_param;
    /** Client announcement callback function. */
    vsp_cmcp_server_announcement_cb announcement_cb;
    /** Client disconnection callback function. */
    vsp_cmcp_server_disconnect_cb disconnect_cb;
    /** Message callback function. */
    vsp_cmcp_server_message_cb message_cb;
};

/** Regular callback function invoked by cmcp_node.
 * This function will be called regularly. */
static void vsp_cmcp_server_regular_callback(void *param);

/** Message callback function invoked by cmcp_node.
 * This function will be called for every received message. */
static void vsp_cmcp_server_message_callback(void *param,
    vsp_cmcp_message *cmcp_message);

/** Handle an internal CMCP control message. */
void vsp_cmcp_server_handle_control_message(vsp_cmcp_server *cmcp_server,
    uint16_t sender_id, uint16_t command_id, vsp_cmcp_datalist *cmcp_datalist);

/** Try to register newly connected client and send (negative) acknowledge. */
static void vsp_cmcp_server_register_client(vsp_cmcp_server *cmcp_server,
    uint16_t client_id, uint64_t client_nonce);

/** Search for client peer ID in registered peers.
 * Returns client peer index if found and -1 else. */
static int vsp_cmcp_server_find_client(
    vsp_cmcp_server *cmcp_server, uint16_t client_id);

/** Deregister the client with the specified ID.
 * Future messages of this ID will be ignored (except announcements). */
static void vsp_cmcp_server_deregister_client(vsp_cmcp_server *cmcp_server,
    uint16_t client_id);

vsp_cmcp_server *vsp_cmcp_server_create(void)
{
    vsp_cmcp_server *cmcp_server;
    /* allocate memory */
    VSP_ALLOC(cmcp_server, vsp_cmcp_server);
    /* initialize base type */
    cmcp_server->cmcp_node = vsp_cmcp_node_create(VSP_CMCP_NODE_SERVER,
        vsp_cmcp_server_message_callback,
        vsp_cmcp_server_regular_callback, cmcp_server);
    /* vsp_error_num() is set by vsp_cmcp_node_create() */
    VSP_CHECK(cmcp_server->cmcp_node != NULL,
        VSP_FREE(cmcp_server); return NULL);
    /* get node ID */
    cmcp_server->id = vsp_cmcp_node_get_id(cmcp_server->cmcp_node);
    /* no client peers registered yet */
    cmcp_server->client_count = 0;
    /* initialize callback parameter and functions */
    cmcp_server->callback_param = NULL;
    cmcp_server->announcement_cb = NULL;
    cmcp_server->disconnect_cb = NULL;
    cmcp_server->message_cb = NULL;
    /* return struct pointer */
    return cmcp_server;
}

void vsp_cmcp_server_free(vsp_cmcp_server *cmcp_server)
{
    /* check parameter */
    VSP_CHECK(cmcp_server != NULL, return);

    /* free node base type */
    vsp_cmcp_node_free(cmcp_server->cmcp_node);

    /* clean up registered client peer data */
    for (; cmcp_server->client_count > 0; --cmcp_server->client_count) {
        /* when n elements are left, the (n-1)'th element is freed */
        VSP_FREE(cmcp_server->client_data[cmcp_server->client_count - 1]);
    }

    /* free memory */
    VSP_FREE(cmcp_server);
}

void vsp_cmcp_server_set_callback_param(vsp_cmcp_server *cmcp_server,
    void *callback_param)
{
    /* check parameters */
    VSP_CHECK(cmcp_server != NULL, return);

    /* set callback param */
    cmcp_server->callback_param = callback_param;
}

void vsp_cmcp_server_set_announcement_cb(vsp_cmcp_server *cmcp_server,
    vsp_cmcp_server_announcement_cb announcement_cb)
{
    /* check parameters */
    VSP_CHECK(cmcp_server != NULL, return);

    /* set callback function */
    cmcp_server->announcement_cb = announcement_cb;
}

void vsp_cmcp_server_set_disconnect_cb(vsp_cmcp_server *cmcp_server,
    vsp_cmcp_server_disconnect_cb disconnect_cb)
{
    /* check parameters */
    VSP_CHECK(cmcp_server != NULL, return);

    /* set callback function */
    cmcp_server->disconnect_cb = disconnect_cb;
}

void vsp_cmcp_server_set_message_cb(vsp_cmcp_server *cmcp_server,
    vsp_cmcp_server_message_cb message_cb)
{
    /* check parameters */
    VSP_CHECK(cmcp_server != NULL, return);

    /* set callback function */
    cmcp_server->message_cb = message_cb;
}

int vsp_cmcp_server_bind(vsp_cmcp_server *cmcp_server,
    const char *publish_address, const char *subscribe_address)
{
    int ret;

    /* check parameters */
    VSP_CHECK(cmcp_server != NULL && publish_address != NULL
        && subscribe_address != NULL, vsp_error_set_num(EINVAL); return -1);

    /* bind sockets */
    ret = vsp_cmcp_node_connect(cmcp_server->cmcp_node,
        publish_address, subscribe_address);
    /* vsp_error_num() is set by vsp_cmcp_node_connect() */
    VSP_CHECK(ret == 0, return -1);

    /* start worker thread */
    vsp_cmcp_node_start(cmcp_server->cmcp_node);

    /* sockets successfully bound */
    return 0;
}

void vsp_cmcp_server_regular_callback(void *param)
{
    vsp_cmcp_server *cmcp_server;
    int index;

    /* check parameters; failures are silently ignored */
    VSP_CHECK(param != NULL, return);

    cmcp_server = (vsp_cmcp_server*) param;

    /* check if client peer connections have timed out */
    for (index = 0; index < cmcp_server->client_count; ++index) {
        if (vsp_time_real_timespec_passed(
            &cmcp_server->client_data[index]->time_connection_timeout) == 0) {
            /* client peer connection timed out */
            vsp_cmcp_server_deregister_client(cmcp_server,
                cmcp_server->client_ids[index]);
        }
    }
}

void vsp_cmcp_server_message_callback(void *param,
    vsp_cmcp_message *cmcp_message)
{
    vsp_cmcp_server *cmcp_server;
    vsp_cmcp_datalist *cmcp_datalist;
    uint16_t topic_id, sender_id, command_id;
    int client_index;

    /* check parameters; failures are silently ignored */
    VSP_CHECK(param != NULL && cmcp_message != NULL, return);

    cmcp_server = (vsp_cmcp_server*) param;

    /* get message IDs */
    topic_id = vsp_cmcp_message_get_topic_id(cmcp_message);
    sender_id = vsp_cmcp_message_get_sender_id(cmcp_message);
    command_id = vsp_cmcp_message_get_command_id(cmcp_message);

    /* check if message is received from a client,
     * as server-to-server messages are not supported yet */
    VSP_CHECK((sender_id & 1) == 1, return);

    /* get data list */
    cmcp_datalist = vsp_cmcp_message_get_datalist(cmcp_message);
    /* check data list; failures are silently ignored */
    VSP_CHECK(cmcp_datalist != NULL, return);

    /* try to find client in registered peers */
    client_index = vsp_cmcp_server_find_client(cmcp_server, sender_id);
    if (client_index >= 0) {
        /* reset client peer timeout time */
        vsp_time_real_timespec_from_now(
            &cmcp_server->client_data[client_index]->time_connection_timeout,
            VSP_CMCP_NODE_CONNECTION_TIMEOUT);
    }

    /* check if internal control message received */
    if (vsp_cmcp_message_get_type(cmcp_message)
        == VSP_CMCP_MESSAGE_TYPE_CONTROL) {
        /* check if message is broadcasted or directed to this node */
        VSP_CHECK(topic_id == VSP_CMCP_SERVER_BROADCAST_TOPIC_ID
            || topic_id == cmcp_server->id, return);
        /* handle control message */
        vsp_cmcp_server_handle_control_message(cmcp_server, sender_id,
            command_id, cmcp_datalist);
    } else {
        /* check if message is broadcasted or directed to a client topic */
        VSP_CHECK(topic_id == VSP_CMCP_SERVER_BROADCAST_TOPIC_ID
            || (topic_id & 1) == 1, return);
        /* check if client is registered; server-to-server messages are not
         * supported yet */
        VSP_CHECK(client_index >= 0, return);
        /* handle data message */
        if (cmcp_server->message_cb != NULL) {
            /* callback function registered; invoke it */
            cmcp_server->message_cb(cmcp_server->callback_param, sender_id,
                command_id, cmcp_datalist);
        }
    }
}

void vsp_cmcp_server_handle_control_message(vsp_cmcp_server *cmcp_server,
    uint16_t sender_id, uint16_t command_id, vsp_cmcp_datalist *cmcp_datalist)
{
    /* handle only client commands */
    VSP_CHECK((sender_id & 1) == 1, return);
    if (command_id == VSP_CMCP_COMMAND_CLIENT_ANNOUNCE) {
        /* client announcement received */
        uint64_t *client_nonce;
        /* get client nonce */
        client_nonce = vsp_cmcp_datalist_get_data_item(cmcp_datalist,
            VSP_CMCP_PARAMETER_NONCE, sizeof(uint64_t));
        /* check data list item (nonce); failures are silently ignored */
        VSP_CHECK(client_nonce != NULL, return);
        /* try to register client peer */
        vsp_cmcp_server_register_client(cmcp_server, sender_id, *client_nonce);
    } else if (command_id == VSP_CMCP_COMMAND_CLIENT_DISCONNECT) {
        /* client disconnection received; deregister client */
        vsp_cmcp_server_deregister_client(cmcp_server, sender_id);
    }
}

void vsp_cmcp_server_register_client(vsp_cmcp_server *cmcp_server,
    uint16_t client_id, uint64_t client_nonce)
{
    vsp_cmcp_datalist *cmcp_datalist;
    int ret, success;

    /* create data list containing client nonce */
    cmcp_datalist = vsp_cmcp_datalist_create();
    /* check for errors */
    VSP_ASSERT(cmcp_datalist != NULL);
    /* add nonce as data list item */
    ret = vsp_cmcp_datalist_add_item(cmcp_datalist, VSP_CMCP_PARAMETER_NONCE,
        sizeof(client_nonce), &client_nonce);
    /* check for errors */
    VSP_ASSERT(ret == 0);

    /* try to register client peer ID */
    success = 0;
    if (vsp_cmcp_server_find_client(cmcp_server, client_id) == 0) {
        /* client peer ID already registered */
        success = -1;
    } else if (cmcp_server->client_count >= VSP_CMCP_SERVER_MAX_PEERS) {
        /* maximum number of peers already registered */
        success = -1;
    } else {
        if (cmcp_server->announcement_cb == NULL) {
            /* no callback function registered; reject client */
            success = -1;
        } else if (cmcp_server->announcement_cb(
            cmcp_server->callback_param, client_id) != 0) {
            /* callback function returns non-zero; reject client */
            success = -1;
        } else {
            /* register client peer ID */
            vsp_cmcp_server_peer *client;
            /* create client data struct */
            VSP_ALLOC(client, vsp_cmcp_server_peer);
            /* initialize struct data */
            vsp_time_real_timespec_from_now(&client->time_connection_timeout,
                VSP_CMCP_NODE_CONNECTION_TIMEOUT);
            /* store client peer ID */
            cmcp_server->client_ids[cmcp_server->client_count] = client_id;
            /* store client peer data */
            cmcp_server->client_data[cmcp_server->client_count] = client;
            /* increment client peer count */
            ++cmcp_server->client_count;
            /* success */
            success = 0;
        }
    }
    if (success == 0) {
        /* subscribe to messages from this client peer */
        vsp_cmcp_node_subscribe(cmcp_server->cmcp_node, client_id);
        /* new client peer ID registered, send acknowledge message */
        ret = vsp_cmcp_node_create_send_message(cmcp_server->cmcp_node,
            VSP_CMCP_MESSAGE_TYPE_CONTROL,
            client_id, cmcp_server->id,
            VSP_CMCP_COMMAND_SERVER_ACK_CLIENT, cmcp_datalist);
        /* check for errors */
        VSP_CHECK(ret == 0, /* failures are silently ignored */);
    } else {
        /* new client peer ID rejected, send negative acknowledge message */
        ret = vsp_cmcp_node_create_send_message(cmcp_server->cmcp_node,
            VSP_CMCP_MESSAGE_TYPE_CONTROL,
            client_id, cmcp_server->id,
            VSP_CMCP_COMMAND_SERVER_NACK_CLIENT, cmcp_datalist);
        /* check for errors */
        VSP_CHECK(ret == 0, /* failures are silently ignored */);
    }

    /* cleanup */
    vsp_cmcp_datalist_free(cmcp_datalist);
}

int vsp_cmcp_server_find_client(vsp_cmcp_server *cmcp_server,
    uint16_t client_id)
{
    int index;

    for (index = 0; index < cmcp_server->client_count; ++index) {
        if (cmcp_server->client_ids[index] == client_id) {
            /* client peer ID found */
            return index;
        }
    }

    /* client peer ID not found */
    return -1;
}

void vsp_cmcp_server_deregister_client(vsp_cmcp_server *cmcp_server,
    uint16_t client_id)
{
    int index;

    /* find client ID */
    index = vsp_cmcp_server_find_client(cmcp_server, client_id);
    /* check for errors */
    VSP_ASSERT(index >= 0);

    /* free memory */
    VSP_FREE(cmcp_server->client_data[index]);

    /* move array entries */
    /* last registered client will be at the position of the deleted client */
    --cmcp_server->client_count;
    cmcp_server->client_ids[index] =
        cmcp_server->client_ids[cmcp_server->client_count];
    cmcp_server->client_data[index] =
        cmcp_server->client_data[cmcp_server->client_count];

    /* unsubscribe from messages from this client peer */
    vsp_cmcp_node_unsubscribe(cmcp_server->cmcp_node, client_id);

    /* invoke callback function */
    if (cmcp_server->disconnect_cb != NULL) {
        /* callback function registered; invoke it */
        cmcp_server->disconnect_cb(cmcp_server->callback_param, client_id);
    }

    /* client ID deleted from client peer list; deregistering done */
}
