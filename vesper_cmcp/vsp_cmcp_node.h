/**
 * \file
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#if !defined VSP_CMCP_COMMON_H_INCLUDED
#define VSP_CMCP_COMMON_H_INCLUDED

#include "vsp_cmcp_datalist.h"
#include "vsp_cmcp_message.h"

#include <pthread.h>

#if defined __cplusplus
extern "C" {
#endif /* defined __cplusplus */

/** Topic ID used to broadcast to all connected clients or servers. */
#define VSP_CMCP_BROADCAST_TOPIC_ID 0

/** Node types. */
typedef enum {
    /** Server node. */
    VSP_CMCP_NODE_SERVER,
    /** Client node. */
    VSP_CMCP_NODE_CLIENT
} vsp_cmcp_node_type;

/** State and other data used for network connection.
 * Base type for vsp_cmcp_server and vsp_cmcp_client.
 * This structure implements basic functions of a finite-state machine. */
struct vsp_cmcp_node {
    /** Node type. */
    vsp_cmcp_node_type node_type;
    /** ID identifying this node in the network.
     * Must not equal broadcast topic ID. */
    uint16_t id;
    /** Finite state machine flag. */
    volatile int state;
    /** Mutex locking state changes. */
    pthread_mutex_t mutex;
    /** Condition variable used to wait for state changes. */
    pthread_cond_t condition;
    /** nanomsg socket number to publish messages. */
    int publish_socket;
    /** nanomsg socket number to receive messages. */
    int subscribe_socket;
    /** Reception thread. */
    pthread_t thread;
};

/** Define type vsp_cmcp_node to avoid 'struct' keyword. */
typedef struct vsp_cmcp_node vsp_cmcp_node;

/**
 * Create new vsp_cmcp_node object.
 * The state of the newly created node is set to zero.
 * Returned pointer should be freed with vsp_cmcp_node_free().
 * Returns NULL and sets vsp_error_num() if failed.
 */
vsp_cmcp_node *vsp_cmcp_node_create(vsp_cmcp_node_type node_type);

/**
 * Free vsp_cmcp_node object.
 * Object should be created with vsp_cmcp_node_create().
 * Returns non-zero and sets vsp_error_num() if failed.
 */
int vsp_cmcp_node_free(vsp_cmcp_node *cmcp_node);

/**
 * Create and send message to the node's publish socket.
 * Blocks until message could be sent. Frees internally created message object.
 * Returns non-zero and sets vsp_error_num() if failed.
 */
int vsp_cmcp_node_create_send_message(vsp_cmcp_node *cmcp_node,
    uint16_t topic_id, uint16_t sender_id, uint16_t command_id,
    vsp_cmcp_datalist *cmcp_datalist);

#if defined __cplusplus
}
#endif /* defined __cplusplus */

#endif /* !defined VSP_CMCP_COMMON_H_INCLUDED */
