/**
 * \file
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#if !defined VSP_CMCP_SERVER_H_INCLUDED
#define VSP_CMCP_SERVER_H_INCLUDED

#include <vesper_util/vsp_api.h>
#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif /* defined __cplusplus */

/** State and other data used for (network) connection. */
struct vsp_cmcp_server;

/** Define type vsp_cmcp_server to avoid 'struct' keyword. */
typedef struct vsp_cmcp_server vsp_cmcp_server;

/** Callback function invoked for every newly announced client.
 * If callback returns zero, client will be registered (ACK message sent);
 * otherwise client will be rejected (NACK message sent). */
typedef int (*vsp_cmcp_server_announcement_cb)(void*, uint16_t);

/**
 * Create new vsp_cmcp_server object.
 * Returned pointer should be freed with vsp_cmcp_server_free().
 * Returns NULL and sets vsp_error_num() if failed.
 */
VSP_API vsp_cmcp_server *vsp_cmcp_server_create(void);

/**
 * Free vsp_cmcp_server object.
 * Object should be created with vsp_cmcp_server_create().
 */
VSP_API void vsp_cmcp_server_free(vsp_cmcp_server *cmcp_server);

/**
 * Set callback function invoked for every newly announced client.
 * If announcement_cb is NULL, the callback function is cleared. In this case,
 * or if this function was not invoked yet (i.e. no callback is registered),
 * newly announced clients will be rejected. callback_param may be NULL.
 */
VSP_API void vsp_cmcp_server_set_announcement_cb(vsp_cmcp_server *cmcp_server,
    vsp_cmcp_server_announcement_cb announcement_cb, void *callback_param);

/**
 * Initialize sockets and wait for incoming connections.
 * An internal message reception thread is started.
 * Returns non-zero and sets vsp_error_num() if failed.
 */
VSP_API int vsp_cmcp_server_bind(vsp_cmcp_server *cmcp_server,
    const char *publish_address, const char *subscribe_address);

#if defined __cplusplus
}
#endif /* defined __cplusplus */

#endif /* !defined VSP_CMCP_SERVER_H_INCLUDED */
