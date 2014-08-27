/**
 * \file
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#if !defined VSP_TIME_H_INCLUDED
#define VSP_TIME_H_INCLUDED

#if defined __cplusplus
extern "C" {
#endif /* defined __cplusplus */

/** Forward-declare struct timeval to avoid including huge header files. */
struct timeval;

/**
 * Get real (wall clock) time since epoch as a timeval struct.
 * Aborts if no timer is available.
 */
void vsp_time_real_timeval(struct timeval *time);

/**
 * Get real (wall clock) time since epoch in seconds.
 * Aborts if no timer is available.
 */
double vsp_time_real_double(void);

/**
 * Get the amount of CPU time used by the current process in seconds.
 * Returns -1.0 if an error occurred.
 * Time is measured since an arbitrary and OS-dependent start time.
 * The returned real time is only useful for computing an elapsed time
 * between two calls to this function.
 */
double vsp_time_cpu_double(void);

#if defined __cplusplus
}
#endif /* defined __cplusplus */

#endif /* !defined VSP_TIME_H_INCLUDED */
