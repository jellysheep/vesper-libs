/**
 * \file
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#if !defined VSP_CMCP_NODE_H_INCLUDED
#define VSP_CMCP_NODE_H_INCLUDED

#include "vsp_cmcp_datalist.h"
#include "vsp_cmcp_message.h"

#if defined __cplusplus
extern "C" {
#endif /* defined __cplusplus */

/** Topic ID used to broadcast to all connected clients or servers. */
#define VSP_CMCP_BROADCAST_TOPIC_ID 0

/** Wall clock time in milliseconds between two heartbeat signals.
 * This is also the timeout of a receive call of a node. */
const int VSP_CMCP_NODE_HEARTBEAT_TIME;

/** Time interval in milliseconds. When a node has not received heartbeat
 * signals from a node peer for this amount of time, connection is timed out. */
#define VSP_CMCP_NODE_CONNECTION_TIMEOUT 10000

/** Node types. */
typedef enum {
    /** Server node. */
    VSP_CMCP_NODE_SERVER,
    /** Client node. */
    VSP_CMCP_NODE_CLIENT
} vsp_cmcp_node_type;

/** State and other data used for network connection.
 * Base type for vsp_cmcp_server and vsp_cmcp_client. */
struct vsp_cmcp_node;

/** Define type vsp_cmcp_node to avoid 'struct' keyword. */
typedef struct vsp_cmcp_node vsp_cmcp_node;

/**
 * Create new vsp_cmcp_node object.
 * The state of the newly created node is set to VSP_CMCP_NODE_UNINITIALIZED.
 * Returned pointer should be freed with vsp_cmcp_node_free().
 * The message_callback function will be called whenever a message is received
 * and vsp_cmcp_node_start() has been invoked (reception thread is running).
 * The regular_callback function will be called regularly with a time interval
 * as specified in VSP_CMCP_NODE_HEARTBEAT_TIME, or more frequently.
 * callback_param may be NULL.
 * Returns NULL and sets vsp_error_num() if failed.
 */
vsp_cmcp_node *vsp_cmcp_node_create(vsp_cmcp_node_type node_type,
    void (*message_callback)(void*, vsp_cmcp_message*),
    void (*regular_callback)(void*), void *callback_param);

/**
 * Free vsp_cmcp_node object.
 * Object should be created with vsp_cmcp_node_create().
 */
void vsp_cmcp_node_free(vsp_cmcp_node *cmcp_node);

/** Generate a new network ID for this node.
 * This function is called automatically when vsp_cmcp_node_create() is invoked.
 * It may be called to avoid duplicate node IDs if no peers are connected. */
void vsp_cmcp_node_generate_id(vsp_cmcp_node *cmcp_node);

/** Get the network ID of this node. */
uint16_t vsp_cmcp_node_get_id(vsp_cmcp_node *cmcp_node);

/**
 * Initialize and connect sockets.
 * Returns non-zero and sets vsp_error_num() if failed.
 */
int vsp_cmcp_node_connect(vsp_cmcp_node *cmcp_node,
    const char *publish_address, const char *subscribe_address);

/** Start message reception thread and wait until thread has started. */
void vsp_cmcp_node_start(vsp_cmcp_node *cmcp_node);

/** Stop message reception thread and wait until thread has finished. */
void vsp_cmcp_node_stop(vsp_cmcp_node *cmcp_node);

/**
 * Create and send message to the publish socket of the node.
 * Blocks until message could be sent. Frees internally created message object.
 * Returns non-zero and sets vsp_error_num() if failed.
 */
int vsp_cmcp_node_create_send_message(vsp_cmcp_node *cmcp_node,
    vsp_cmcp_message_type message_type,
    uint16_t topic_id, uint16_t sender_id, uint16_t command_id,
    vsp_cmcp_datalist *cmcp_datalist);

/**
 * Subscribe the node to the specified topic ID.
 */
void vsp_cmcp_node_subscribe(vsp_cmcp_node *cmcp_node, uint16_t topic_id);

/**
 * Unsubscribe the node from the specified topic ID.
 */
void vsp_cmcp_node_unsubscribe(vsp_cmcp_node *cmcp_node, uint16_t topic_id);

#if defined __cplusplus
}
#endif /* defined __cplusplus */

#endif /* !defined VSP_CMCP_NODE_H_INCLUDED */
