/* mesh.c - Bluetooth Mesh Tester */

/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <errno.h>
#include <sys/byteorder.h>
#include <power/reboot.h>

#include <bttester.h>

#define TESTER_SYSTEM_RESET 0xff

void tester_handle_system(uint8_t opcode, uint8_t index, uint8_t *data, uint16_t len)
{
	switch (opcode) {
		case TESTER_SYSTEM_RESET:
			sys_reboot(SYS_REBOOT_WARM);
			break;
		default:
			break;
	}
}
