/*
 * Copyright (c) 2012 David Siñuela Pastor, siu.4coders@gmail.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef __MINUNIT_H__
#define __MINUNIT_H__

#include <vesper_util/vsp_time.h>

#ifdef __cplusplus
    extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>

/* Maximum length of last message */
#define MINUNIT_MESSAGE_LEN 1024

/* Misc. counters */
extern int minunit_run;
extern int minunit_assert;
extern int minunit_fail;
extern int minunit_status;

/* Timers */
extern double minunit_real_timer;
extern double minunit_proc_timer;

/* Last message */
extern char minunit_last_message[MINUNIT_MESSAGE_LEN];

/* Test setup and teardown function pointers */
extern void (*minunit_setup)(void);
extern void (*minunit_teardown)(void);

/* Definitions */
#define MU_TEST(method_name) void method_name(void)
#define MU_TEST_SUITE(suite_name) void suite_name(void)

#define MU__SAFE_BLOCK(block) do {\
    block\
} while(0)

/* Run test suite and unset setup and teardown functions */
#define MU_RUN_SUITE(suite_name) MU__SAFE_BLOCK(\
    suite_name();\
    minunit_setup = NULL;\
    minunit_teardown = NULL;\
)

/* Configure setup and teardown functions */
#define MU_SUITE_CONFIGURE(setup_fun, teardown_fun) MU__SAFE_BLOCK(\
    minunit_setup = setup_fun;\
    minunit_teardown = teardown_fun;\
)

/* Test runner */
#define MU_RUN_TEST(test) MU__SAFE_BLOCK(\
    if (minunit_real_timer==0 && minunit_real_timer==0) {\
        minunit_real_timer = vsp_time_real_double();\
        minunit_proc_timer = vsp_time_cpu_double();\
    }\
    if (minunit_setup) (*minunit_setup)();\
    minunit_status = 0;\
    test();\
    minunit_run++;\
    if (minunit_status) {\
        minunit_fail++;\
        printf("F");\
        printf("\n%s\n", minunit_last_message);\
    }\
    fflush(stdout);\
    if (minunit_teardown) (*minunit_teardown)();\
)

/* Report */
#define MU_REPORT() MU__SAFE_BLOCK(\
    double minunit_end_real_timer;\
    double minunit_end_proc_timer;\
    printf("\n\n%d tests, %d assertions, %d failures\n", minunit_run, minunit_assert, minunit_fail);\
    minunit_end_real_timer = vsp_time_real_double();\
    minunit_end_proc_timer = vsp_time_cpu_double();\
    printf("\nFinished in %.8f seconds (real) %.8f seconds (proc)\n\n",\
        minunit_end_real_timer - minunit_real_timer,\
        minunit_end_proc_timer - minunit_proc_timer);\
)

/* Assertions */
#define mu_assert_abort(test, message) MU__SAFE_BLOCK(\
    minunit_assert++;\
    if (!(test)) {\
        snprintf(minunit_last_message, MINUNIT_MESSAGE_LEN, "%s failed:\n\t%s:%d: %s", __func__, __FILE__, __LINE__, message);\
        minunit_status = 1;\
        abort();\
    } else {\
        printf(".");\
    }\
)

#define mu_assert(test, message) MU__SAFE_BLOCK(\
    minunit_assert++;\
    if (!(test)) {\
        snprintf(minunit_last_message, MINUNIT_MESSAGE_LEN, "%s failed:\n\t%s:%d: %s", __func__, __FILE__, __LINE__, message);\
        minunit_status = 1;\
    } else {\
        printf(".");\
    }\
)

#ifdef __cplusplus
}
#endif

#endif /* __MINUNIT_H__ */
