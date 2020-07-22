/*
 * Copyright (c) 2019 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @defgroup arch-interface Architecture Interface
 * @brief Internal kernel APIs with public scope
 *
 * Any public kernel APIs that are implemented as inline functions and need to
 * call architecture-specific API so will have the prototypes for the
 * architecture-specific APIs here. Architecture APIs that aren't used in this
 * way go in kernel/include/kernel_arch_interface.h.
 *
 * The set of architecture-specific APIs used internally by public macros and
 * inline functions in public headers are also specified and documented.
 *
 * For all macros and inline function prototypes described herein, <arch/cpu.h>
 * must eventually pull in full definitions for all of them (the actual macro
 * defines and inline function bodies)
 *
 * include/kernel.h and other public headers depend on definitions in this
 * header.
 */
#ifndef ZEPHYR_INCLUDE_SYS_ARCH_INTERFACE_H_
#define ZEPHYR_INCLUDE_SYS_ARCH_INTERFACE_H_

#include <toolchain.h>
#include <stddef.h>
#include <zephyr/types.h>
#include <arch/cpu.h>

#ifdef __cplusplus
extern "C" {
#endif

struct k_thread;
typedef struct z_thread_stack_element k_thread_stack_t;
typedef void (*k_thread_entry_t)(void *p1, void *p2, void *p3);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ZEPHYR_INCLUDE_SYS_ARCH_INTERFACE_H_ */
