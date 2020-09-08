/****************************************************************************
 * apps/external/zblue/port/subsys/bluetooth/shell/bt_shell.c
 *
 *   Copyright (C) 2020 Xiaomi InC. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#include <zephyr.h>
#include <shell/shell.h>

#if defined(CONFIG_BT_BT_SHELL)
extern const struct shell_cmd_entry shell_cmd_bt;
static const struct shell_cmd_entry *g_shell_cmd = &shell_cmd_bt;
#elif defined(CONFIG_BT_GATT_SHELL)
extern const struct shell_cmd_entry shell_cmd_gatt;
static const struct shell_cmd_entry *g_shell_cmd = &shell_cmd_gatt;
#elif defined(CONFIG_BT_L2CAP_SHELL)
extern const struct shell_cmd_entry shell_cmd_l2cap;
static const struct shell_cmd_entry *g_shell_cmd = &shell_cmd_l2cap;
#elif defined(CONFIG_BT_TICKER_SHELL)
extern const struct shell_cmd_entry shell_cmd_ticker;
static const struct shell_cmd_entry *g_shell_cmd = &shell_cmd_ticker;
#elif defined(CONFIG_BT_BREDR_SHELL)
extern const struct shell_cmd_entry shell_cmd_bredr;
static const struct shell_cmd_entry *g_shell_cmd = &shell_cmd_bredr;
#elif defined(CONFIG_BT_ISO_SHELL)
extern const struct shell_cmd_entry shell_cmd_iso;
static const struct shell_cmd_entry *g_shell_cmd = &shell_cmd_iso;
#elif defined(CONFIG_BT_RPCOMM_SHELL)
extern const struct shell_cmd_entry shell_cmd_rfcomm;
static const struct shell_cmd_entry *g_shell_cmd = &shell_cmd_rfcomm;
#elif defined(CONFIG_BT_MESH_SHELL)
extern const struct shell_cmd_entry shell_cmd_mesh;
static const struct shell_cmd_entry *g_shell_cmd = &shell_cmd_mesh;
#else
static const struct shell_cmd_entry shell_cmd_dummy;
static const struct shell_cmd_entry *g_shell_cmd = &shell_cmd_dummy;
#endif

int main(int argc, FAR char *argv[])
{
	const struct shell_static_entry *cmds =
		g_shell_cmd->u.entry->subcmd->u.entry;
	struct shell sh = { .cmd = g_shell_cmd };

	if (argc < 2)
		goto bail;

	argc -= 1;

	for (; cmds->syntax; cmds++)
		if (!strncmp(cmds->syntax, argv[1], CONFIG_SHELL_STR_SIZE))
			break;

	if (cmds->syntax == NULL ||
			cmds->handler == NULL ||
			cmds->args.mandatory > argc)
		goto bail;

	return cmds->handler(&sh, argc, &argv[1]);

bail:
	shell_help(&sh);

	return 0;
}
