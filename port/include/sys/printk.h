/* printk.h - low-level debug output */

/*
 * Copyright (c) 2010-2012, 2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_SYS_PRINTK_H_
#define ZEPHYR_INCLUDE_SYS_PRINTK_H_

#include <toolchain.h>
#include <stddef.h>
#include <stdarg.h>
#include <inttypes.h>
#include <syslog.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int snprintf(char *str, size_t size, const char *format, ...);
extern int vsnprintf(char *str, size_t size, const char *format, va_list ap);

#define printk(fmt, ...)    syslog(LOG_INFO, fmt, ##__VA_ARGS__)
#define snprintk            snprintf
#define vsnprintk           vsnprintf

#ifdef __cplusplus
}
#endif

#endif
