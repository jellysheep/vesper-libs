/**
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#include "vsp_cmcp_common.h"

#include <vesper_util/vsp_util.h>
#include <nanomsg/nn.h>

int vsp_cmcp_common_send_message(int socket, vsp_cmcp_message *cmcp_message)
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
