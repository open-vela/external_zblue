/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_LOGGING_LOG_H_
#define ZEPHYR_INCLUDE_LOGGING_LOG_H_

#include <nuttx/config.h>

#define LOG_MODULE_REGISTER(a, ...)

#undef LOG_DBG
#undef LOG_INF
#undef LOG_WRN
#undef LOG_ERR

#ifndef BT_DBG_ENABLED
#define BT_DBG_ENABLED 1
#endif

#ifndef LOG_MODULE_NAME
#define LOG_MODULE_NAME unknown
#endif

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

#define _STR(_s)	   #_s
#define STR(s)	   	   _STR(s)

#if (CONFIG_BT_DEBUG_LOG_LEVEL >= PORT_LOG_DEBUG) && (BT_DBG_ENABLED)
  #define LOG_DBG(fmt, ...) syslog(PORT_LOG_DEBUG,   "<dbg> "STR(LOG_MODULE_NAME)": "fmt"\n", ##__VA_ARGS__)
#else
  #define LOG_DBG(fmt, ...)
#endif

#if (CONFIG_BT_DEBUG_LOG_LEVEL >= PORT_LOG_INFO)
  #define LOG_INF(fmt, ...) syslog(PORT_LOG_INFO,    "<inf> "STR(LOG_MODULE_NAME)": "fmt"\n", ##__VA_ARGS__)
#else
  #define LOG_INF(fmt, ...)
#endif

#if (CONFIG_BT_DEBUG_LOG_LEVEL >= PORT_LOG_WARNING)
  #define LOG_WRN(fmt, ...) syslog(PORT_LOG_WARNING, "<wrn> "STR(LOG_MODULE_NAME)": "fmt"\n", ##__VA_ARGS__)
#else
  #define LOG_WRN(fmt, ...)
#endif

#if (CONFIG_BT_DEBUG_LOG_LEVEL >= PORT_LOG_ERR)
  #define LOG_ERR(fmt, ...) syslog(PORT_LOG_ERR,     "<err> "STR(LOG_MODULE_NAME)": "fmt"\n", ##__VA_ARGS__)
#else
  #define LOG_ERR(fmt, ...)
#endif

#define LOG_HEXDUMP_DBG(_data, _length, _str)

static inline char *log_strdup(const char *str)
{
	return (char *)str;
}

#endif /* ZEPHYR_INCLUDE_LOGGING_LOG_H_ */
