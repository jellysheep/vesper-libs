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

#include "vsp_cmcp_datalist.h"

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
 * The parameters are the callback parameter set by
 * vsp_cmcp_server_set_callback_param() and the client ID.
 * If the callback returns zero, the client will be registered (ACK message
 * sent); otherwise the client will be rejected (NACK message sent). */
typedef int (*vsp_cmcp_server_announcement_cb)(void*, uint16_t);

/** Callback function invoked for a received message.
 * The parameters are the callback parameter set by
 * vsp_cmcp_server_set_callback_param(), the client ID, the command ID and the
 * data list. */
typedef void (*vsp_cmcp_server_message_cb)(void*, uint16_t, uint16_t,
    vsp_cmcp_datalist*);

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
 * Set callback function parameter used for all registered callback functions.
 * callback_param may be NULL.
 */
VSP_API void vsp_cmcp_server_set_callback_param(vsp_cmcp_server *cmcp_server,
    void *callback_param);

/**
 * Set callback function invoked for every newly announced client.
 * If announcement_cb is NULL, the callback function is cleared. In this case,
 * or if this function was not invoked yet (i.e. no callback is registered),
 * newly announced clients will be rejected.
 */
VSP_API void vsp_cmcp_server_set_announcement_cb(vsp_cmcp_server *cmcp_server,
    vsp_cmcp_server_announcement_cb announcement_cb);

/**
 * Set callback function invoked for every received message.
 * If message_cb is NULL, the callback function is cleared.
 */
VSP_API void vsp_cmcp_server_set_message_cb(vsp_cmcp_server *cmcp_server,
    vsp_cmcp_server_message_cb message_cb);

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
