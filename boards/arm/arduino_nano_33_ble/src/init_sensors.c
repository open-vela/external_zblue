/*
 * Copyright (c) 2020 Jefferson Lee.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <init.h>
#include <arduino_nano_33_ble.h>

#define ARDUINO_SENSOR_INIT_PRIORITY 50

/*
 * this method roughly follows the steps here:
 * https://github.com/arduino/ArduinoCore-nRF528x-mbedos/blob/6216632cc70271619ad43547c804dabb4afa4a00/variants/ARDUINO_NANO33BLE/variant.cpp#L136
 */

static int board_internal_sensors_init(const struct device *dev)
{
	ARG_UNUSED(dev);
	struct arduino_gpio_t gpios;

	arduino_gpio_init(&gpios);

	arduino_gpio_pinMode(&gpios, ARDUINO_INTERNAL_I2C_PULLUP, GPIO_OUTPUT);
	arduino_gpio_digitalWrite(&gpios, ARDUINO_INTERNAL_I2C_PULLUP, 1);
	return 0;
}
SYS_INIT(board_internal_sensors_init, POST_KERNEL, ARDUINO_SENSOR_INIT_PRIORITY);
