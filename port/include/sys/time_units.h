/*
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_TIME_UNITS_H_
#define ZEPHYR_INCLUDE_TIME_UNITS_H_

#ifdef __cplusplus
extern "C" {
#endif

#define SYS_FOREVER_MS (-1)
#define SYS_TIMEOUT_MS(ms) ((ms) == SYS_FOREVER_MS ? K_FOREVER : K_MSEC(ms))

static inline uint32_t k_ms_to_ticks_ceil32(uint32_t t)
{
  return MSEC2TICK(t);
}

static inline uint32_t k_ticks_to_ms_ceil32(uint32_t t)
{
  return TICK2MSEC(t);
}

uint64_t k_ticks_to_ms_floor64(uint64_t t);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ZEPHYR_INCLUDE_TIME_UNITS_H_ */
