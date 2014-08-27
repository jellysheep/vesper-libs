/**
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#include "vsp_time.h"
#include "vsp_util.h"

#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) \
    || defined(unix) || (defined(__APPLE__) && defined(__MACH__)))

  #include <unistd.h> /* POSIX flags */
  #include <sys/resource.h>
  #include <sys/times.h>
  #include <time.h>

#elif !defined(_WIN32)
  #error "Unable to define timers for an unknown OS."
#endif


/** FILETIME of Jan 1 1970 00:00:00. */
#define VSP_TIME_EPOCH_FILETIME 116444736000000000ULL

void vsp_time_real_timespec(struct timespec *time)
{
#if defined(_WIN32)
    FILETIME tm;
    ULONGLONG t;
    /* check parameter */
    VSP_ASSERT(time != NULL, abort());
    #if defined(NTDDI_WIN8) && NTDDI_VERSION >= NTDDI_WIN8
        /* Windows 8, Windows Server 2012 and later */
        GetSystemTimePreciseAsFileTime(&tm);
    #else
        /* Windows 2000 and later */
        GetSystemTimeAsFileTime(&tm);
    #endif
    t = ((ULONGLONG)tm.dwHighDateTime << 32) | (ULONGLONG)tm.dwLowDateTime;
    t -= VSP_TIME_EPOCH_FILETIME;
    time->tv_sec = t / 10000000;
    time->tv_nsec = (t % 10000000) * 100;
    return;

#elif defined(_POSIX_VERSION)
    /* POSIX */
    struct timeval tv;
    /* check parameter */
    VSP_ASSERT(time != NULL, abort());
    #if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L \
        && defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0)
    {
        #if defined(CLOCK_MONOTONIC_PRECISE)
            /* BSD */
            const clockid_t id = CLOCK_MONOTONIC_PRECISE;
        #elif defined(CLOCK_MONOTONIC_RAW)
            /* Linux */
            const clockid_t id = CLOCK_MONOTONIC_RAW;
        #elif defined(CLOCK_HIGHRES)
            /* Solaris */
            const clockid_t id = CLOCK_HIGHRES;
        #elif defined(CLOCK_MONOTONIC)
            /* AIX, BSD, Linux, POSIX, Solaris */
            const clockid_t id = CLOCK_MONOTONIC;
        #elif defined(CLOCK_REALTIME)
            /* AIX, BSD, HP-UX, Linux, POSIX */
            const clockid_t id = CLOCK_REALTIME;
        #else
            const clockid_t id = (clockid_t)-1; /* unknown */
        #endif /* defined(CLOCK_MONOTONIC_PRECISE) */
        if (id != (clockid_t)-1 && clock_gettime(id, time) != -1) {
            return;
        }
        /* fall through */
    }
    #endif /* defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0) */

    /* AIX, BSD, Cygwin, HP-UX, Linux, OSX, POSIX, Solaris */
    gettimeofday(&tv, NULL);
    time->tv_sec = tv.tv_sec;
    time->tv_nsec = tv.tv_usec * 1000;
    return;
#else
    abort(); /* failed */
#endif /* defined(_WIN32) */
}

double vsp_time_real_double(void)
{
    struct timespec time;
    vsp_time_real_timespec(&time);
    return ((double) time.tv_sec) + ((double) time.tv_nsec / 1000000000.0);
}

/*
 * The following function was written by David Robert Nadeau
 * from http://NadeauSoftware.com/ and distributed under the
 * Creative Commons Attribution 3.0 Unported License.
 */

double vsp_time_cpu_double()
{
#if defined(_WIN32)
    /* Windows */
    FILETIME createTime;
    FILETIME exitTime;
    FILETIME kernelTime;
    FILETIME userTime;
    if (GetProcessTimes(GetCurrentProcess(),
        &createTime, &exitTime, &kernelTime, &userTime) != -1) {
        SYSTEMTIME userSystemTime;
        if (FileTimeToSystemTime(&userTime, &userSystemTime) != -1) {
            return (double)userSystemTime.wHour * 3600.0 +
                (double)userSystemTime.wMinute * 60.0 +
                (double)userSystemTime.wSecond +
                (double)userSystemTime.wMilliseconds / 1000.0;
        }
    }

#elif defined(__unix__) || defined(__unix) || defined(unix) \
    || (defined(__APPLE__) && defined(__MACH__))
    /* AIX, BSD, Cygwin, HP-UX, Linux, OSX, and Solaris */

    #if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L \
        && defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0)
    /* prefer high-res POSIX timers, when available */
    {
        clockid_t id;
        struct timespec ts;
        #if _POSIX_CPUTIME > 0
            /* clock ids vary by OS. Query the id, if possible */
            if (clock_getcpuclockid(0, &id) == -1)
        #endif /* _POSIX_CPUTIME > 0 */
            {
        #if defined(CLOCK_PROCESS_CPUTIME_ID)
                /* use known clock id for AIX, Linux, or Solaris */
                id = CLOCK_PROCESS_CPUTIME_ID;
        #elif defined(CLOCK_VIRTUAL)
                /* use known clock id for BSD or HP-UX */
                id = CLOCK_VIRTUAL;
        #else
                id = (clockid_t)-1;
        #endif /* defined(CLOCK_PROCESS_CPUTIME_ID) */
            }
            if (id != (clockid_t)-1 && clock_gettime(id, &ts) != -1) {
                return (double)ts.tv_sec +
                    (double)ts.tv_nsec / 1000000000.0;
            }
        }
    #endif /* defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0) */

    #if defined(RUSAGE_SELF)
    {
        struct rusage rusage;
        if (getrusage(RUSAGE_SELF, &rusage) != -1) {
            return (double)rusage.ru_utime.tv_sec +
                (double)rusage.ru_utime.tv_usec / 1000000.0;
        }
    }
    #endif /* defined(RUSAGE_SELF) */

    #if defined(_SC_CLK_TCK)
    {
        const double ticks = (double)sysconf(_SC_CLK_TCK);
        struct tms tms;
        if (times(&tms) != (clock_t)-1) {
            return (double)tms.tms_utime / ticks;
        }
    }
    #endif /* defined(_SC_CLK_TCK) */

    #if defined(CLOCKS_PER_SEC)
    {
        clock_t cl = clock();
        if (cl != (clock_t)-1) {
            return (double)cl / (double)CLOCKS_PER_SEC;
        }
    }
    #endif /* defined(CLOCKS_PER_SEC) */

#endif /* defined(_WIN32) */

    return -1;      /* failed */
}
