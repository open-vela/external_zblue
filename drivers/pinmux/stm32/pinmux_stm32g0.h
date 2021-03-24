/*
 * Copyright (c) 2019 Philippe Retornaz <philippe@shapescale.com>
 * Copyright (c) 2019 ST Microelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_PINMUX_STM32_PINMUX_STM32G0_H_
#define ZEPHYR_DRIVERS_PINMUX_STM32_PINMUX_STM32G0_H_

/**
 * @file Header for STM32G0 pin multiplexing helper
 */
#define STM32G0_PINMUX_FUNC_PA2_USART2_TX __DEPRECATED_MACRO \
	(STM32_PINMUX_ALT_FUNC_1 | STM32_PUSHPULL_NOPULL)
#define STM32G0_PINMUX_FUNC_PA3_USART2_RX __DEPRECATED_MACRO \
	(STM32_PINMUX_ALT_FUNC_1 | STM32_PUPDR_NO_PULL)

#define STM32G0_PINMUX_FUNC_PA9_USART1_TX __DEPRECATED_MACRO \
	(STM32_PINMUX_ALT_FUNC_1 | STM32_PUSHPULL_NOPULL)
#define STM32G0_PINMUX_FUNC_PA10_USART1_RX __DEPRECATED_MACRO \
	(STM32_PINMUX_ALT_FUNC_1 | STM32_PUSHPULL_NOPULL)
#define STM32G0_PINMUX_FUNC_PB6_USART1_TX __DEPRECATED_MACRO \
	(STM32_PINMUX_ALT_FUNC_0 | STM32_PUSHPULL_NOPULL)
#define STM32G0_PINMUX_FUNC_PB7_USART1_RX __DEPRECATED_MACRO \
	(STM32_PINMUX_ALT_FUNC_0 | STM32_PUPDR_NO_PULL)
#define STM32G0_PINMUX_FUNC_PB6_USART1_TX_RX __DEPRECATED_MACRO \
	(STM32_PINMUX_ALT_FUNC_0 | STM32_OPENDRAIN_PULLUP)

#define STM32G0_PINMUX_FUNC_PB6_I2C1_SCL __DEPRECATED_MACRO \
	(STM32_PINMUX_ALT_FUNC_6 | STM32_OPENDRAIN_PULLUP)
#define STM32G0_PINMUX_FUNC_PB7_I2C1_SDA __DEPRECATED_MACRO \
	(STM32_PINMUX_ALT_FUNC_6 | STM32_OPENDRAIN_PULLUP)

#define STM32G0_PINMUX_FUNC_PA7_TIM3_CH2 \
	(STM32_PINMUX_ALT_FUNC_1 | STM32_PUSHPULL_NOPULL)

#define STM32G0_PINMUX_FUNC_PA6_PWM3_CH1 __DEPRECATED_MACRO    \
	(STM32_PINMUX_ALT_FUNC_1 | STM32_PUSHPULL_NOPULL)

#endif /* ZEPHYR_DRIVERS_PINMUX_STM32_PINMUX_STM32G0_H_ */
