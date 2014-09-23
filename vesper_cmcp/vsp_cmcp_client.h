/**
 * \file
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#if !defined VSP_CMCP_CLIENT_H_INCLUDED
#define VSP_CMCP_CLIENT_H_INCLUDED

#include "vsp_cmcp_datalist.h"

#include <vesper_util/vsp_api.h>
#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif /* defined __cplusplus */

/** State and other data used for (network) connection. */
struct vsp_cmcp_client;

/** Define type vsp_cmcp_client to avoid 'struct' keyword. */
typedef struct vsp_cmcp_client vsp_cmcp_client;

/**
 * Create new vsp_cmcp_client object.
 * Returned pointer should be freed with vsp_cmcp_client_free().
 * Returns NULL and sets vsp_error_num() if failed.
 */
VSP_API vsp_cmcp_client *vsp_cmcp_client_create(void);

/**
 * Free vsp_cmcp_client object.
 * Object should be created with vsp_cmcp_client_create().
 */
VSP_API void vsp_cmcp_client_free(vsp_cmcp_client *cmcp_client);

/**
 * Initialize sockets and establish connection.
 * An internal message reception thread is started.
 * Returns non-zero and sets vsp_error_num() if failed.
 */
VSP_API int vsp_cmcp_client_connect(vsp_cmcp_client *cmcp_client,
    const char *publish_address, const char *subscribe_address);

/**
 * Send a message to the connected server.
 * The specified command_id has to be lower than 2^15, i.e. MSB cleared.
 * This function blocks until the message could be sent.
 * Returns non-zero and sets vsp_error_num() if failed.
 */
VSP_API int vsp_cmcp_client_send(vsp_cmcp_client *cmcp_client,
    uint16_t command_id, vsp_cmcp_datalist *cmcp_datalist);

#if defined __cplusplus
}
#endif /* defined __cplusplus */

#endif /* !defined VSP_CMCP_CLIENT_H_INCLUDED */
