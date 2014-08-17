/**
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#include "vsp_cmcp_node.h"

#include <vesper_util/vsp_error.h>
#include <vesper_util/vsp_random.h>
#include <vesper_util/vsp_util.h>
#include <nanomsg/nn.h>

/**
 * Send previously created message to the specified socket.
 * Blocks until message could be sent.
 * Returns non-zero and sets vsp_error_num() if failed.
 */
int vsp_cmcp_node_send_message(int socket, vsp_cmcp_message *cmcp_message);

vsp_cmcp_node *vsp_cmcp_node_create(vsp_cmcp_node_type node_type)
{
    vsp_cmcp_node *cmcp_node;
    /* check parameter */
    VSP_CHECK(node_type == VSP_CMCP_NODE_SERVER
        || node_type == VSP_CMCP_NODE_CLIENT,
        vsp_error_set_num(EINVAL); return NULL);
    /* allocate memory */
    VSP_ALLOC(cmcp_node, vsp_cmcp_node, return NULL);
    /* initialize struct data */
    cmcp_node->node_type = node_type;
    cmcp_node->state = 0;
    /* generate node ID that does not equal broadcast topic ID */
    if (cmcp_node->node_type == VSP_CMCP_NODE_SERVER) {
        /* server type: ID is even (first bit cleared) */
        do {
            cmcp_node->id = (uint16_t) (vsp_random_get() << 1);
        } while (cmcp_node->id == VSP_CMCP_BROADCAST_TOPIC_ID);
    } else {
        /* client type: ID is odd (first bit set) */
        do {
            cmcp_node->id = (uint16_t) (vsp_random_get() | 1);
        } while (cmcp_node->id == VSP_CMCP_BROADCAST_TOPIC_ID);
    }
    cmcp_node->publish_socket = -1;
    cmcp_node->subscribe_socket = -1;
    pthread_mutex_init(&cmcp_node->mutex, NULL);
    pthread_cond_init(&cmcp_node->condition, NULL);
    /* return struct pointer */
    return cmcp_node;
}

int vsp_cmcp_node_free(vsp_cmcp_node *cmcp_node)
{
    /* check parameter */
    VSP_CHECK(cmcp_node != NULL, vsp_error_set_num(EINVAL); return -1);

    /* clean up mutex and condition variable */
    pthread_mutex_destroy(&cmcp_node->mutex);
    pthread_cond_destroy(&cmcp_node->condition);

    /* free memory */
    VSP_FREE(cmcp_node);
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
