/*
 * Copyright (c) 2011-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_SYS___ASSERT_H_
#define ZEPHYR_INCLUDE_SYS___ASSERT_H_

#include <stdbool.h>

#define __ASSERT(test, fmt, ...) do { if (!(test)) printf("%s: %d: %s\n", __func__, __LINE__, fmt); } while (0)
#define __ASSERT_NO_MSG(test) __ASSERT(test, "")
#define __ASSERT_LOC(test) __ASSERT_NO_MSG(test)
#define __ASSERT_MSG_INFO(fmt, ...) __ASSERT(false, fmt, ##__VA_ARGS__)

#endif /* ZEPHYR_INCLUDE_SYS___ASSERT_H_ */
