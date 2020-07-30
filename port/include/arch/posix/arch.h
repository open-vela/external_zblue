/*
 * Copyright (c) 2010-2014 Wind River Systems, Inc.
 * Copyright (c) 2017 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief POSIX arch specific kernel interface header
 * This header contains the POSIX arch specific kernel interface.
 * It is included by the generic kernel interface header (include/arch/cpu.h)
 *
 */

#ifndef ZEPHYR_INCLUDE_ARCH_POSIX_ARCH_H_
#define ZEPHYR_INCLUDE_ARCH_POSIX_ARCH_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_64BIT
#define ARCH_STACK_PTR_ALIGN 8
#else
#define ARCH_STACK_PTR_ALIGN 4
#endif

#include <arch/common/ffs.h>
#include <pthread.h>

static pthread_mutex_t g_irq_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

static inline unsigned int irq_lock(void)
{
	return pthread_mutex_lock(&g_irq_mutex);
}

static inline void irq_unlock(unsigned int key)
{
	pthread_mutex_unlock(&g_irq_mutex);
}

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_ARCH_POSIX_ARCH_H_ */
