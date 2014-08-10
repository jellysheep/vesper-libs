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

#if defined __cplusplus
extern "C" {
#endif /* defined __cplusplus */

/** Topic ID used to broadcast to all connected clients or servers. */
#define VSP_CMCP_BROADCAST_TOPIC_ID 0

/**
 * Send message to the specified socket.
 * Blocks until message could be sent.
 * Returns non-zero and sets vsp_error_num() if failed.
 */
int vsp_cmcp_common_send_message(int socket, vsp_cmcp_message *cmcp_message);

/**
 * Create and send message to the specified socket.
 * Blocks until message could be sent. Frees internally created message object.
 * Returns non-zero and sets vsp_error_num() if failed.
 */
int vsp_cmcp_common_create_send_message(int socket, uint16_t topic_id,
    uint16_t sender_id, uint16_t command_id, vsp_cmcp_datalist *cmcp_datalist);

#if defined __cplusplus
}
#endif /* defined __cplusplus */

#endif /* !defined VSP_CMCP_COMMON_H_INCLUDED */
