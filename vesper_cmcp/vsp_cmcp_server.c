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
#include <vesper_util/vsp_util.h>

#define VSP_CMCP_SERVER_MAX_PEERS 16

/** Data of a network node connected to server. */
struct vsp_cmcp_server_peer {
};

/** Define type vsp_cmcp_server_peer to avoid 'struct' keyword. */
typedef struct vsp_cmcp_server_peer vsp_cmcp_server_peer;

/** State and other data used for network connection. */
struct vsp_cmcp_server {
    /** Basic node and finite-state machine data. */
    vsp_cmcp_node *cmcp_node;
    /** Number of registered client peers. */
    uint16_t client_count;
    /** Client peer IDs. */
    uint16_t client_ids[VSP_CMCP_SERVER_MAX_PEERS];
    /** Client peer data. */
    vsp_cmcp_server_peer *client_data[VSP_CMCP_SERVER_MAX_PEERS];
};

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

vsp_cmcp_server *vsp_cmcp_server_create(void)
{
    vsp_cmcp_server *cmcp_server;
    /* allocate memory */
    VSP_ALLOC(cmcp_server, vsp_cmcp_server);
    /* initialize base type */
    cmcp_server->cmcp_node = vsp_cmcp_node_create(VSP_CMCP_NODE_SERVER,
        vsp_cmcp_server_message_callback, cmcp_server);
    /* in case of failure vsp_error_num() is already set */
    VSP_CHECK(cmcp_server->cmcp_node != NULL,
        VSP_FREE(cmcp_server); return NULL);
    /* no client peers registered yet */
    cmcp_server->client_count = 0;
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
    /* check for errors */
    VSP_CHECK(ret == 0, return -1);

    /* start worker thread */
    vsp_cmcp_node_start(cmcp_server->cmcp_node);
    /* check for errors */
    VSP_CHECK(ret == 0, return -1);

    /* sockets successfully bound */
    return 0;
}

void vsp_cmcp_server_message_callback(void *param,
    vsp_cmcp_message *cmcp_message)
{
    vsp_cmcp_server *cmcp_server;
    vsp_cmcp_datalist *cmcp_datalist;
    uint16_t topic_id, sender_id, command_id;

    /* check parameters; failures are silently ignored */
    VSP_CHECK(param != NULL && cmcp_message != NULL, return);

    cmcp_server = (vsp_cmcp_server*) param;

    /* get message IDs */
    topic_id = vsp_cmcp_message_get_topic_id(cmcp_message);
    sender_id = vsp_cmcp_message_get_sender_id(cmcp_message);
    command_id = vsp_cmcp_message_get_command_id(cmcp_message);

    /* check if message has valid sender */
    VSP_CHECK(sender_id != VSP_CMCP_BROADCAST_TOPIC_ID, return);

    /* get data list */
    cmcp_datalist = vsp_cmcp_message_get_datalist(cmcp_message);
    /* check data list; failures are silently ignored */
    VSP_CHECK(cmcp_datalist != NULL, return);

    /* check if internal control message received */
    if (topic_id == VSP_CMCP_BROADCAST_TOPIC_ID) {
        /* handle control message */
        vsp_cmcp_server_handle_control_message(cmcp_server, sender_id,
            command_id, cmcp_datalist);
    } else {
        /* handle user message */
        /* ... */
    }
}

void vsp_cmcp_server_handle_control_message(vsp_cmcp_server *cmcp_server,
    uint16_t sender_id, uint16_t command_id, vsp_cmcp_datalist *cmcp_datalist)
{
    /* check if client announcement received */
    if ((sender_id & 1) == 1
        && command_id == VSP_CMCP_COMMAND_CLIENT_ANNOUNCE) {
        /* client announcement received */
        uint64_t *client_nonce;
        /* get client nonce */
        client_nonce = vsp_cmcp_datalist_get_data_item(cmcp_datalist,
            VSP_CMCP_PARAMETER_NONCE, sizeof(uint64_t));
        /* check data list item (nonce); failures are silently ignored */
        VSP_CHECK(client_nonce != NULL, return);
        /* try to register client peer */
        vsp_cmcp_server_register_client(cmcp_server, sender_id, *client_nonce);
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
        /* register client peer ID */
        vsp_cmcp_server_peer *client;
        /* create client data struct */
        VSP_ALLOC(client, vsp_cmcp_server_peer);
        /* initialize struct data */
        /* ... */
        /* store client peer ID */
        cmcp_server->client_ids[cmcp_server->client_count] = client_id;
        /* store client peer data */
        cmcp_server->client_data[cmcp_server->client_count] = client;
        /* increment client peer count */
        ++cmcp_server->client_count;
        /* success */
        success = 0;
    }
    if (success == 0) {
        /* new client peer ID registered, send acknowledge message */
        ret = vsp_cmcp_node_create_send_message(cmcp_server->cmcp_node,
            VSP_CMCP_BROADCAST_TOPIC_ID, cmcp_server->cmcp_node->id,
            VSP_CMCP_COMMAND_SERVER_ACK_CLIENT, cmcp_datalist);
        /* check for errors */
        VSP_CHECK(ret == 0, /* failures are silently ignored */);
    } else {
        /* new client peer ID rejected, send negative acknowledge message */
        ret = vsp_cmcp_node_create_send_message(cmcp_server->cmcp_node,
            VSP_CMCP_BROADCAST_TOPIC_ID, cmcp_server->cmcp_node->id,
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
