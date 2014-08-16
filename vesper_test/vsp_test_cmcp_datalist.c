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
#include <vesper_util/vsp_error.h>
#include <vesper_util/vsp_util.h>
#include <err.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>

/** Global data list object. */
vsp_cmcp_datalist *global_cmcp_datalist;

/** Create global global_cmcp_datalist object. */
void vsp_test_cmcp_datalist_setup(void);

/** Free global global_cmcp_datalist object. */
void vsp_test_cmcp_datalist_teardown(void);

/** Create data list and test adding and reading items. */
MU_TEST(vsp_test_cmcp_datalist_test);

void vsp_test_cmcp_datalist_setup(void)
{
    /* allocation */
    global_cmcp_datalist = vsp_cmcp_datalist_create();
    mu_assert_abort(global_cmcp_datalist != NULL,
        vsp_error_str(vsp_error_num()));
}

void vsp_test_cmcp_datalist_teardown(void)
{
    int ret;
    /* deallocation */
    ret = vsp_cmcp_datalist_free(global_cmcp_datalist);
    mu_assert(ret == 0, vsp_error_str(vsp_error_num()));
}

MU_TEST(vsp_test_cmcp_datalist_test)
{
    vsp_cmcp_datalist *cmcp_datalist2;
    int ret;
    int data_length;
    void *data_pointer;
    void *data_item_pointer;

    /* insert data list items */
    ret = vsp_cmcp_datalist_add_item(global_cmcp_datalist,
        VSP_TEST_DATALIST_ITEM1_ID,
        VSP_TEST_DATALIST_ITEM1_LENGTH, VSP_TEST_DATALIST_ITEM1_DATA);
    mu_assert(ret == 0, vsp_error_str(vsp_error_num()));

    ret = vsp_cmcp_datalist_add_item(global_cmcp_datalist,
        VSP_TEST_DATALIST_ITEM2_ID,
        VSP_TEST_DATALIST_ITEM2_LENGTH, VSP_TEST_DATALIST_ITEM2_DATA);
    mu_assert(ret == 0, vsp_error_str(vsp_error_num()));

    /* get binary data array length */
    data_length = vsp_cmcp_datalist_get_data_length(global_cmcp_datalist);
    mu_assert(data_length ==
        (VSP_TEST_DATALIST_ITEM1_LENGTH + VSP_TEST_DATALIST_ITEM2_LENGTH + 8),
        vsp_error_str(EINVAL));
    /* allocate array */
    data_pointer = malloc(data_length);
    mu_assert_abort(data_pointer != NULL, vsp_error_str(ENOMEM));
    /* get binary data */
    ret = vsp_cmcp_datalist_get_data(global_cmcp_datalist, data_pointer);
    mu_assert(ret == 0, vsp_error_str(vsp_error_num()));

    /* construct second data list using binary data array */
    cmcp_datalist2 = vsp_cmcp_datalist_create_parse(data_length, data_pointer);
    mu_assert_abort(cmcp_datalist2 != NULL, vsp_error_str(vsp_error_num()));

    /* insert data ID a second time and check for rejection */
    ret = vsp_cmcp_datalist_add_item(cmcp_datalist2, VSP_TEST_DATALIST_ITEM1_ID,
        VSP_TEST_DATALIST_ITEM1_LENGTH, VSP_TEST_DATALIST_ITEM1_DATA);
    mu_assert(ret != 0, vsp_error_str(EINVAL));

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

    /* get back item data with wrong length and check for failure */
    data_item_pointer = vsp_cmcp_datalist_get_data_item(cmcp_datalist2,
        VSP_TEST_DATALIST_ITEM1_ID, VSP_TEST_DATALIST_ITEM1_LENGTH + 1);
    mu_assert(data_item_pointer == NULL, vsp_error_str(EINVAL));

    /* get back item data for unknown item ID and check for failure */
    data_item_pointer = vsp_cmcp_datalist_get_data_item(cmcp_datalist2,
        VSP_TEST_DATALIST_ITEM2_ID + 1, VSP_TEST_DATALIST_ITEM2_LENGTH);
    mu_assert(data_item_pointer == NULL, vsp_error_str(EINVAL));

    /* deallocation */
    VSP_FREE(data_pointer);
    ret = vsp_cmcp_datalist_free(cmcp_datalist2);
    mu_assert(ret == 0, vsp_error_str(vsp_error_num()));
}

MU_TEST_SUITE(vsp_test_cmcp_datalist)
{
    MU_SUITE_CONFIGURE(&vsp_test_cmcp_datalist_setup,
        &vsp_test_cmcp_datalist_teardown);
    MU_RUN_TEST(vsp_test_cmcp_datalist_test);
}
