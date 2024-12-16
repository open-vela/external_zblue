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

/**
 * @def HAS_BUILTIN(x)
 * @brief Check if the compiler supports the built-in function \a x.
 *
 * This macro is for use with conditional compilation to enable code using a
 * builtin function that may not be available in every compiler.
 */
#ifdef __has_builtin
#define HAS_BUILTIN(x) __has_builtin(x)
#else
/*
 * The compiler doesn't provide the __has_builtin() macro, so instead we depend
 * on the toolchain-specific headers to define HAS_BUILTIN_x for the builtins
 * supported.
 */
#define HAS_BUILTIN(x) HAS_BUILTIN_##x
#endif

#if defined(__TOOLCHAIN_CUSTOM__)
/* This include line exists for off-tree definitions of compilers,
 * and therefore this header is not meant to exist in-tree
 */
#include <toolchain/other.h>
#elif defined(__XCC__)
#include <zephyr/toolchain/xcc.h>
#elif defined(__CCAC__)
#include <zephyr/toolchain/mwdt.h>
#elif defined(__ARMCOMPILER_VERSION)
#include <zephyr/toolchain/armclang.h>
#elif defined(__llvm__) || (defined(_LINKER) && defined(__LLD_LINKER_CMD__))
#include <zephyr/toolchain/llvm.h>
#elif defined(__GNUC__) || (defined(_LINKER) && defined(__GCC_LINKER_CMD__))
#include <zephyr/toolchain/gcc.h>
#else
#error "Invalid/unknown toolchain configuration"
#endif

#endif /* ZEPHYR_INCLUDE_TOOLCHAIN_H_ */
