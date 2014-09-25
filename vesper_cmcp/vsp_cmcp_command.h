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

/** Internal message commands from servers used for CMCP handshake as well as
 * connection establishment and maintaining.
 * Necessary data list parameters have to be listed here. */
typedef enum {
    /** Server heartbeat signal command. No parameters required. */
    VSP_CMCP_COMMAND_SERVER_HEARTBEAT,
    /** Acknowledge signal when registering a new client.
     * Parameters: VSP_CMCP_PARAMETER_NONCE. */
    VSP_CMCP_COMMAND_SERVER_ACK_CLIENT,
    /** Negative acknowledge when rejecting a new client.
     * Parameters: VSP_CMCP_PARAMETER_NONCE. */
    VSP_CMCP_COMMAND_SERVER_NACK_CLIENT
} vsp_cmcp_server_command_id;

/** Internal message commands from clients used for CMCP handshake as well as
 * connection establishment and maintaining.
 * Necessary data list parameters have to be listed here. */
typedef enum {
    /** Command to announce client connection to server.
     * Parameters: VSP_CMCP_PARAMETER_NONCE. */
    VSP_CMCP_COMMAND_CLIENT_ANNOUNCE,
    /** Client heartbeat signal command. No parameters required. */
    VSP_CMCP_COMMAND_CLIENT_HEARTBEAT,
    /** Client disconnection command. No parameters required. */
    VSP_CMCP_COMMAND_CLIENT_DISCONNECT
} vsp_cmcp_client_command_id;

/** Internal message command parameters used for CMCP handshake as well as
 * connection establishment and maintaining.
 * The data type and size (in bytes) has to be specified. */
typedef enum {
    /** A randomly generated nonce used for temporary node indentification.
     * Type: uint64_t. Size: 8 bytes. */
    VSP_CMCP_PARAMETER_NONCE
} vsp_cmcp_command_parameter_id;

#if defined __cplusplus
}
#endif /* defined __cplusplus */

#endif /* !defined VSP_CMCP_COMMAND_H_INCLUDED */
