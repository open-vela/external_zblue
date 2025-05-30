/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 *
 * This module provides the atomic operators for that relies on
 * spinlock.h
 *
 */

#include <zephyr/spinlock.h>
#include <zephyr/sys/atomic.h>

static struct k_spinlock lock;

void *atomic_ptr_set(atomic_ptr_t *target, void *value)
{
	k_spinlock_key_t key;
	void *ret;

	key = k_spin_lock(&lock);

	ret = (void *)*target;
	*target = (atomic_ptr_t)value;

	k_spin_unlock(&lock, key);

	return ret;
}

atomic_val_t atomic_nand(atomic_t *target, atomic_val_t value)
{
	k_spinlock_key_t key;
	atomic_val_t ret;

	key = k_spin_lock(&lock);

	ret = *target;
	*target = ~(*target & value);

	k_spin_unlock(&lock, key);

	return ret;
}
