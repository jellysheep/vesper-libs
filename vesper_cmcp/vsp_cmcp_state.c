/**
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#include "vsp_cmcp_state.h"

#include <vesper_util/vsp_error.h>
#include <vesper_util/vsp_util.h>
#include <pthread.h>

/** State storage with locking and waiting for change capabilities. */
struct vsp_cmcp_state {
    /** Finite state machine flag. */
    volatile int state;
    /** Mutex locking state changes. */
    pthread_mutex_t mutex;
    /** Condition variable used to wait for state changes. */
    pthread_cond_t condition;
};

/**
 * Wait for the current state to change or the time to pass timeout_time.
 * If timeout_time is NULL, this function does not time out.
 * Returns zero if succeeded and current state changed.
 * Returns non-zero and sets vsp_error_num() if timed out.
 */
int vsp_cmcp_state_wait(vsp_cmcp_state *cmcp_state,
    struct timespec *timeout_time);

vsp_cmcp_state *vsp_cmcp_state_create(int initial_state)
{
    vsp_cmcp_state *cmcp_state;
    /* allocate memory */
    VSP_ALLOC(cmcp_state, vsp_cmcp_state);
    /* initialize struct data */
    cmcp_state->state = initial_state;
    pthread_mutex_init(&cmcp_state->mutex, NULL);
    pthread_cond_init(&cmcp_state->condition, NULL);
    /* return struct pointer */
    return cmcp_state;
}

void vsp_cmcp_state_free(vsp_cmcp_state *cmcp_state)
{
    /* check parameter */
    VSP_ASSERT(cmcp_state != NULL);
    /* destroy mutex and condition variable */
    pthread_mutex_destroy(&cmcp_state->mutex);
    pthread_cond_destroy(&cmcp_state->condition);
    /* free memory */
    VSP_FREE(cmcp_state);
}

int vsp_cmcp_state_get(vsp_cmcp_state *cmcp_state)
{
    /* check parameter */
    VSP_ASSERT(cmcp_state != NULL);
    /* return state value */
    return cmcp_state->state;
}

void vsp_cmcp_state_set(vsp_cmcp_state *cmcp_state, int state)
{
    /* check parameter */
    VSP_ASSERT(cmcp_state != NULL);
    /* lock state mutex */
    pthread_mutex_lock(&cmcp_state->mutex);
    /* update state value */
    cmcp_state->state = state;
    /* signal waiting threads */
    pthread_cond_broadcast(&cmcp_state->condition);
    /* unlock state mutex */
    pthread_mutex_unlock(&cmcp_state->mutex);
}

void vsp_cmcp_state_lock(vsp_cmcp_state *cmcp_state)
{
    int ret;
    /* check parameter */
    VSP_ASSERT(cmcp_state != NULL);
    /* lock mutex */
    ret = pthread_mutex_lock(&cmcp_state->mutex);
    VSP_ASSERT(ret == 0);
}

void vsp_cmcp_state_unlock(vsp_cmcp_state *cmcp_state)
{
    int ret;
    /* check parameter */
    VSP_ASSERT(cmcp_state != NULL);
    /* unlock mutex */
    ret = pthread_mutex_unlock(&cmcp_state->mutex);
    VSP_ASSERT(ret == 0);
}

int vsp_cmcp_state_wait(vsp_cmcp_state *cmcp_state,
    struct timespec *timeout_time)
{
    int ret;
    int success;

    success = 0;
    if (timeout_time != NULL) {
        /* wait until thread is running, with timeout */
        ret = pthread_cond_timedwait(&cmcp_state->condition,
            &cmcp_state->mutex, timeout_time);
        if (ret == ETIMEDOUT) {
            vsp_error_set_num(ETIMEDOUT);
            success = -1;
        } else {
            VSP_ASSERT(ret == 0);
        }
    } else {
        /* wait until thread is running, without timeout */
        ret = pthread_cond_wait(&cmcp_state->condition, &cmcp_state->mutex);
        VSP_ASSERT(ret == 0);
    }
    /* success */
    return success;
}

int vsp_cmcp_state_await_state(vsp_cmcp_state *cmcp_state, int state,
    struct timespec *timeout_time)
{
    int ret;

    /* check parameters */
    VSP_ASSERT(cmcp_state != NULL);

    /* initialize local variable */
    ret = 0;
    /* loop until specified state is set or timed out */
    while (ret == 0 && cmcp_state->state != state) {
        ret = vsp_cmcp_state_wait(cmcp_state, timeout_time);
    }

    /* check if specified state is set */
    VSP_CHECK(cmcp_state->state == state,
        vsp_error_set_num(ETIMEDOUT); return -1);

    /* success */
    return 0;
}
