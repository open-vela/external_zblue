/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_SYS_ATOMIC_PORT_H_
#define ZEPHYR_INCLUDE_SYS_ATOMIC_PORT_H_

void *atomic_ptr_set(atomic_ptr_t *target, void *value);
atomic_val_t atomic_nand(atomic_t *target, atomic_val_t value);

#endif