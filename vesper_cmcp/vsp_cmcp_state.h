/**
 * \file
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#if !defined VSP_CMCP_STATE_H_INCLUDED
#define VSP_CMCP_STATE_H_INCLUDED

#include <vesper_util/vsp_time.h>

#if defined __cplusplus
extern "C" {
#endif /* defined __cplusplus */

/** State storage with mutex locking and waiting for change capabilities. */
struct vsp_cmcp_server;

/** Define type vsp_cmcp_state to avoid 'struct' keyword. */
typedef struct vsp_cmcp_state vsp_cmcp_state;

/**
 * Create new vsp_cmcp_state object.
 * The state is set to initial_state.
 * Returned pointer should be freed with vsp_cmcp_state_free().
 * Returns NULL and sets vsp_error_num() if failed.
 */
vsp_cmcp_state *vsp_cmcp_state_create(int initial_state);

/**
 * Free vsp_cmcp_state object.
 * Object should be created with vsp_cmcp_state_create().
 * Returns non-zero and sets vsp_error_num() if failed.
 */
void vsp_cmcp_state_free(vsp_cmcp_state *cmcp_state);

/**
 * Get the current state.
 * cmcp_state must not be NULL. Aborts if failed.
 */
int vsp_cmcp_state_get(vsp_cmcp_state *cmcp_state);

/**
 * Set the current state.
 * Locks the mutex while changing the state value, and signals waiting threads.
 */
void vsp_cmcp_state_set(vsp_cmcp_state *cmcp_state, int state);

/**
 * Wait and lock the current state.
 * cmcp_state must not be NULL. Aborts if failed.
 */
void vsp_cmcp_state_lock(vsp_cmcp_state *cmcp_state);

/**
 * Unlock the current state.
 * cmcp_state must not be NULL. Aborts if failed.
 */
void vsp_cmcp_state_unlock(vsp_cmcp_state *cmcp_state);

/**
 * Wait for the current state to change or the time to pass timeout_time.
 * The mutex can be locked before invoking this function, it will be
 * automatically unlocked by this function.
 * Returns positive value if timed out.
 * Returns negative value and sets vsp_error_num() if failed.
 */
int vsp_cmcp_state_wait(vsp_cmcp_state *cmcp_state,
    struct timespec *timeout_time);

#if defined __cplusplus
}
#endif /* defined __cplusplus */

#endif /* !defined VSP_CMCP_STATE_H_INCLUDED */