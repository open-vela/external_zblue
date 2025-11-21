/** @file at.h
 *  @brief AT command header.
 */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 * Copyright (c) 2025 Xiaomi Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_BLUETOOTH_AT_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_AT_H_

enum bt_at_result {
	BT_AT_RESULT_OK,
	BT_AT_RESULT_ERROR,
	BT_AT_RESULT_CME_ERROR
};

enum bt_at_cme {
	BT_AT_CME_ERROR_AG_FAILURE                    = 0,
	BT_AT_CME_ERROR_NO_CONNECTION_TO_PHONE        = 1,
	BT_AT_CME_ERROR_OPERATION_NOT_ALLOWED         = 3,
	BT_AT_CME_ERROR_OPERATION_NOT_SUPPORTED       = 4,
	BT_AT_CME_ERROR_PH_SIM_PIN_REQUIRED           = 5,
	BT_AT_CME_ERROR_SIM_NOT_INSERTED              = 10,
	BT_AT_CME_ERROR_SIM_PIN_REQUIRED              = 11,
	BT_AT_CME_ERROR_SIM_PUK_REQUIRED              = 12,
	BT_AT_CME_ERROR_SIM_FAILURE                   = 13,
	BT_AT_CME_ERROR_SIM_BUSY                      = 14,
	BT_AT_CME_ERROR_INCORRECT_PASSWORD            = 16,
	BT_AT_CME_ERROR_SIM_PIN2_REQUIRED             = 17,
	BT_AT_CME_ERROR_SIM_PUK2_REQUIRED             = 18,
	BT_AT_CME_ERROR_MEMORY_FULL                   = 20,
	BT_AT_CME_ERROR_INVALID_INDEX                 = 21,
	BT_AT_CME_ERROR_MEMORY_FAILURE                = 23,
	BT_AT_CME_ERROR_TEXT_STRING_TOO_LONG          = 24,
	BT_AT_CME_ERROR_INVALID_CHARS_IN_TEXT_STRING  = 25,
	BT_AT_CME_ERROR_DIAL_STRING_TOO_LONG          = 26,
	BT_AT_CME_ERROR_INVALID_CHARS_IN_DIAL_STRING  = 27,
	BT_AT_CME_ERROR_NO_NETWORK_SERVICE            = 30,
	BT_AT_CME_ERROR_NETWORK_TIMEOUT               = 31,
	BT_AT_CME_ERROR_NETWORK_NOT_ALLOWED           = 32,
	BT_AT_CME_ERROR_UNKNOWN                       = 33,
};

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_AT_H_ */
