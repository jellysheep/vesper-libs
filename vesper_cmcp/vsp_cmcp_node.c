/**
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#include "vsp_cmcp_node.h"

#include <vesper_util/vsp_util.h>
#include <nanomsg/nn.h>

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

int vsp_cmcp_node_create_send_message(int socket, uint16_t topic_id,
    uint16_t sender_id, uint16_t command_id, vsp_cmcp_datalist *cmcp_datalist)
{
    int ret;
    int success;
    vsp_cmcp_message *cmcp_message;

    /* clear state, no errors yet */
    success = 0;

    /* check parameters; data list may be NULL */
    VSP_ASSERT(socket != -1, vsp_error_set_num(EINVAL); return -1);

    /* generate message */
    cmcp_message = vsp_cmcp_message_create(topic_id, sender_id, command_id,
        cmcp_datalist);
    /* check for error */
    VSP_ASSERT(cmcp_message != NULL, return -1);

    /* send message */
    ret = vsp_cmcp_node_send_message(socket, cmcp_message);
    /* check for error, but do not directly return because of memory leak */
    VSP_ASSERT(ret == 0, success = -1);

    /* free message */
    ret = vsp_cmcp_message_free(cmcp_message);
    /* check for error */
    VSP_ASSERT(ret == 0, return -1);

    /* success */
    return success;
}
