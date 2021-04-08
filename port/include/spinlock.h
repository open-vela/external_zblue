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

static pthread_mutex_t g_null_mutex;
static pthread_mutex_t g_init_mutex = PTHREAD_MUTEX_INITIALIZER;

static ALWAYS_INLINE k_spinlock_key_t k_spin_lock(struct k_spinlock *l)
{
	pthread_mutexattr_t attr;

	pthread_mutex_lock(&g_init_mutex);
	if (!memcmp(&g_null_mutex, &l->mutex, sizeof(pthread_mutex_t))) {
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&l->mutex, &attr);
		pthread_mutexattr_destroy(&attr);
	}
	pthread_mutex_unlock(&g_init_mutex);

	pthread_mutex_lock(&l->mutex);
	return 0;
}

static ALWAYS_INLINE void k_spin_unlock(struct k_spinlock *l,
					k_spinlock_key_t key)
{
	pthread_mutex_unlock(&l->mutex);
}

static ALWAYS_INLINE void k_spin_release(struct k_spinlock *l)
{
	pthread_mutex_destroy(&l->mutex);
}

#endif /* ZEPHYR_INCLUDE_SPINLOCK_H_ */
