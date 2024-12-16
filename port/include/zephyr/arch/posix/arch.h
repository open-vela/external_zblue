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

#include <nuttx/clock.h>
#include <nuttx/irq.h>
#include <nuttx/wdog.h>

#include <pthread.h>
#include <semaphore.h>

#include <zephyr/arch/posix/thread.h>
#include <zephyr/devicetree.h>
#include <zephyr/arch/common/ffs.h>

#ifdef CONFIG_64BIT
#define ARCH_STACK_PTR_ALIGN 8
#else
#define ARCH_STACK_PTR_ALIGN 4
#endif

static inline uint32_t arch_k_cycle_get_32(void)
{
	return 0;
}

static inline uint64_t arch_k_cycle_get_64(void)
{
	return 0;
}

static ALWAYS_INLINE void arch_nop(void)
{
}

static ALWAYS_INLINE bool arch_irq_unlocked(unsigned int key)
{
	return false;
}

static ALWAYS_INLINE unsigned int arch_irq_lock(void)
{
	if (!up_interrupt_context()) {
		sched_lock();
	}

	return enter_critical_section();
}

static ALWAYS_INLINE void arch_irq_unlock(unsigned int key)
{
	if (!up_interrupt_context()) {
		sched_unlock();
	}

	leave_critical_section(key);
}

static inline unsigned int arch_num_cpus(void)
{
	return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_ARCH_POSIX_ARCH_H_ */
