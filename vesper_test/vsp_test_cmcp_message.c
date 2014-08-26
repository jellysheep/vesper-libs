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

#include <vesper_cmcp/vsp_cmcp_datalist.h>
#include <vesper_cmcp/vsp_cmcp_message.h>
#include <vesper_util/vsp_error.h>
#include <vesper_util/vsp_util.h>
#include <string.h>

/** Test to invoke message functions with invalid parameters and check if they
 * behave correctly and do not abort. */
MU_TEST(vsp_test_cmcp_message_invalid_parameters);

/** Create message and test creating binary data and parse it. */
MU_TEST(vsp_test_cmcp_message_test);

MU_TEST(vsp_test_cmcp_message_invalid_parameters)
{
    int ret;
    vsp_cmcp_message *cmcp_message1;
    vsp_cmcp_datalist *cmcp_datalist1, *cmcp_datalist2;
    uint8_t buffer[VSP_CMCP_MESSAGE_HEADER_LENGTH] = {0};
    uint16_t id;

    /* invalid message deallocation */
    ret = vsp_cmcp_message_free(NULL);
    mu_assert(ret != 0, VSP_TEST_INVALID_PARAMETER_ACCEPTED);

    /* parse message from NULL data */
    cmcp_message1 = vsp_cmcp_message_create_parse(
        VSP_CMCP_MESSAGE_HEADER_LENGTH, NULL);
    mu_assert(cmcp_message1 == NULL, VSP_TEST_INVALID_PARAMETER_ACCEPTED);

    /* parse message from too small buffer size */
    cmcp_message1 = vsp_cmcp_message_create_parse(
        VSP_CMCP_MESSAGE_HEADER_LENGTH - 1, buffer);
    mu_assert(cmcp_message1 == NULL, VSP_TEST_INVALID_PARAMETER_ACCEPTED);

    /* get message data length from NULL object */
    ret = vsp_cmcp_message_get_data_length(NULL);
    mu_assert(ret < 0, VSP_TEST_INVALID_PARAMETER_ACCEPTED);

    /* get message data from NULL object */
    ret = vsp_cmcp_message_get_data(NULL, buffer);
    mu_assert(ret != 0, VSP_TEST_INVALID_PARAMETER_ACCEPTED);

    /* get message id from NULL object */
    id = vsp_cmcp_message_get_topic_id(NULL);
    mu_assert(id == (uint16_t) -1, VSP_TEST_INVALID_PARAMETER_ACCEPTED);

    /* get data list from NULL object */
    cmcp_datalist1 = vsp_cmcp_message_get_datalist(NULL);
    mu_assert(cmcp_datalist1 == NULL, VSP_TEST_INVALID_PARAMETER_ACCEPTED);

    /* parse message */
    cmcp_message1 = vsp_cmcp_message_create_parse(
        VSP_CMCP_MESSAGE_HEADER_LENGTH, buffer);
    mu_assert_abort(cmcp_message1 != NULL, vsp_error_str(vsp_error_num()));

    /* get message data length from parsed message */
    ret = vsp_cmcp_message_get_data_length(cmcp_message1);
    mu_assert(ret < 0, VSP_TEST_INVALID_PARAMETER_ACCEPTED);

    /* get message data from parsed message */
    ret = vsp_cmcp_message_get_data(cmcp_message1, buffer);
    mu_assert(ret != 0, VSP_TEST_INVALID_PARAMETER_ACCEPTED);

    /* deallocate message */
    ret = vsp_cmcp_message_free(cmcp_message1);
    mu_assert(ret == 0, vsp_error_str(vsp_error_num()));

    /* create data list */
    cmcp_datalist1 = vsp_cmcp_datalist_create();
    mu_assert_abort(cmcp_datalist1 != NULL, vsp_error_str(vsp_error_num()));

    /* create message */
    cmcp_message1 = vsp_cmcp_message_create(0, 0, 0, cmcp_datalist1);
    mu_assert_abort(cmcp_message1 != NULL, vsp_error_str(vsp_error_num()));

    /* get message data to NULL buffer */
    ret = vsp_cmcp_message_get_data(cmcp_message1, NULL);
    mu_assert(ret != 0, VSP_TEST_INVALID_PARAMETER_ACCEPTED);

    /* get data list from created message */
    cmcp_datalist2 = vsp_cmcp_message_get_datalist(cmcp_message1);
    mu_assert(cmcp_datalist2 == NULL, VSP_TEST_INVALID_PARAMETER_ACCEPTED);

    /* deallocate message */
    ret = vsp_cmcp_message_free(cmcp_message1);
    mu_assert(ret == 0, vsp_error_str(vsp_error_num()));

    /* deallocate data list */
    ret = vsp_cmcp_datalist_free(cmcp_datalist1);
    mu_assert(ret == 0, vsp_error_str(vsp_error_num()));
}

MU_TEST(vsp_test_cmcp_message_test)
{
    vsp_cmcp_datalist *cmcp_datalist1, *cmcp_datalist2;
    vsp_cmcp_message *cmcp_message1, *cmcp_message2;
    int ret;
    int data_length;
    void *data_pointer;
    void *data_item_pointer;
    uint16_t message_id;

    /* allocate message without data list */
    cmcp_message1 = vsp_cmcp_message_create(VSP_TEST_MESSAGE_TOPIC_ID,
        VSP_TEST_MESSAGE_SENDER_ID, VSP_TEST_MESSAGE_COMMAND_ID, NULL);
    mu_assert_abort(cmcp_message1 != NULL, vsp_error_str(vsp_error_num()));
    /* get binary data array length */
    data_length = vsp_cmcp_message_get_data_length(cmcp_message1);
    mu_assert(data_length == VSP_CMCP_MESSAGE_HEADER_LENGTH,
        vsp_error_str(EINVAL));
    /* free message */
    ret = vsp_cmcp_message_free(cmcp_message1);
    mu_assert(ret == 0, vsp_error_str(vsp_error_num()));

    /* allocate data list */
    cmcp_datalist1 = vsp_cmcp_datalist_create();
    mu_assert_abort(cmcp_datalist1 != NULL, vsp_error_str(vsp_error_num()));
    /* insert data list items */
    ret = vsp_cmcp_datalist_add_item(cmcp_datalist1, VSP_TEST_DATALIST_ITEM1_ID,
        VSP_TEST_DATALIST_ITEM1_LENGTH, VSP_TEST_DATALIST_ITEM1_DATA);
    mu_assert(ret == 0, vsp_error_str(vsp_error_num()));
    ret = vsp_cmcp_datalist_add_item(cmcp_datalist1, VSP_TEST_DATALIST_ITEM2_ID,
        VSP_TEST_DATALIST_ITEM2_LENGTH, VSP_TEST_DATALIST_ITEM2_DATA);
    mu_assert(ret == 0, vsp_error_str(vsp_error_num()));

    /* allocate message */
    cmcp_message1 = vsp_cmcp_message_create(VSP_TEST_MESSAGE_TOPIC_ID,
        VSP_TEST_MESSAGE_SENDER_ID, VSP_TEST_MESSAGE_COMMAND_ID,
        cmcp_datalist1);

    /* get binary data array length */
    data_length = vsp_cmcp_message_get_data_length(cmcp_message1);
    mu_assert_abort(data_length ==
        (VSP_TEST_DATALIST_ITEM1_LENGTH + VSP_TEST_DATALIST_ITEM2_LENGTH
        + 8 + VSP_CMCP_MESSAGE_HEADER_LENGTH), vsp_error_str(EINVAL));
    /* allocate array */
    data_pointer = malloc(data_length);
    mu_assert_abort(data_pointer != NULL, vsp_error_str(ENOMEM));
    /* get binary data */
    ret = vsp_cmcp_message_get_data(cmcp_message1, data_pointer);
    mu_assert(ret == 0, vsp_error_str(vsp_error_num()));

    /* construct second message using binary data array */
    cmcp_message2 = vsp_cmcp_message_create_parse(data_length, data_pointer);
    mu_assert_abort(cmcp_message2 != NULL, vsp_error_str(vsp_error_num()));

    /* get back and verify message IDs */
    message_id = vsp_cmcp_message_get_topic_id(cmcp_message2);
    mu_assert(message_id != (uint16_t) -1, vsp_error_str(vsp_error_num()));
    mu_assert(message_id == VSP_TEST_MESSAGE_TOPIC_ID, vsp_error_str(EINVAL));

    message_id = vsp_cmcp_message_get_sender_id(cmcp_message2);
    mu_assert(message_id != (uint16_t) -1, vsp_error_str(vsp_error_num()));
    mu_assert(message_id == VSP_TEST_MESSAGE_SENDER_ID, vsp_error_str(EINVAL));

    message_id = vsp_cmcp_message_get_command_id(cmcp_message2);
    mu_assert(message_id != (uint16_t) -1, vsp_error_str(vsp_error_num()));
    mu_assert(message_id == VSP_TEST_MESSAGE_COMMAND_ID, vsp_error_str(EINVAL));

    /* get data list from second message */
    cmcp_datalist2 = vsp_cmcp_message_get_datalist(cmcp_message2);
    mu_assert_abort(cmcp_datalist2 != NULL, vsp_error_str(vsp_error_num()));

    /* get back data list item and verify data */
    data_item_pointer = vsp_cmcp_datalist_get_data_item(cmcp_datalist2,
        VSP_TEST_DATALIST_ITEM1_ID, VSP_TEST_DATALIST_ITEM1_LENGTH);
    mu_assert_abort(data_item_pointer != NULL, vsp_error_str(vsp_error_num()));
    mu_assert(memcmp(data_item_pointer, VSP_TEST_DATALIST_ITEM1_DATA,
        VSP_TEST_DATALIST_ITEM1_LENGTH) == 0, vsp_error_str(EINVAL));

    data_item_pointer = vsp_cmcp_datalist_get_data_item(cmcp_datalist2,
        VSP_TEST_DATALIST_ITEM2_ID, VSP_TEST_DATALIST_ITEM2_LENGTH);
    mu_assert_abort(data_item_pointer != NULL, vsp_error_str(vsp_error_num()));
    mu_assert(memcmp(data_item_pointer, VSP_TEST_DATALIST_ITEM2_DATA,
        VSP_TEST_DATALIST_ITEM2_LENGTH) == 0, vsp_error_str(EINVAL));

    /* deallocation; cmcp_datalist2 will be freed by cmcp_message2 */
    VSP_FREE(data_pointer);
    ret = vsp_cmcp_datalist_free(cmcp_datalist1);
    mu_assert(ret == 0, vsp_error_str(vsp_error_num()));
    ret = vsp_cmcp_message_free(cmcp_message1);
    mu_assert(ret == 0, vsp_error_str(vsp_error_num()));
    ret = vsp_cmcp_message_free(cmcp_message2);
    mu_assert(ret == 0, vsp_error_str(vsp_error_num()));
}

MU_TEST_SUITE(vsp_test_cmcp_message)
{
    MU_RUN_TEST(vsp_test_cmcp_message_invalid_parameters);
    MU_RUN_TEST(vsp_test_cmcp_message_test);
}
