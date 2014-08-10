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
#include <vesper_util/vsp_time.h>
#include <vesper_util/vsp_util.h>
#include <stdio.h>
#include <string.h>

/** Check error number functions. */
MU_TEST(vsp_test_error_test);

/** Check output of pseudo-random number generator. */
MU_TEST(vsp_test_random_test);

/** Check time measurement functions. */
MU_TEST(vsp_test_time_test);

MU_TEST(vsp_test_error_test)
{
    int error;
    int ret;
    const char *error_str;

    error = EINVAL;
    /* set error number */
    vsp_error_set_num(error);
    /* get error number back */
    ret = vsp_error_num();
    mu_assert(ret == error, "Error value was not stored correctly.");
    /* get error string */
    error_str = vsp_error_str(error);
    mu_assert(error_str != NULL && strlen(error_str) > 0,
        "No error string returned.");
}

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

MU_TEST(vsp_test_time_test)
{
    double realtime_before, realtime_after;
    double cputime_before, cputime_after;

    realtime_before = vsp_time_real();
    cputime_before = vsp_time_cpu();

    do {
        realtime_after = vsp_time_real();
        cputime_after = vsp_time_cpu();
    } while (realtime_after == realtime_before ||
        cputime_after == cputime_before);

    /* check if duration is positive */
    mu_assert(realtime_after > realtime_before, "Real time values invalid.");
    mu_assert(cputime_after > cputime_before, "CPU time values invalid.");
}

MU_TEST_SUITE(vsp_test_util)
{
    MU_RUN_TEST(vsp_test_error_test);
    MU_RUN_TEST(vsp_test_random_test);
    MU_RUN_TEST(vsp_test_time_test);
}
