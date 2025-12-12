/** @file
 *  @brief Bluetooth subsystem classic core APIs.
 */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_BLUETOOTH_CLASSIC_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_CLASSIC_H_

/**
 * @brief Bluetooth APIs
 * @defgroup bluetooth Bluetooth APIs
 * @ingroup connectivity
 * @{
 */

#include <stdbool.h>
#include <string.h>

#include <zephyr/sys/util.h>
#include <zephyr/net_buf.h>
#include <zephyr/bluetooth/addr.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Generic Access Profile (GAP)
 * @defgroup bt_gap Generic Access Profile (GAP)
 * @since 1.0
 * @version 1.0.0
 * @ingroup bluetooth
 * @{
 */

/**
 * @private
 * @brief BR/EDR discovery private structure
 */
struct bt_br_discovery_priv {
	/** Clock offset */
	uint16_t clock_offset;
	/** Page scan repetition mode */
	uint8_t pscan_rep_mode;
	/** Resolving remote name*/
	bool resolving;
};

/** @brief BR/EDR discovery result structure */
struct bt_br_discovery_result {
	/** Private data */
	struct bt_br_discovery_priv _priv;

	/** Remote device address */
	bt_addr_t addr;

	/** RSSI from inquiry */
	int8_t rssi;

	/** Class of Device */
	uint8_t cod[3];

	/** Extended Inquiry Response */
	uint8_t eir[240];
};

/** BR/EDR discovery parameters */
struct bt_br_discovery_param {
	/** Maximum length of the discovery in units of 1.28 seconds.
	 *  Valid range is 0x01 - 0x30.
	 */
	uint8_t length;

	/** True if limited discovery procedure is to be used. */
	bool limited;
};

/**
 * @brief Start BR/EDR discovery
 *
 * Start BR/EDR discovery (inquiry) and provide results through the specified
 * callback. The discovery results will be notified through callbacks
 * registered by @ref bt_br_discovery_cb_register.
 * If more inquiry results were received during session than
 * fits in provided result storage, only ones with highest RSSI will be
 * reported.
 *
 * @param param Discovery parameters.
 * @param results Storage for discovery results.
 * @param count Number of results in storage. Valid range: 1-255.
 *
 * @return Zero on success or error code otherwise, positive in case
 * of protocol error or negative (POSIX) in case of stack internal error
 */
int bt_br_discovery_start_mc(uint8_t dev_id, const struct bt_br_discovery_param *param,
	struct bt_br_discovery_result *results, size_t count);
#ifdef CONFIG_BT_ORIGINAL_API
static inline int bt_br_discovery_start(const struct bt_br_discovery_param *param,
			  struct bt_br_discovery_result *results, size_t count)
{
	return bt_br_discovery_start_mc(0, param, results, count);
}
#endif

/**
 * @brief Stop BR/EDR discovery.
 *
 * Stops ongoing BR/EDR discovery. If discovery was stopped by this call
 * results won't be reported
 *
 * @return Zero on success or error code otherwise, positive in case of
 *         protocol error or negative (POSIX) in case of stack internal error.
 */
int bt_br_discovery_stop_mc(uint8_t dev_id);
#ifdef CONFIG_BT_ORIGINAL_API
static inline int bt_br_discovery_stop(void)
{
	return bt_br_discovery_stop_mc(0);
}
#endif

struct bt_br_discovery_cb {

	/**
	 * @brief An inquiry response received callback.
	 *
	 * @param result Storage used for discovery results
	 */
	void (*recv)(const struct bt_br_discovery_result *result);

	/** @brief The inquiry has stopped after discovery timeout.
	 *
	 * @param results Storage used for discovery results
	 * @param count Number of valid discovery results.
	 */
	void (*timeout)(const struct bt_br_discovery_result *results,
				  size_t count);

	sys_snode_t node;
};

/**
 * @brief Register discovery packet callbacks.
 *
 * Adds the callback structure to the list of callback structures that monitors
 * inquiry activity.
 *
 * This callback will be called for all inquiry activity, regardless of what
 * API was used to start the discovery.
 *
 * @param cb Callback struct. Must point to memory that remains valid.
 */
void bt_br_discovery_cb_register_mc(uint8_t dev_id, struct bt_br_discovery_cb *cb);
#ifdef CONFIG_BT_ORIGINAL_API
static inline void bt_br_discovery_cb_register(struct bt_br_discovery_cb *cb)
{
	bt_br_discovery_cb_register_mc(0, cb);
}
#endif

/**
 * @brief Unregister discovery packet callbacks.
 *
 * Remove the callback structure from the list of discovery callbacks.
 *
 * @param cb Callback struct. Must point to memory that remains valid.
 */
void bt_br_discovery_cb_unregister_mc(uint8_t dev_id, struct bt_br_discovery_cb *cb);
#ifdef CONFIG_BT_ORIGINAL_API
static inline void bt_br_discovery_cb_unregister(struct bt_br_discovery_cb *cb)
{
	bt_br_discovery_cb_unregister_mc(0, cb);
}
#endif

struct bt_br_oob {
	/** BR/EDR address. */
	bt_addr_t addr;
};

/**
 * @brief Get BR/EDR local Out Of Band information
 *
 * This function allows to get local controller information that are useful
 * for Out Of Band pairing or connection creation process.
 *
 * @param oob Out Of Band information
 */
int bt_br_oob_get_local_mc(uint8_t dev_id, struct bt_br_oob *oob);
#ifdef CONFIG_BT_ORIGINAL_API
static inline int bt_br_oob_get_local(struct bt_br_oob *oob)
{
	return bt_br_oob_get_local_mc(0, oob);
}
#endif

/**
 * @brief Enable/disable set controller in discoverable state.
 *
 * Allows make local controller to listen on INQUIRY SCAN channel and responds
 * to devices making general inquiry. To enable this state it's mandatory
 * to first be in connectable state.
 *
 * @param enable Value allowing/disallowing controller to become discoverable.
 *
 * @return Negative if fail set to requested state or requested state has been
 *         already set. Zero if done successfully.
 */
int bt_br_set_discoverable_mc(uint8_t dev_id, bool enable);
#ifdef CONFIG_BT_ORIGINAL_API
static inline int bt_br_set_discoverable(bool enable)
{
	return bt_br_set_discoverable_mc(0, enable);
}
#endif

/**
 * @brief Enable/disable set controller in connectable state.
 *
 * Allows make local controller to be connectable. It means the controller
 * start listen to devices requests on PAGE SCAN channel. If disabled also
 * resets discoverability if was set.
 *
 * @param enable Value allowing/disallowing controller to be connectable.
 *
 * @return Negative if fail set to requested state or requested state has been
 *         already set. Zero if done successfully.
 */
int bt_br_set_connectable_mc(uint8_t dev_id, bool enable);
#ifdef CONFIG_BT_ORIGINAL_API
static inline int bt_br_set_connectable(bool enable)
{
	return bt_br_set_connectable_mc(0, enable);
}
#endif

/**
 * @brief Enable/disable set controller in connectable and discoverable state.
 * 
 * Allows make local controller to be connectable or/and discoverable.
 * 
 * @param disc_mode Value allowing/disallowing controller to be discoverable.
 * @param conn_mode Value allowing/disallowing controller to be connectable.
 * 
 * @return Negative if fail set to requested state or requested state has been
 *         already set. Zero if done successfully. 
 */
int bt_br_set_visibility_mc(uint8_t dev_id, bool disc_mode, bool conn_mode);
#ifdef CONFIG_BT_ORIGINAL_API
static inline int bt_br_set_visibility(bool disc_mode, bool conn_mode)
{
	return bt_br_set_visibility_mc(0, disc_mode, conn_mode);
}
#endif

/**
 * @brief Set controller page scan activity.
 *
 * Page Scan is only performed when Page_Scan is enabled.
 *
 * @param interval Page scan interval in 0.625 ms units
 *        Range: 0x0012 to 0x1000; only even values are valid.
 * @param window Page scan window in 0.625 ms units
 *        Range: 0x0011 to 0x1000.
 *
 * @return Negative if fail set to requested state or requested state has been
 *         already set. Zero if done successfully.
 */
int bt_br_write_page_scan_activity_mc(uint8_t dev_id, uint16_t interval, uint16_t window);
#ifdef CONFIG_BT_ORIGINAL_API
static inline int bt_br_write_page_scan_activity(uint16_t interval, uint16_t window)
{
	return bt_br_write_page_scan_activity_mc(0, interval, window);
}
#endif

/**
 * @brief Set controller inquiry scan activity.
 *
 * Inquiry Scan is only performed when Inquiry_Scan is enabled.
 *
 * @param interval Inquiry scan interval in 0.625 ms units
 *        Range: 0x0012 to 0x1000; only even values are valid.
 * @param window Inquiry scan window in 0.625 ms units
 *        Range: 0x0011 to 0x1000.
 *
 * @return Negative if fail set to requested state or requested state has been
 *         already set. Zero if done successfully.
 */
int bt_br_write_inquiry_scan_activity_mc(uint8_t dev_id, uint16_t interval, uint16_t window);
#ifdef CONFIG_BT_ORIGINAL_API
static inline int bt_br_write_inquiry_scan_activity(uint16_t interval, uint16_t window)
{
	return bt_br_write_inquiry_scan_activity_mc(0, interval, window);
}
#endif

/**
 * @brief Set the inquiry Scan Type configuration parameter of the local
 *        BR/EDR Controller.
 *
 * @param type Inquiry scan type.
 * 		   0x00: Standard scan (default)
 * 		   0x01: Interlaced scan
 *
 * @return Negative if fail set to requested state or requested state has been
 *         already set. Zero if done successfully.
 */
int bt_br_write_inquiry_scan_type_mc(uint8_t dev_id, uint8_t type);
#ifdef CONFIG_BT_ORIGINAL_API
static inline int bt_br_write_inquiry_scan_type(uint8_t type)
{
	return bt_br_write_inquiry_scan_type_mc(0, type);
}
#endif

/**
 * @brief Set the page Scan Type configuration parameter of the local
 *        BR/EDR Controller.
 *
 * @param type Page scan type.
 *        0x00: Standard scan (default)
 *        0x01: Interlaced scan
 *
 * @return Negative if fail set to requested state or requested state has been
 *         already set. Zero if done successfully.
 */
int bt_br_write_page_scan_type_mc(uint8_t dev_id, uint8_t type);
#ifdef CONFIG_BT_ORIGINAL_API
static inline int bt_br_write_page_scan_type(uint8_t type)
{
	return bt_br_write_page_scan_type_mc(0, type);
}
#endif

/**
 * @brief Set the Class of Device configuration parameter of the local
 *        BR/EDR Controller.
 *
 * @param local_cod Class of Device value.
 *
 * @return Negative if fail set to requested state or requested state has been
 *         already set. Zero if done successfully.
 */
int bt_br_set_class_of_device_mc(uint8_t dev_id, uint32_t local_cod);
#ifdef CONFIG_BT_ORIGINAL_API
static inline int bt_br_set_class_of_device(uint32_t local_cod)
{
	return bt_br_set_class_of_device_mc(0, local_cod);
}
#endif

/**
 * @brief Set the local name of the BR/EDR Controller.
 *
 * @param name Local name of the BR/EDR Controller.
 *
 * @return Negative if fail set to requested state or requested state has been
 *         already set. Zero if done successfully.
 */
int bt_br_write_local_name_mc(uint8_t dev_id, const char *name);
#ifdef CONFIG_BT_ORIGINAL_API
static inline int bt_br_write_local_name(const char *name)
{
	return bt_br_write_local_name_mc(0, name);
}
#endif

/**
 * @brief Read the Extended Inquiry Response configuration parameter of the
 *        local BR/EDR Controller.
 *
 * @param status Status of the command.
 * @param fec_required FEC required for EIR.
 * @param eir Extended Inquiry Response data.
 *
 * @return Zero on success or error code otherwise, positive in case
 * of protocol error or negative (POSIX) in case of stack internal error.
 */
int bt_br_read_ext_inq_response_mc(uint8_t dev_id, uint8_t *status, uint8_t *fec_required, uint8_t *eir);
#ifdef CONFIG_BT_ORIGINAL_API
static inline int bt_br_read_ext_inq_response(uint8_t *status, uint8_t *fec_required, uint8_t *eir)
{
	return bt_br_read_ext_inq_response_mc(0, status, fec_required, eir);
}
#endif

/**
 * @brief Write the Extended Inquiry Response configuration parameter of the
 *        local BR/EDR Controller.
 *
 * @param fec_required FEC required for EIR.
 *
 * @return Zero on success or error code otherwise, positive in case
 * of protocol error or negative (POSIX) in case of stack internal error.
 */
int bt_br_write_ext_inq_response_mc(uint8_t dev_id, uint8_t fec_required);
#ifdef CONFIG_BT_ORIGINAL_API
static inline int bt_br_write_ext_inq_response(uint8_t fec_required)
{
	return bt_br_write_ext_inq_response_mc(0, fec_required);
}
#endif

struct bt_bond_info;

/**
 * @brief Callback for iterating over all BR/EDR bond information.
 *
 * @param info Bond information.
 * @param user_data Data passed to the iterator.
 */
void bt_foreach_bond_br_mc(uint8_t dev_id, void (*func)(const struct bt_bond_info *info, void *user_data),
			void *user_data);
#ifdef CONFIG_BT_ORIGINAL_API
static inline void bt_foreach_bond_br(void (*func)(const struct bt_bond_info *info, void *user_data),
			void *user_data)
{
	bt_foreach_bond_br_mc(0, func, user_data);
}
#endif

/**
 * @brief Request remote device name callback.
 *
 * @param bdaddr Remote device address.
 * @param name Remote device name.
 * @param status Status of the request.
 */
typedef void (*bt_br_remote_name_req_cb_t)(const bt_addr_t *bdaddr, const char *name, uint8_t status);

/**
 * @brief Request remote device name.
 *
 * Remote Name Request is used to find out the name of the remote
 * device without requiring an explicit ACL connection
 *
 * @param addr Remote device address.
 * @param cb Callback to notify about remote device name.
 *
 * @return 0 on success or negative error value on failure.
 */
int bt_br_remote_name_request_mc(uint8_t dev_id, const bt_addr_t *addr, bt_br_remote_name_req_cb_t cb);
#ifdef CONFIG_BT_ORIGINAL_API
static inline int bt_br_remote_name_request(const bt_addr_t *addr, bt_br_remote_name_req_cb_t cb)
{
	return bt_br_remote_name_request_mc(0, addr, cb);
}
#endif

/**
 * @brief Unpair with a br remote device.
 *
 * remove the bond information with the remote device in controller
 * or settings.
 *
 * @param bdaddr Remote device address.
 *
 * @return 0 on success or negative error value on failure.
 */
int bt_br_unpair_mc(uint8_t dev_id, bt_addr_t *bdaddr);
#ifdef CONFIG_BT_ORIGINAL_API
static inline int bt_br_unpair(bt_addr_t *bdaddr)
{
	return bt_br_unpair_mc(0, bdaddr);
}
#endif

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_CLASSIC_H_ */
