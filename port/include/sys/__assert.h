/*
 * Copyright (c) 2011-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_SYS___ASSERT_H_
#define ZEPHYR_INCLUDE_SYS___ASSERT_H_

#include <stdbool.h>

#define __ASSERT(test, fmt, ...) \
  do { \
    if (!(test)) { \
      printk(fmt"\n", ##__VA_ARGS__); \
      k_panic(); \
    } \
  } while (0)

#define __ASSERT_NO_MSG(test) \
  do { \
    if (!(test)) { \
      printk("%s: %d: ", __func__, __LINE__); \
      k_panic(); \
    } \
  } while (0)

#define __ASSERT_LOC(test)          printk("FAIL @ %s:%d\n", __FILE__, __LINE__)
#define __ASSERT_MSG_INFO(fmt, ...) printk(fmt, ##__VA_ARGS__)

#endif /* ZEPHYR_INCLUDE_SYS___ASSERT_H_ */
