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
#include <vesper_cmcp/vsp_cmcp_state.h>
#include <vesper_util/vsp_error.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>

#define VSP_TEST_CMCP_TIMEOUT 5000

/** Test states. */
typedef enum {
    /** Client is not connected to server. */
    VSP_TEST_CMCP_NOT_CONNECTED,
    /** Client is connected to server, main thread is waiting until message
     * callback is invoked. */
    VSP_TEST_CMCP_CONNECTED,
    /** Server message was received. */
    VSP_TEST_CMCP_SERVER_MESSAGE_RECEIVED,
    /** Client message was received. */
    VSP_TEST_CMCP_CLIENT_MESSAGE_RECEIVED,
    /** Client was disconnected from server. */
    VSP_TEST_CMCP_DISCONNECTED
} vsp_cmcp_client_state;

/** Global test state. */
vsp_cmcp_state *global_test_state;

/** Global CMCP server object. */
vsp_cmcp_server *global_cmcp_server;

/** Global CMCP client object. */
vsp_cmcp_client *global_cmcp_client;

/** Global CMCP client ID. */
uint16_t global_cmcp_client_id;

/** Client announcement callback function. */
int vsp_test_cmcp_announcement_cb(void *callback_param, uint16_t client_id);

/** Client disconnection callback function. */
void vsp_test_cmcp_disconnect_cb(void *callback_param, uint16_t client_id);

/** Server message callback function. */
void vsp_test_cmcp_server_message_cb(void *callback_param, uint16_t client_id,
    uint16_t command_id, vsp_cmcp_datalist *cmcp_datalist);

/** Client message callback function. */
void vsp_test_cmcp_client_message_cb(void *callback_param, uint16_t command_id,
    vsp_cmcp_datalist *cmcp_datalist);

/** Create global_cmcp_server and global_cmcp_client objects. */
void vsp_test_cmcp_connection_setup(void);

/** Initialize global_test_state object, create and connect
 * global_cmcp_server and global_cmcp_client objects. */
void vsp_test_cmcp_communication_setup(void);

/** Free global_cmcp_server and global_cmcp_client objects. */
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

/** Test connection and communication between vsp_cmcp_server and
 * vsp_cmcp_client. */
MU_TEST(vsp_test_cmcp_communication_test);

int vsp_test_cmcp_announcement_cb(void *callback_param, uint16_t client_id)
{
    /* check if callback parameter equals global server object */
    mu_assert_abort(callback_param == global_cmcp_server,
        vsp_error_str(EINVAL));
    /* check if client ID is valid */
    mu_assert_abort(client_id != VSP_CMCP_SERVER_BROADCAST_TOPIC_ID
        && client_id != VSP_CMCP_CLIENT_BROADCAST_TOPIC_ID
        && (client_id & 1) == 1, vsp_error_str(EINVAL));
    /* check if test state is correct */
    mu_assert_abort(vsp_cmcp_state_get(global_test_state)
        == VSP_TEST_CMCP_NOT_CONNECTED, vsp_error_str(EINVAL));
    /* store client ID */
    global_cmcp_client_id = client_id;
    /* update test state */
    vsp_cmcp_state_set(global_test_state, VSP_TEST_CMCP_CONNECTED);
    /* accept new client */
    return 0;
}

void vsp_test_cmcp_disconnect_cb(void *callback_param, uint16_t client_id)
{
    /* check if callback parameter equals global server object */
    mu_assert_abort(callback_param == global_cmcp_server,
        vsp_error_str(EINVAL));
    /* check if client ID is valid */
    mu_assert_abort(client_id == global_cmcp_client_id, vsp_error_str(EINVAL));
    /* check if test state is correct */
    mu_assert_abort(vsp_cmcp_state_get(global_test_state)
        == VSP_TEST_CMCP_CLIENT_MESSAGE_RECEIVED, vsp_error_str(EINVAL));
    /* update test state */
    vsp_cmcp_state_set(global_test_state, VSP_TEST_CMCP_DISCONNECTED);
}

void vsp_test_cmcp_server_message_cb(void *callback_param, uint16_t client_id,
    uint16_t command_id, vsp_cmcp_datalist *cmcp_datalist)
{
    void *data_item_pointer;

    /* check if callback parameter equals global server object */
    mu_assert_abort(callback_param == global_cmcp_server,
        vsp_error_str(EINVAL));
    /* check if client ID is valid */
    mu_assert_abort(client_id == global_cmcp_client_id, vsp_error_str(EINVAL));
    /* check if command ID is valid */
    mu_assert_abort(command_id == VSP_TEST_MESSAGE_COMMAND_ID,
        vsp_error_str(EINVAL));

    /* check if data list is valid */
    mu_assert_abort(cmcp_datalist != NULL, vsp_error_str(EINVAL));
    /* get data list item and verify data */
    data_item_pointer = vsp_cmcp_datalist_get_data_item(cmcp_datalist,
        VSP_TEST_DATALIST_ITEM2_ID, VSP_TEST_DATALIST_ITEM2_LENGTH);
    mu_assert_abort(data_item_pointer != NULL, vsp_error_str(vsp_error_num()));
    mu_assert(memcmp(data_item_pointer, VSP_TEST_DATALIST_ITEM2_DATA,
        VSP_TEST_DATALIST_ITEM2_LENGTH) == 0, vsp_error_str(EINVAL));

    /* check if test state is correct */
    mu_assert_abort(vsp_cmcp_state_get(global_test_state)
        == VSP_TEST_CMCP_SERVER_MESSAGE_RECEIVED, vsp_error_str(EINVAL));
    /* update test state */
    vsp_cmcp_state_set(global_test_state, VSP_TEST_CMCP_CLIENT_MESSAGE_RECEIVED);
}

void vsp_test_cmcp_client_message_cb(void *callback_param, uint16_t command_id,
    vsp_cmcp_datalist *cmcp_datalist)
{
    void *data_item_pointer;

    /* check if callback parameter equals global client object */
    mu_assert_abort(callback_param == global_cmcp_client,
        vsp_error_str(EINVAL));
    /* check if command ID is valid */
    mu_assert_abort(command_id == VSP_TEST_MESSAGE_COMMAND_ID,
        vsp_error_str(EINVAL));

    /* check if data list is valid */
    mu_assert_abort(cmcp_datalist != NULL, vsp_error_str(EINVAL));
    /* get data list item and verify data */
    data_item_pointer = vsp_cmcp_datalist_get_data_item(cmcp_datalist,
        VSP_TEST_DATALIST_ITEM1_ID, VSP_TEST_DATALIST_ITEM1_LENGTH);
    mu_assert_abort(data_item_pointer != NULL, vsp_error_str(vsp_error_num()));
    mu_assert(memcmp(data_item_pointer, VSP_TEST_DATALIST_ITEM1_DATA,
        VSP_TEST_DATALIST_ITEM1_LENGTH) == 0, vsp_error_str(EINVAL));

    /* check if test state is correct */
    mu_assert_abort(vsp_cmcp_state_get(global_test_state)
        == VSP_TEST_CMCP_CONNECTED, vsp_error_str(EINVAL));
    /* update test state */
    vsp_cmcp_state_set(global_test_state, VSP_TEST_CMCP_SERVER_MESSAGE_RECEIVED);
}

void vsp_test_cmcp_connection_setup(void)
{
    /* initialize test state to NULL */
    global_test_state = NULL;
    /* create server */
    global_cmcp_server = vsp_cmcp_server_create();
    mu_assert_abort(global_cmcp_server != NULL, vsp_error_str(vsp_error_num()));
    /* create client */
    global_cmcp_client = vsp_cmcp_client_create();
    mu_assert_abort(global_cmcp_client != NULL, vsp_error_str(vsp_error_num()));
}

void vsp_test_cmcp_communication_setup(void)
{
    int ret;

    /* create server and client */
    vsp_test_cmcp_connection_setup();

    /* initialize test state */
    global_test_state = vsp_cmcp_state_create(VSP_TEST_CMCP_NOT_CONNECTED);
    mu_assert_abort(global_test_state != NULL, vsp_error_str(vsp_error_num()));

    /* register server callback parameter */
    vsp_cmcp_server_set_callback_param(global_cmcp_server, global_cmcp_server);
    /* register server callback functions */
    vsp_cmcp_server_set_announcement_cb(global_cmcp_server,
        vsp_test_cmcp_announcement_cb);
    vsp_cmcp_server_set_disconnect_cb(global_cmcp_server,
        vsp_test_cmcp_disconnect_cb);
    vsp_cmcp_server_set_message_cb(global_cmcp_server,
        vsp_test_cmcp_server_message_cb);

    /* register client callback parameter */
    vsp_cmcp_client_set_callback_param(global_cmcp_client, global_cmcp_client);
    /* register client callback functions */
    vsp_cmcp_client_set_message_cb(global_cmcp_client,
        vsp_test_cmcp_client_message_cb);

    /* bind server */
    ret = vsp_cmcp_server_bind(global_cmcp_server,
        VSP_TEST_SERVER_PUBLISH_ADDRESS, VSP_TEST_SERVER_SUBSCRIBE_ADDRESS);
    mu_assert_abort(ret == 0, vsp_error_str(vsp_error_num()));
    /* connect client */
    ret = vsp_cmcp_client_connect(global_cmcp_client,
        VSP_TEST_SERVER_SUBSCRIBE_ADDRESS, VSP_TEST_SERVER_PUBLISH_ADDRESS);
    mu_assert_abort(ret == 0, vsp_error_str(vsp_error_num()));
}

void vsp_test_cmcp_connection_teardown(void)
{
    /* free client */
    if (global_cmcp_client != NULL) {
        vsp_cmcp_client_free(global_cmcp_client);
        global_cmcp_client = NULL;
    }
    /* free server */
    if (global_cmcp_server != NULL) {
        vsp_cmcp_server_free(global_cmcp_server);
        global_cmcp_server = NULL;
    }
    /* free test state if created */
    if (global_test_state != NULL) {
        vsp_cmcp_state_free(global_test_state);
        global_test_state = NULL;
    }
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

MU_TEST(vsp_test_cmcp_communication_test)
{
    int ret;
    vsp_cmcp_datalist *cmcp_datalist;
    struct timespec time_test_timeout;

    /* check if test state is correct */
    mu_assert_abort(vsp_cmcp_state_get(global_test_state)
        == VSP_TEST_CMCP_CONNECTED, vsp_error_str(EINVAL));

    /* create data list */
    cmcp_datalist = vsp_cmcp_datalist_create();
    mu_assert_abort(cmcp_datalist != NULL, vsp_error_str(vsp_error_num()));
    /* add a data list item */
    ret = vsp_cmcp_datalist_add_item(cmcp_datalist, VSP_TEST_DATALIST_ITEM1_ID,
        VSP_TEST_DATALIST_ITEM1_LENGTH, VSP_TEST_DATALIST_ITEM1_DATA);
    mu_assert_abort(ret == 0, vsp_error_str(vsp_error_num()));
    /* send server message */
    ret = vsp_cmcp_server_send(global_cmcp_server, global_cmcp_client_id,
        VSP_TEST_MESSAGE_COMMAND_ID, cmcp_datalist);
    mu_assert(ret == 0, vsp_error_str(vsp_error_num()));
    /* free data list */
    vsp_cmcp_datalist_free(cmcp_datalist);

    /* start measuring time for test timeout */
    vsp_time_real_timespec_from_now(&time_test_timeout,
        VSP_TEST_CMCP_TIMEOUT);

    /* lock state mutex */
    vsp_cmcp_state_lock(global_test_state);
    /* wait until client message received or waiting timed out */
    ret = vsp_cmcp_state_await_state(global_test_state,
        VSP_TEST_CMCP_SERVER_MESSAGE_RECEIVED, &time_test_timeout);
    /* unlock state mutex */
    vsp_cmcp_state_unlock(global_test_state);
    /* check if test was successful */
    mu_assert(ret == 0, vsp_error_str(ETIMEDOUT));

    /* create data list */
    cmcp_datalist = vsp_cmcp_datalist_create();
    mu_assert_abort(cmcp_datalist != NULL, vsp_error_str(vsp_error_num()));
    /* add a data list item */
    ret = vsp_cmcp_datalist_add_item(cmcp_datalist, VSP_TEST_DATALIST_ITEM2_ID,
        VSP_TEST_DATALIST_ITEM2_LENGTH, VSP_TEST_DATALIST_ITEM2_DATA);
    mu_assert_abort(ret == 0, vsp_error_str(vsp_error_num()));
    /* send server message */
    ret = vsp_cmcp_client_send(global_cmcp_client, VSP_TEST_MESSAGE_COMMAND_ID,
        cmcp_datalist);
    mu_assert(ret == 0, vsp_error_str(vsp_error_num()));
    /* free data list */
    vsp_cmcp_datalist_free(cmcp_datalist);

    /* disconnect client by freeing it */
    vsp_cmcp_client_free(global_cmcp_client);
    global_cmcp_client = NULL;

    /* start measuring time for test timeout */
    vsp_time_real_timespec_from_now(&time_test_timeout,
        VSP_TEST_CMCP_TIMEOUT);

    /* lock state mutex */
    vsp_cmcp_state_lock(global_test_state);
    /* wait until test completed or waiting timed out */
    ret = vsp_cmcp_state_await_state(global_test_state,
        VSP_TEST_CMCP_DISCONNECTED, &time_test_timeout);
    /* unlock state mutex */
    vsp_cmcp_state_unlock(global_test_state);
    /* check if test was successful */
    mu_assert(ret == 0, vsp_error_str(ETIMEDOUT));
}

MU_TEST_SUITE(vsp_test_cmcp_connection)
{
    MU_RUN_TEST(vsp_test_cmcp_server_allocation);
    MU_RUN_TEST(vsp_test_cmcp_client_allocation);

    MU_SUITE_CONFIGURE(&vsp_test_cmcp_connection_setup,
        &vsp_test_cmcp_connection_teardown);
    MU_RUN_TEST(vsp_test_cmcp_server_invalid_parameters);
    MU_RUN_TEST(vsp_test_cmcp_client_invalid_parameters);

    MU_SUITE_CONFIGURE(&vsp_test_cmcp_communication_setup,
        &vsp_test_cmcp_connection_teardown);
    MU_RUN_TEST(vsp_test_cmcp_communication_test);
}
