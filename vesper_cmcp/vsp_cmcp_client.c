/**
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#include "vsp_cmcp_client.h"

#if !defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* define "inline" keyword for pre-C99 C standard */
  #define inline
#endif
#include <nanomsg/nn.h>
#include <nanomsg/pubsub.h>
#include <stdlib.h>
#include <vesper_util/vsp_error.h>
#include <vesper_util/vsp_util.h>

/** vsp_cmcp_client finite state machine flag. */
typedef enum {
    /** Sockets are not initialized and not connected. */
    VSP_CMCP_CLIENT_UNINITIALIZED,
    /** Sockets are initialized and connected. */
    VSP_CMCP_CLIENT_INITIALIZED
} vsp_cmcp_state;

/** State and other data used for network connection. */
struct vsp_cmcp_client {
    /** Finite state machine flag. */
    vsp_cmcp_state state;
    /** nanomsg socket number to publish messages. */
    int publish_socket;
    /** nanomsg socket number to receive messages. */
    int subscribe_socket;
    /** Flag for reception thread (1 if running, 0 otherwise). */
    int receiving;
};

vsp_cmcp_client* vsp_cmcp_client_create(void)
{
    vsp_cmcp_client *cmcp_client;
    /* allocate memory */
    VSP_ALLOC(cmcp_client, vsp_cmcp_client, return NULL);
    /* initialize struct data */
    cmcp_client->state = VSP_CMCP_CLIENT_UNINITIALIZED;
    cmcp_client->publish_socket = -1;
    cmcp_client->subscribe_socket = -1;
    /* return struct pointer */
    return cmcp_client;
}

int vsp_cmcp_client_free(vsp_cmcp_client *cmcp_client)
{
    int ret;
    int success;

    success = 0;

    /* check parameter */
    VSP_ASSERT(cmcp_client != NULL, vsp_error_set_num(EINVAL); return -1);

    if (cmcp_client->state != VSP_CMCP_CLIENT_UNINITIALIZED) {
        /* close publish socket */
        ret = nn_close(cmcp_client->publish_socket);
        /* check error set by nanomsg */
        VSP_ASSERT(ret == 0, success = -1);

        /* close subscribe socket */
        ret = nn_close(cmcp_client->subscribe_socket);
        /* check error set by nanomsg */
        VSP_ASSERT(ret == 0, success = -1);
    }
    /* free memory */
    VSP_FREE(cmcp_client);
    return success;
}

int vsp_cmcp_client_connect(vsp_cmcp_client *cmcp_client,
    const char *publish_address, const char *subscribe_address)
{
    int ret;

    /* check parameters */
    VSP_ASSERT(cmcp_client != NULL && publish_address != NULL
        && subscribe_address != NULL, vsp_error_set_num(EINVAL); return -1);
    /* check sockets not yet initialized */
    VSP_ASSERT(cmcp_client->state == VSP_CMCP_CLIENT_UNINITIALIZED,
        vsp_error_set_num(EALREADY); return -1);

    /* initialize sockets */
    cmcp_client->publish_socket = nn_socket(AF_SP, NN_PUB);
    cmcp_client->subscribe_socket = nn_socket(AF_SP, NN_SUB);

    /* connect sockets */
    ret = nn_connect(cmcp_client->publish_socket, publish_address);
    /* check error set by nanomsg */
    VSP_ASSERT(ret >= 0, return -1);

    ret = nn_connect(cmcp_client->subscribe_socket, subscribe_address);
    /* check error set by nanomsg */
    VSP_ASSERT(ret >= 0, return -1);

    /* set state */
    cmcp_client->state = VSP_CMCP_CLIENT_INITIALIZED;
    /* sockets successfully connected */
    return 0;
}

int vsp_cmcp_client_disconnect(vsp_cmcp_client *cmcp_client)
{
    int ret;

    /* check parameter */
    VSP_ASSERT(cmcp_client != NULL, vsp_error_set_num(EINVAL); return -1);

    /* check sockets already initialized */
    VSP_ASSERT(cmcp_client->state != VSP_CMCP_CLIENT_UNINITIALIZED,
        vsp_error_set_num(ENOTCONN); return -1);

    /* disconnect sockets */
    ret = nn_close(cmcp_client->publish_socket);
    /* check error set by nanomsg */
    VSP_ASSERT(ret == 0, return -1);

    ret = nn_close(cmcp_client->subscribe_socket);
    /* check error set by nanomsg */
    VSP_ASSERT(ret == 0, return -1);

    /* deinitialize sockets */
    cmcp_client->publish_socket = -1;
    cmcp_client->subscribe_socket = -1;

    /* set state */
    cmcp_client->state = VSP_CMCP_CLIENT_UNINITIALIZED;
    /* sockets successfully disconnected */
    return 0;
}

int vsp_cmcp_client_reception_thread_run(vsp_cmcp_client *cmcp_client)
{
    /* check parameter */
    VSP_ASSERT(cmcp_client != NULL, vsp_error_set_num(EINVAL); return -1);

    cmcp_client->receiving = 1;
    /* reception loop */
    while (cmcp_client->receiving) {

    }
    /* success */
    return 0;
}

int vsp_cmcp_client_reception_thread_stop(vsp_cmcp_client *cmcp_client)
{
    /* check parameter */
    VSP_ASSERT(cmcp_client != NULL, vsp_error_set_num(EINVAL); return -1);

    /* stop reception */
    cmcp_client->receiving = 0;
    /* success */
    return 0;
}
