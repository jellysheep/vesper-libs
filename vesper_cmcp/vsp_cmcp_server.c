/**
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#include "vsp_cmcp_server.h"
#include "vsp_cmcp_node.h"

#include <vesper_util/vsp_error.h>
#include <vesper_util/vsp_util.h>

/** State and other data used for network connection. */
struct vsp_cmcp_server {
    /** Basic node and finite-state machine data. */
    vsp_cmcp_node *cmcp_node;
};

/** Message callback function invoked by cmcp_node.
 * This function will be called for every received message. */
static void vsp_cmcp_server_message_callback(void *param,
    vsp_cmcp_message *cmcp_message);

vsp_cmcp_server *vsp_cmcp_server_create(void)
{
    vsp_cmcp_server *cmcp_server;
    /* allocate memory */
    VSP_ALLOC(cmcp_server, vsp_cmcp_server, return NULL);
    /* initialize base type */
    cmcp_server->cmcp_node = vsp_cmcp_node_create(VSP_CMCP_NODE_SERVER,
        vsp_cmcp_server_message_callback, cmcp_server);
    /* in case of failure vsp_error_num() is already set */
    VSP_ASSERT(cmcp_server->cmcp_node != NULL,
        VSP_FREE(cmcp_server); return NULL);
    /* return struct pointer */
    return cmcp_server;
}

int vsp_cmcp_server_free(vsp_cmcp_server *cmcp_server)
{
    int ret;
    int success;

    success = 0;

    /* check parameter */
    VSP_CHECK(cmcp_server != NULL, vsp_error_set_num(EINVAL); return -1);

    /* free node base type */
    ret = vsp_cmcp_node_free(cmcp_server->cmcp_node);
    VSP_ASSERT(ret == 0,
        /* failures are silently ignored in release build */);

    /* free memory */
    VSP_FREE(cmcp_server);
    return success;
}

int vsp_cmcp_server_bind(vsp_cmcp_server *cmcp_server,
    const char *publish_address, const char *subscribe_address)
{
    int ret;

    /* check parameter */
    VSP_CHECK(cmcp_server != NULL, vsp_error_set_num(EINVAL); return -1);

    /* bind sockets */
    ret = vsp_cmcp_node_connect(cmcp_server->cmcp_node,
        publish_address, subscribe_address);
    /* check error */
    VSP_CHECK(ret == 0, return -1);

    /* start worker thread */
    ret = vsp_cmcp_node_start(cmcp_server->cmcp_node);
    /* check error */
    VSP_ASSERT(ret == 0, return -1);

    /* sockets successfully bound */
    return 0;
}

void vsp_cmcp_server_message_callback(void *param,
    vsp_cmcp_message *cmcp_message)
{
    vsp_cmcp_server *cmcp_server;

    /* check parameters; failures are silently ignored in release build */
    VSP_ASSERT(param != NULL && cmcp_message != NULL, return);

    cmcp_server = (vsp_cmcp_server*) param;

    /* process message */
}
