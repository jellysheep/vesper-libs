/**
 * \file
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#include "minunit.h"
#include "vsp_test.h"

#include <vesper_cmcp/vsp_cmcp_client.h>
#include <vesper_cmcp/vsp_cmcp_node.h>
#include <vesper_cmcp/vsp_cmcp_server.h>
#include <vesper_util/vsp_error.h>
#include <errno.h>
#include <stddef.h>

/** Global CMCP server object. */
vsp_cmcp_server *global_cmcp_server;

/** Global CMCP client object. */
vsp_cmcp_client *global_cmcp_client;

/** Client announcement callback function. */
int vsp_test_cmcp_connection_cb(void*, uint16_t);

/** Create global global_cmcp_server and global_cmcp_client objects. */
void vsp_test_cmcp_connection_setup(void);

/** Free global global_cmcp_server and global_cmcp_client objects. */
void vsp_test_cmcp_connection_teardown(void);

/** Test vsp_cmcp_server_create() and vsp_cmcp_server_free(). */
MU_TEST(vsp_test_cmcp_server_allocation);

/** Test vsp_cmcp_client_create() and vsp_cmcp_client_free(). */
MU_TEST(vsp_test_cmcp_client_allocation);

/** Test to invoke server functions with invalid parameters and check if they
 * behave correctly and do not abort. */
MU_TEST(vsp_test_cmcp_server_invalid_parameters);

/** Test to invoke client functions with invalid parameters and check if they
 * behave correctly and do not abort. */
MU_TEST(vsp_test_cmcp_client_invalid_parameters);

/** Test connection between vsp_cmcp_server and vsp_cmcp_client. */
MU_TEST(vsp_test_cmcp_connection_test);

int vsp_test_cmcp_connection_cb(void *callback_param, uint16_t client_id)
{
    /* check if callback parameter equals global server object */
    mu_assert_abort(callback_param == global_cmcp_server,
        vsp_error_str(EINVAL));
    /* check if client ID is valid */
    mu_assert_abort(client_id != VSP_CMCP_BROADCAST_TOPIC_ID
        && (client_id & 1) == 1, vsp_error_str(EINVAL));
    /* accept new client */
    return 0;
}

void vsp_test_cmcp_connection_setup(void)
{
    /* create server */
    global_cmcp_server = vsp_cmcp_server_create();
    mu_assert_abort(global_cmcp_server != NULL, vsp_error_str(vsp_error_num()));
    /* register callback function */
    vsp_cmcp_server_set_announcement_cb(global_cmcp_server,
        vsp_test_cmcp_connection_cb, global_cmcp_server);
    /* create client */
    global_cmcp_client = vsp_cmcp_client_create();
    mu_assert_abort(global_cmcp_client != NULL, vsp_error_str(vsp_error_num()));
}

void vsp_test_cmcp_connection_teardown(void)
{
    /* free client */
    vsp_cmcp_client_free(global_cmcp_client);
    /* free server */
    vsp_cmcp_server_free(global_cmcp_server);
}

MU_TEST(vsp_test_cmcp_server_allocation)
{
    vsp_cmcp_server *local_global_cmcp_server;
    /* allocation */
    local_global_cmcp_server = vsp_cmcp_server_create();
    mu_assert_abort(local_global_cmcp_server != NULL,
        vsp_error_str(vsp_error_num()));
    /* deallocation */
    vsp_cmcp_server_free(local_global_cmcp_server);
}

MU_TEST(vsp_test_cmcp_client_allocation)
{
    vsp_cmcp_client *local_global_cmcp_client;
    /* allocation */
    local_global_cmcp_client = vsp_cmcp_client_create();
    mu_assert_abort(local_global_cmcp_client != NULL,
        vsp_error_str(vsp_error_num()));
    /* deallocation */
    vsp_cmcp_client_free(local_global_cmcp_client);
}

MU_TEST(vsp_test_cmcp_server_invalid_parameters)
{
    int ret;

    /* invalid server deallocation */
    vsp_cmcp_server_free(NULL);

    /* invalid server bind: server object NULL */
    ret = vsp_cmcp_server_bind(NULL,
        VSP_TEST_SERVER_PUBLISH_ADDRESS, VSP_TEST_SERVER_SUBSCRIBE_ADDRESS);
    mu_assert(ret != 0, VSP_TEST_INVALID_PARAMETER_ACCEPTED);

    /* invalid server bind: address NULL */
    ret = vsp_cmcp_server_bind(global_cmcp_server,
        NULL, VSP_TEST_SERVER_SUBSCRIBE_ADDRESS);
    mu_assert(ret != 0, VSP_TEST_INVALID_PARAMETER_ACCEPTED);

    ret = vsp_cmcp_server_bind(global_cmcp_server,
        VSP_TEST_SERVER_PUBLISH_ADDRESS, NULL);
    mu_assert(ret != 0, VSP_TEST_INVALID_PARAMETER_ACCEPTED);

    /* invalid server bind: address empty */
    ret = vsp_cmcp_server_bind(global_cmcp_server,
        "", VSP_TEST_SERVER_SUBSCRIBE_ADDRESS);
    mu_assert(ret != 0, VSP_TEST_INVALID_PARAMETER_ACCEPTED);

    ret = vsp_cmcp_server_bind(global_cmcp_server,
        VSP_TEST_SERVER_PUBLISH_ADDRESS, "");
    mu_assert(ret != 0, VSP_TEST_INVALID_PARAMETER_ACCEPTED);
}

MU_TEST(vsp_test_cmcp_client_invalid_parameters)
{
    int ret;

    /* invalid client deallocation */
    vsp_cmcp_client_free(NULL);

    /* invalid client connect: client object NULL */
    ret = vsp_cmcp_client_connect(NULL,
        VSP_TEST_SERVER_SUBSCRIBE_ADDRESS, VSP_TEST_SERVER_PUBLISH_ADDRESS);
    mu_assert(ret != 0, VSP_TEST_INVALID_PARAMETER_ACCEPTED);

    /* invalid client connect: address NULL */
    ret = vsp_cmcp_client_connect(global_cmcp_client,
        NULL, VSP_TEST_SERVER_PUBLISH_ADDRESS);
    mu_assert(ret != 0, VSP_TEST_INVALID_PARAMETER_ACCEPTED);

    ret = vsp_cmcp_client_connect(global_cmcp_client,
        VSP_TEST_SERVER_SUBSCRIBE_ADDRESS, NULL);
    mu_assert(ret != 0, VSP_TEST_INVALID_PARAMETER_ACCEPTED);

    /* invalid client connect: address empty */
    ret = vsp_cmcp_client_connect(global_cmcp_client,
        "", VSP_TEST_SERVER_PUBLISH_ADDRESS);
    mu_assert(ret != 0, VSP_TEST_INVALID_PARAMETER_ACCEPTED);

    ret = vsp_cmcp_client_connect(global_cmcp_client,
        VSP_TEST_SERVER_SUBSCRIBE_ADDRESS, "");
    mu_assert(ret != 0, VSP_TEST_INVALID_PARAMETER_ACCEPTED);
}

MU_TEST(vsp_test_cmcp_connection_test)
{
    int ret;
    /* bind */
    ret = vsp_cmcp_server_bind(global_cmcp_server,
        VSP_TEST_SERVER_PUBLISH_ADDRESS, VSP_TEST_SERVER_SUBSCRIBE_ADDRESS);
    mu_assert(ret == 0, vsp_error_str(vsp_error_num()));
    /* connect */
    ret = vsp_cmcp_client_connect(global_cmcp_client,
        VSP_TEST_SERVER_SUBSCRIBE_ADDRESS, VSP_TEST_SERVER_PUBLISH_ADDRESS);
    mu_assert(ret == 0, vsp_error_str(vsp_error_num()));
}

MU_TEST_SUITE(vsp_test_cmcp_connection)
{
    MU_RUN_TEST(vsp_test_cmcp_server_allocation);
    MU_RUN_TEST(vsp_test_cmcp_client_allocation);
    MU_SUITE_CONFIGURE(&vsp_test_cmcp_connection_setup,
        &vsp_test_cmcp_connection_teardown);
    MU_RUN_TEST(vsp_test_cmcp_server_invalid_parameters);
    MU_RUN_TEST(vsp_test_cmcp_client_invalid_parameters);
    MU_RUN_TEST(vsp_test_cmcp_connection_test);
}
