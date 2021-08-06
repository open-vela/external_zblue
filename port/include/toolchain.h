/*
 * Copyright (c) 2010-2014, Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Macros to abstract toolchain specific capabilities
 *
 * This file contains various macros to abstract compiler capabilities that
 * utilize toolchain specific attributes and/or pragmas.
 */

#ifndef ZEPHYR_INCLUDE_TOOLCHAIN_H_
#define ZEPHYR_INCLUDE_TOOLCHAIN_H_

#ifdef __has_builtin
	#define HAS_BUILTIN(x) __has_builtin(x)
#else
	#define HAS_BUILTIN(x) HAS_BUILTIN_##x
#endif

#ifndef __clang__
#define __clang__ 0
#endif

#include <toolchain/gcc.h>

#endif /* ZEPHYR_INCLUDE_TOOLCHAIN_H_ */
