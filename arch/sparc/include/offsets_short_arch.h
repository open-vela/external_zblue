/*
 * Copyright (c) 2019-2020 Cobham Gaisler AB
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_ARCH_SPARC_INCLUDE_OFFSETS_SHORT_ARCH_H_
#define ZEPHYR_ARCH_SPARC_INCLUDE_OFFSETS_SHORT_ARCH_H_

#include <offsets.h>

#define _thread_offset_to_y \
	(___thread_t_callee_saved_OFFSET + ___callee_saved_t_y_OFFSET)

#define _thread_offset_to_l0_and_l1 \
	(___thread_t_callee_saved_OFFSET + ___callee_saved_t_l0_and_l1_OFFSET)

#define _thread_offset_to_l2 \
	(___thread_t_callee_saved_OFFSET + ___callee_saved_t_l2_OFFSET)

#define _thread_offset_to_l3 \
	(___thread_t_callee_saved_OFFSET + ___callee_saved_t_l3_OFFSET)

#define _thread_offset_to_l4 \
	(___thread_t_callee_saved_OFFSET + ___callee_saved_t_l4_OFFSET)

#define _thread_offset_to_l5 \
	(___thread_t_callee_saved_OFFSET + ___callee_saved_t_l5_OFFSET)

#define _thread_offset_to_l6 \
	(___thread_t_callee_saved_OFFSET + ___callee_saved_t_l6_OFFSET)

#define _thread_offset_to_l7 \
	(___thread_t_callee_saved_OFFSET + ___callee_saved_t_l7_OFFSET)

#define _thread_offset_to_i0 \
	(___thread_t_callee_saved_OFFSET + ___callee_saved_t_i0_OFFSET)

#define _thread_offset_to_i1 \
	(___thread_t_callee_saved_OFFSET + ___callee_saved_t_i1_OFFSET)

#define _thread_offset_to_i2 \
	(___thread_t_callee_saved_OFFSET + ___callee_saved_t_i2_OFFSET)

#define _thread_offset_to_i3 \
	(___thread_t_callee_saved_OFFSET + ___callee_saved_t_i3_OFFSET)

#define _thread_offset_to_i4 \
	(___thread_t_callee_saved_OFFSET + ___callee_saved_t_i4_OFFSET)

#define _thread_offset_to_i5 \
	(___thread_t_callee_saved_OFFSET + ___callee_saved_t_i5_OFFSET)

#define _thread_offset_to_i6 \
	(___thread_t_callee_saved_OFFSET + ___callee_saved_t_i6_OFFSET)

#define _thread_offset_to_i7 \
	(___thread_t_callee_saved_OFFSET + ___callee_saved_t_i7_OFFSET)

#define _thread_offset_to_o6 \
	(___thread_t_callee_saved_OFFSET + ___callee_saved_t_o6_OFFSET)

#define _thread_offset_to_o7 \
	(___thread_t_callee_saved_OFFSET + ___callee_saved_t_o7_OFFSET)

#define _thread_offset_to_psr \
	(___thread_t_callee_saved_OFFSET + ___callee_saved_t_psr_OFFSET)

#endif /* ZEPHYR_ARCH_SPARC_INCLUDE_OFFSETS_SHORT_ARCH_H_ */
