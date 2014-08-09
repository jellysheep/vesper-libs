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

#include <vesper_util/vsp_error.h>
#include <vesper_util/vsp_random.h>
#include <vesper_util/vsp_util.h>
#include <stdio.h>

/** Check output of pseudo-random number generator. */
MU_TEST(vsp_test_random_test);

MU_TEST(vsp_test_random_test)
{
    uint32_t value1;
    uint32_t value2;
    uint32_t i;

    /* check if initial value is not zero */
    value1 = vsp_random_get();
    mu_assert(value1 != 0, "Random value must not be zero.");

    /* loop a few times and check shifting */
    for (i = 0; i < 4; i++) {
        value2 = vsp_random_get();
        mu_assert((value2 & ~(1 << 31)) == (value1 >> 1),
            "Random generator shifting not working.");
        value1 = value2;
    }
}

MU_TEST_SUITE(vsp_test_random)
{
    MU_RUN_TEST(vsp_test_random_test);
}
