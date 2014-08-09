/**
 * \file
 * \authors Max Mertens
 *
 * Copyright (c) 2014, Max Mertens. All rights reserved.
 * This file is licensed under the "BSD 3-Clause License".
 * Full license text is under the file "LICENSE" provided with this code.
 */

#if !defined VSP_RANDOM_H_INCLUDED
#define VSP_RANDOM_H_INCLUDED

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif /* defined __cplusplus */

/**
 * Get a pseudo-random 32 bit integer value.
 * This function is not thread-safe and should not be called by multiple threads
 * at the same time.
 * The pseudo-random number generator is implemented as a linear feedback shift
 * register, so this function never returns zero value (all bits cleared).
 */
uint32_t vsp_random_get(void);

#if defined __cplusplus
}
#endif /* defined __cplusplus */

#endif /* !defined VSP_RANDOM_H_INCLUDED */
