/**
 * \file
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#if !defined VSP_CMCP_COMMAND_H_INCLUDED
#define VSP_CMCP_COMMAND_H_INCLUDED

#if defined __cplusplus
extern "C" {
#endif /* defined __cplusplus */

/** Internal message commands used for CMCP handshake as well as
 * connection establishment and maintaining.
 * Necessary data list parameters have to be listed here. */
typedef enum {
    /** Heartbeat signal command. No parameters required. */
    VSP_CMCP_COMMAND_HEARTBEAT
} vsp_cmcp_command_id;

#if defined __cplusplus
}
#endif /* defined __cplusplus */

#endif /* !defined VSP_CMCP_COMMAND_H_INCLUDED */
