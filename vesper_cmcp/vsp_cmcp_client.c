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
#include <nanomsg/nn.h>

/** Wall clock time in milliseconds between two client heartbeat signals. */
#define VSP_CMCP_CLIENT_CONNECTION_TIMEOUT 60000

/** vsp_cmcp_node finite state machine flag. */
typedef enum {
    /** Client is not connected to server. */
    VSP_CMCP_CLIENT_DISCONNECTED,
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
    /** Client finite state machine flag, not related to node state. */
    vsp_cmcp_state *state;
};

/** Connect to server and establish connection using handshake.
 * Connection establishment is specified by CMCP protocol.
 * Returns non-zero and sets vsp_error_num() if failed. */
static int vsp_cmcp_client_establish_connection(vsp_cmcp_client *cmcp_client);

/** Send an announcement message to server.
 * Returns non-zero and sets vsp_error_num() if failed. */
static int vsp_cmcp_client_send_announcement(vsp_cmcp_client *cmcp_client);

/** Message callback function invoked by cmcp_node.
 * This function will be called for every received message. */
static void vsp_cmcp_client_message_callback(void *param,
    vsp_cmcp_message *cmcp_message);

vsp_cmcp_client *vsp_cmcp_client_create(void)
{
    vsp_cmcp_client *cmcp_client;
    /* allocate memory */
    VSP_ALLOC(cmcp_client, vsp_cmcp_client);
    /* initialize base type */
    cmcp_client->cmcp_node = vsp_cmcp_node_create(VSP_CMCP_NODE_CLIENT,
        vsp_cmcp_client_message_callback, cmcp_client);
    /* in case of failure vsp_error_num() is already set */
    VSP_CHECK(cmcp_client->cmcp_node != NULL,
        VSP_FREE(cmcp_client); return NULL);
    /* initialize state struct */
    cmcp_client->state = vsp_cmcp_state_create(VSP_CMCP_CLIENT_DISCONNECTED);
    /* in case of failure vsp_error_num() is already set */
    VSP_CHECK(cmcp_client->state != NULL, VSP_FREE(cmcp_client); return NULL);
    /* return struct pointer */
    return cmcp_client;
}

void vsp_cmcp_client_free(vsp_cmcp_client *cmcp_client)
{
    /* check parameter */
    VSP_CHECK(cmcp_client != NULL, return);

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

    /* connect sockets; the node's state is checked by this function */
    ret = vsp_cmcp_node_connect(cmcp_client->cmcp_node,
        publish_address, subscribe_address);
    /* check for errors */
    VSP_CHECK(ret == 0, return -1);

    /* start worker thread */
    vsp_cmcp_node_start(cmcp_client->cmcp_node);

    /* establish connection */
    ret = vsp_cmcp_client_establish_connection(cmcp_client);
    /* check for errors */
    VSP_CHECK(ret == 0, return -1);

    /* successfully connected and waiting for messages */
    return 0;
}

int vsp_cmcp_client_establish_connection(vsp_cmcp_client *cmcp_client)
{
    int ret;
    int success;
    int state;
    struct timespec time_now, time_connection_timeout;

    /* initialize local variables */
    success = 0;

    /* start measuring time for heartbeat timeout */
    vsp_time_real_timespec(&time_now);
    time_connection_timeout.tv_sec = time_now.tv_sec
        + (VSP_CMCP_CLIENT_CONNECTION_TIMEOUT / 1000);
    time_connection_timeout.tv_nsec = time_now.tv_nsec;

    /* wait until connected or waiting timed out */
    do {
        vsp_cmcp_state_lock(cmcp_client->state);
        ret = vsp_cmcp_state_wait(cmcp_client->state, &time_connection_timeout);
        state = vsp_cmcp_state_get(cmcp_client->state);
    } while (ret == 0 && state != VSP_CMCP_CLIENT_HEARTBEAT_RECEIVED);

    /* check if connected */
    VSP_CHECK(state == VSP_CMCP_CLIENT_HEARTBEAT_RECEIVED, success = -1);

    return success;
}

void vsp_cmcp_client_message_callback(void *param,
    vsp_cmcp_message *cmcp_message)
{
    vsp_cmcp_client *cmcp_client;
    int ret;
    uint16_t topic_id, sender_id, command_id;
    uint64_t nonce;

    /* check parameters; failures are silently ignored */
    VSP_CHECK(param != NULL && cmcp_message != NULL, return);

    cmcp_client = (vsp_cmcp_client*) param;

    /* process message */
    if (vsp_cmcp_state_get(cmcp_client->state)
        == VSP_CMCP_CLIENT_DISCONNECTED) {
        /* receive only server heartbeat signals */
        topic_id = vsp_cmcp_message_get_topic_id(cmcp_message);
        sender_id = vsp_cmcp_message_get_sender_id(cmcp_message);
        command_id = vsp_cmcp_message_get_command_id(cmcp_message);
        /* check if server heartbeat received */
        VSP_CHECK(topic_id == VSP_CMCP_BROADCAST_TOPIC_ID
            && sender_id != 0 && (sender_id & 1) == 0
            && command_id == VSP_CMCP_COMMAND_SERVER_HEARTBEAT, return);
        /* server heartbeat received */
        vsp_cmcp_state_set(cmcp_client->state,
            VSP_CMCP_CLIENT_HEARTBEAT_RECEIVED);
        /* send announcement message */
        ret = vsp_cmcp_client_send_announcement(cmcp_client);
        /* check for errors; failures are silently ignored */
        VSP_CHECK(ret == 0, return);
    }
}

int vsp_cmcp_client_send_announcement(vsp_cmcp_client *cmcp_client)
{
    int ret;
    int success;
    vsp_cmcp_datalist *cmcp_datalist;
    uint64_t nonce;

    /* initialize local variables */
    success = 0;
    cmcp_datalist = NULL;

    /* create identification nonce */
    nonce = vsp_random_get();

    /* create announcement command message */
    cmcp_datalist = vsp_cmcp_datalist_create();
    /* check for errors */
    VSP_ASSERT(cmcp_datalist != NULL);
    /* add nonce parameter as a data list item */
    ret = vsp_cmcp_datalist_add_item(cmcp_datalist, VSP_CMCP_PARAMETER_NONCE,
        sizeof(nonce), &nonce);
    /* check for errors */
    VSP_CHECK(ret == 0, goto error_exit);
    /* send message */
    ret = vsp_cmcp_node_create_send_message(cmcp_client->cmcp_node,
        VSP_CMCP_BROADCAST_TOPIC_ID, cmcp_client->cmcp_node->id,
        VSP_CMCP_COMMAND_CLIENT_ANNOUNCE, cmcp_datalist);
    /* check for errors */
    VSP_CHECK(ret == 0, goto error_exit);

    /* success: clean up and exit */
    goto cleanup_exit;

    error_exit:
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
