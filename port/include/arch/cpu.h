/* cpu.h - automatically selects the correct arch.h file to include */

/*
 * Copyright (c) 1997-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_ARCH_CPU_H_
#define ZEPHYR_INCLUDE_ARCH_CPU_H_

#define ARCH_STACK_PTR_ALIGN	16

/* Architecture thread structure */
struct _callee_saved {
};

typedef struct _callee_saved _callee_saved_t;

struct _thread_arch {
};

typedef struct _thread_arch _thread_arch_t;

typedef uint32_t z_arch_esf_t;

#include <devicetree.h>
#include <sys/arch_interface.h>
#include <arch/common/ffs.h>

#endif /* ZEPHYR_INCLUDE_ARCH_CPU_H_ */
