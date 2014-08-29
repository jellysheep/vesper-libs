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
    /* check parameter */
    VSP_ASSERT(cmcp_state != NULL);
    /* lock mutex */
    pthread_mutex_lock(&cmcp_state->mutex);
}

void vsp_cmcp_state_unlock(vsp_cmcp_state *cmcp_state)
{
    /* check parameter */
    VSP_ASSERT(cmcp_state != NULL);
    /* unlock mutex */
    pthread_mutex_unlock(&cmcp_state->mutex);
}

int vsp_cmcp_state_wait(vsp_cmcp_state *cmcp_state,
    struct timespec *timeout_time)
{
    int ret;
    /* check parameter */
    VSP_ASSERT(cmcp_state != NULL);
    if (timeout_time != NULL) {
        /* wait until thread is running, with timeout */
        ret = pthread_cond_timedwait(&cmcp_state->condition,
            &cmcp_state->mutex, timeout_time);
    } else {
        /* wait until thread is running, without timeout */
        ret = pthread_cond_wait(&cmcp_state->condition, &cmcp_state->mutex);
    }
    /* unlock mutex */
    pthread_mutex_unlock(&cmcp_state->mutex);
    /* check for condition waiting errors */
    VSP_ASSERT(ret == 0);
    /* success */
    return 0;
}
