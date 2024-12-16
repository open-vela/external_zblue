/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_LOGGING_LOG_H_
#define ZEPHYR_INCLUDE_LOGGING_LOG_H_

#include <nuttx/config.h>
#include <syslog.h>

#define LOG_MODULE_REGISTER(...)
#define LOG_MODULE_DECLARE(...)

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

#define PORT_LOG(_level, _l_str, fmt, args...) do { \
    char _log_buf[64]; \
    int _pos = 0; \
    _pos += snprintf(_log_buf + _pos, sizeof(_log_buf) - _pos, _l_str); \
    if (IS_ENABLED(CONFIG_BT_DEBUG_LOG_FUNCTION_NAME)) { \
        _pos += snprintf(_log_buf + _pos, sizeof(_log_buf) - _pos, " [%s]", __func__); \
    } \
    if (IS_ENABLED(CONFIG_BT_DEBUG_LOG_LINE_NUMBER)) { \
        _pos += snprintf(_log_buf + _pos, sizeof(_log_buf) - _pos, " <%d>:", __LINE__); \
    } \
    syslog(_level, "%s " fmt "\n", _log_buf, ##args); \
} while (0)

#if (CONFIG_BT_DEBUG_LOG_LEVEL >= PORT_LOG_DEBUG)
  #define LOG_DBG(fmt, ...) PORT_LOG(PORT_LOG_DEBUG, "<dbg>", fmt, ##__VA_ARGS__)
#else
  #define LOG_DBG(fmt, ...)
#endif

#if (CONFIG_BT_DEBUG_LOG_LEVEL >= PORT_LOG_INFO)
  #define LOG_INF(fmt, ...) PORT_LOG(PORT_LOG_INFO, "<inf>", fmt, ##__VA_ARGS__)
#else
  #define LOG_INF(fmt, ...)
#endif

#if (CONFIG_BT_DEBUG_LOG_LEVEL >= PORT_LOG_WARNING)
  #define LOG_WRN(fmt, ...) PORT_LOG(PORT_LOG_WARNING, "<wrn>", fmt, ##__VA_ARGS__)
#else
  #define LOG_WRN(fmt, ...)
#endif

#if (CONFIG_BT_DEBUG_LOG_LEVEL >= PORT_LOG_ERR)
  #define LOG_ERR(fmt, ...) PORT_LOG(PORT_LOG_ERR, "<err>", fmt, ##__VA_ARGS__)
#else
  #define LOG_ERR(fmt, ...)
#endif

#define LOG_HEXDUMP_INF(_data, _length, _str)
#define LOG_HEXDUMP_DBG(_data, _length, _str)

#endif /* ZEPHYR_INCLUDE_LOGGING_LOG_H_ */
