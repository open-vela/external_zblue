/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_LOGGING_LOG_H_
#define ZEPHYR_INCLUDE_LOGGING_LOG_H_

#include <nuttx/config.h>

#define LOG_MODULE_REGISTER(...)

#undef LOG_DBG
#undef LOG_INF
#undef LOG_WRN
#undef LOG_ERR

/*
 * Conflict Log Tags from NuttX definition.
 * These defines follow the values used by syslog(2)
 */

#define PORT_LOG_EMERG     0  /* System is unusable */
#define PORT_LOG_ALERT     1  /* Action must be taken immediately */
#define PORT_LOG_CRIT      2  /* Critical conditions */
#define PORT_LOG_ERR       3  /* Error conditions */
#define PORT_LOG_WARNING   4  /* Warning conditions */
#define PORT_LOG_NOTICE    5  /* Normal, but significant, condition */
#define PORT_LOG_INFO      6  /* Informational message */
#define PORT_LOG_DEBUG     7  /* Debug-level message */

#if (CONFIG_BT_DEBUG_LOG_LEVEL >= PORT_LOG_DEBUG)
  #define LOG_DBG(fmt, ...) syslog(PORT_LOG_DEBUG,   fmt"\n", ##__VA_ARGS__)
#else
  #define LOG_DBG(fmt, ...)
#endif

#define LOG_INF(fmt, ...) syslog(PORT_LOG_INFO,    fmt"\n", ##__VA_ARGS__)
#define LOG_WRN(fmt, ...) syslog(PORT_LOG_WARNING, fmt"\n", ##__VA_ARGS__)
#define LOG_ERR(fmt, ...) syslog(PORT_LOG_ERR,     fmt"\n", ##__VA_ARGS__)

#define LOG_HEXDUMP_DBG(_data, _length, _str)

static inline char *log_strdup(const char *str)
{
	return (char *)str;
}

#endif /* ZEPHYR_INCLUDE_LOGGING_LOG_H_ */
