/**
 * \file
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#if !defined VSP_TEST_H_INCLUDED
#define VSP_TEST_H_INCLUDED

#if defined __cplusplus
extern "C" {
#endif /* defined __cplusplus */

/** Function did not fail when invoked with invalid (out of range) parameter. */
#define VSP_TEST_INVALID_PARAMETER_ACCEPTED "Invalid parameter was accepted."


/** Server publish socket address. */
#define VSP_TEST_SERVER_PUBLISH_ADDRESS "tcp://127.0.0.1:7571"
/** Server subscribe socket address. */
#define VSP_TEST_SERVER_SUBSCRIBE_ADDRESS "tcp://127.0.0.1:7572"


/** First data list item ID. */
#define VSP_TEST_DATALIST_ITEM1_ID 32349
/** Second data list item ID. */
#define VSP_TEST_DATALIST_ITEM2_ID 9273

/** First data list item length. */
#define VSP_TEST_DATALIST_ITEM1_LENGTH 6
/** Second data list item length. */
#define VSP_TEST_DATALIST_ITEM2_LENGTH 7

/** First data list item data. */
#define VSP_TEST_DATALIST_ITEM1_DATA "Hello"
/** Second data list item data. */
#define VSP_TEST_DATALIST_ITEM2_DATA "World!"


/** Message topic ID. */
#define VSP_TEST_MESSAGE_TOPIC_ID 28437
/** Message sender ID. */
#define VSP_TEST_MESSAGE_SENDER_ID 6391
/** Message command ID. Has to be lower than 2^15. */
#define VSP_TEST_MESSAGE_COMMAND_ID 27743


/** Test CMCP implementation. */
MU_TEST_SUITE(vsp_test_cmcp_connection);
/** Test data list implementation. */
MU_TEST_SUITE(vsp_test_cmcp_datalist);
/** Test CMCP message implementation. */
MU_TEST_SUITE(vsp_test_cmcp_message);
/** Test internal utility functions. */
MU_TEST_SUITE(vsp_test_util);

#if defined __cplusplus
}
#endif /* defined __cplusplus */

#endif /* !defined VSP_TEST_H_INCLUDED */
