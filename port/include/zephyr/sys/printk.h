/* printk.h - low-level debug output */

/*
 * Copyright (c) 2010-2012, 2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_SYS_PRINTK_H_
#define ZEPHYR_INCLUDE_SYS_PRINTK_H_

#include <zephyr/toolchain.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <inttypes.h>
#include <syslog.h>

#ifdef __cplusplus
extern "C" {
#endif

#define printk(fmt, ...)    syslog(LOG_INFO, fmt, ##__VA_ARGS__)
#define vprintk(fmt, ap)	vsyslog(LOG_INFO, fmt, ap)

#define snprintk(...) snprintf(__VA_ARGS__)
#define vsnprintk(str, size, fmt, ap) vsnprintf(str, size, fmt, ap)

#ifdef __cplusplus
}
#endif

#endif
