/*
 * Copyright (c) 2018 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_SPINLOCK_H_
#define ZEPHYR_INCLUDE_SPINLOCK_H_

#include <sys/atomic.h>
#include <kernel_structs.h>

struct k_spinlock;

typedef int k_spinlock_key_t;

static ALWAYS_INLINE k_spinlock_key_t k_spin_lock(struct k_spinlock *l)
{
	spin_lock(&l->lock);
	return 0;
}

static ALWAYS_INLINE void k_spin_unlock(struct k_spinlock *l,
					k_spinlock_key_t key)
{
	spin_unlock(&l->lock);
}

static ALWAYS_INLINE void k_spin_release(struct k_spinlock *l)
{
}

#endif /* ZEPHYR_INCLUDE_SPINLOCK_H_ */
