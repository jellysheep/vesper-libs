/**
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#include "vsp_random.h"
#include "vsp_time.h"

#include <time.h>

#if defined(_WIN32)
  #include <Windows.h>
#else
  #include <sys/types.h>
  #include <unistd.h>
#endif

/** Initialize 32 bit seed for pseudo-random number generator. */
static void vsp_random_initialize(void);

/** Linear feedback shift value.
 * Must not be zero, so is set to zero until initialized. */
static uint32_t vsp_random_value = {0};

void vsp_random_initialize(void)
{
    uint64_t seed;
    double realtime;
    uint64_t realtime_int;
    uint64_t pid;

    if (vsp_random_value != 0) {
        return;
    }

    /* get random data from current time and process id */
    realtime = vsp_time_real();
    realtime_int = *(uint64_t*) &realtime;
    #if defined(_WIN32)
        pid = (uint64_t) GetCurrentProcessId();
    #else
        pid = (uint64_t) getpid();
    #endif
    /* mix random data into seed */
    seed = realtime_int;
    seed ^= pid;
    seed ^= (uint32_t) (seed >> 32);
    /* store seed */
    vsp_random_value = (uint32_t) seed;
}

uint32_t vsp_random_get(void)
{
    uint32_t bit;
    /* initialize generator seed */
    vsp_random_initialize();
    /* generate next linear feedback shift value bit */
    /* uses xor from bits 32, 22, 2, 1 */
    bit = ((vsp_random_value >> 0) ^ (vsp_random_value >> 10)
        ^ (vsp_random_value >> 30) ^ (vsp_random_value >> 31)) & 1;
    /* update and return value */
    vsp_random_value = (vsp_random_value >> 1) | (bit << 31);
    return vsp_random_value;
}
