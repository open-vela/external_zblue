/******************************************************************************
 *
 * Copyright (C) 2024 Xiaomi Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *****************************************************************************/

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#include <syslog.h>

long shell_strtol(const char *str, int base, int *err)
{
	long val;
	char *endptr = NULL;

	errno = 0;
	val = strtol(str, &endptr, base);
	if (errno == ERANGE) {
		*err = -ERANGE;
		return 0;
	} else if (errno || endptr == str || *endptr) {
		*err = -EINVAL;
		return 0;
	}

	return val;
}

unsigned long shell_strtoul(const char *str, int base, int *err)
{
	unsigned long val;
	char *endptr = NULL;

	if (*str == '-') {
		*err = -EINVAL;
		return 0;
	}

	errno = 0;
	val = strtoul(str, &endptr, base);
	if (errno == ERANGE) {
		*err = -ERANGE;
		return 0;
	} else if (errno || endptr == str || *endptr) {
		*err = -EINVAL;
		return 0;
	}

	return val;
}

bool shell_strtobool(const char *str, int base, int *err)
{
	if (!strcmp(str, "on") || !strcmp(str, "enable") || !strcmp(str, "true")) {
		return true;
	}

	if (!strcmp(str, "off") || !strcmp(str, "disable") || !strcmp(str, "false")) {
		return false;
	}

	return shell_strtoul(str, base, err);
}

void shell_vfprintf(const struct shell *sh, enum shell_vt100_color color,
		   const char *fmt, va_list args)
{
    ARG_UNUSED(sh);
    ARG_UNUSED(color);

    vsyslog(LOG_INFO, fmt, args);
}

/* These functions mustn't be used from shell context to avoid deadlock:
 * - shell_fprintf_impl
 * - shell_fprintf_info
 * - shell_fprintf_normal
 * - shell_fprintf_warn
 * - shell_fprintf_error
 * However, they can be used in shell command handlers.
 */
void shell_fprintf_impl(const struct shell *sh, enum shell_vt100_color color,
		   const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	shell_vfprintf(sh, color, fmt, args);
	va_end(args);
}

void shell_fprintf_info(const struct shell *sh, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	shell_vfprintf(sh, SHELL_INFO, fmt, args);
	va_end(args);
}

void shell_fprintf_normal(const struct shell *sh, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	shell_vfprintf(sh, SHELL_NORMAL, fmt, args);
	va_end(args);
}

void shell_fprintf_warn(const struct shell *sh, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	shell_vfprintf(sh, SHELL_WARNING, fmt, args);
	va_end(args);
}

void shell_fprintf_error(const struct shell *sh, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	shell_vfprintf(sh, SHELL_ERROR, fmt, args);
	va_end(args);
}

void shell_hexdump_line(const struct shell *sh, unsigned int offset,
			const uint8_t *data, size_t len)
{
	__ASSERT_NO_MSG(sh);

	int i;

	shell_fprintf_normal(sh, "%08X: ", offset);

	for (i = 0; i < SHELL_HEXDUMP_BYTES_IN_LINE; i++) {
		if (i > 0 && !(i % 8)) {
			shell_fprintf_normal(sh, " ");
		}

		if (i < len) {
			shell_fprintf_normal(sh, "%02x ",
					     data[i] & 0xFF);
		} else {
			shell_fprintf_normal(sh, "   ");
		}
	}

	shell_fprintf_normal(sh, "|");

	for (i = 0; i < SHELL_HEXDUMP_BYTES_IN_LINE; i++) {
		if (i > 0 && !(i % 8)) {
			shell_fprintf_normal(sh, " ");
		}

		if (i < len) {
			char c = data[i];

			shell_fprintf_normal(sh, "%c",
					     isprint((int)c) != 0 ? c : '.');
		} else {
			shell_fprintf_normal(sh, " ");
		}
	}

	shell_print(sh, "|");
}

void shell_hexdump(const struct shell *sh, const uint8_t *data, size_t len)
{
	__ASSERT_NO_MSG(sh);

	const uint8_t *p = data;
	size_t line_len;

	while (len) {
		line_len = MIN(len, SHELL_HEXDUMP_BYTES_IN_LINE);

		shell_hexdump_line(sh, p - data, p, line_len);

		len -= line_len;
		p += line_len;
	}
}

void shell_help(const struct shell *sh)
{
	const struct shell_static_entry *pcmds = &sh->ctx->active_cmd;

    shell_fprintf_normal(sh, "Help message\n");
	shell_fprintf_info(sh, "\t%s mands:%d opts:%d help:%s\n",
		   pcmds->syntax,
		   pcmds->args.mandatory, pcmds->args.optional,
		   pcmds->help);

	if (!pcmds->subcmd) {
		return;
	}

	pcmds = pcmds->subcmd->entry;
	for (; pcmds && pcmds->syntax; pcmds++) {
		shell_fprintf_info(sh, "\t%s mands:%d opts:%d help:%s\n",
		       pcmds->syntax,
		       pcmds->args.mandatory, pcmds->args.optional,
		       pcmds->help);
	}
}

/* Function returning pointer to parent command matching requested syntax. */
static const struct shell_static_entry *root_cmd_find(const char *syntax)
{
	TYPE_SECTION_FOREACH(const union shell_cmd_entry, shell_root_cmds, cmd) {
		if (strcmp(syntax, cmd->entry->syntax) == 0) {
			return cmd->entry;
		}
	}

	return NULL;
}

static void cmds_show(const struct shell *sh)
{
	TYPE_SECTION_FOREACH(const union shell_cmd_entry, shell_root_cmds, cmd) {
		shell_fprintf_info(sh, "%s\t%s\n",
			   cmd->entry->syntax, cmd->entry->help);
	}
}

static int execute_cmd(const struct shell *sh, size_t argc, char **argv)
{
	const struct shell_static_entry *cmd;

	cmd = root_cmd_find(argv[0]);
	if (!cmd) {
		return -ENOEXEC;
	}

	if (argc == 1) {
		memcpy(&sh->ctx->active_cmd, cmd,
		       sizeof(struct shell_static_entry));

		if (!cmd->handler) {
			return 0;
		}

		return cmd->handler(sh, argc, &argv[0]);
	}

	cmd = cmd->subcmd->entry;
	for (; cmd && cmd->syntax; cmd++) {
		if (strcmp(argv[1], cmd->syntax)) {
			continue;
		}

		if (cmd->args.mandatory > argc - 1) {
			shell_fprintf_info(sh, "cmd:%s Mands:%d opts:%d help:%s\n",
			   	   cmd->syntax,
			   	   cmd->args.mandatory, cmd->args.optional,
			   	   cmd->help);
			return 0;
		}

		memcpy(&sh->ctx->active_cmd, cmd,
		       sizeof(struct shell_static_entry));
		return cmd->handler(sh, argc - 1, &argv[1]);
	}

	return -ENOEXEC;
}

extern z_sys_init(void);

int main(int argc, char *argv[])
{
	struct shell_ctx ctx;
	struct shell sh = { .ctx = &ctx };
	int _argc = 0;
	char* _argv[32];
	char* buffer = NULL;
	char* saveptr;
	int ret;
	size_t len, size = 0;

	z_sys_init();

	while (1) {
		printf("zblue> ");
		fflush(stdout);

		memset(_argv, 0, sizeof(_argv));
		len = getline(&buffer, &size, stdin);
		if (-1 == len)
			goto end;

		buffer[len] = '\0';
		if (buffer[len - 1] == '\n')
			buffer[len - 1] = '\0';

		saveptr = NULL;
		char* tmpstr = buffer;

		while ((tmpstr = strtok_r(tmpstr, " ", &saveptr)) != NULL) {
			_argv[_argc] = tmpstr;
			_argc++;
			tmpstr = NULL;
		}

		if (_argc > 0) {
			if (strcmp(_argv[0], "q") == 0) {
				shell_fprintf_info(&sh, "Bye!\n");
				ret = 0;
				goto end;
			} else if (strcmp(_argv[0], "help") == 0) {
				cmds_show(&sh);
			} else {
				ret = execute_cmd(&sh, _argc, _argv);
			}

			_argc = 0;
		}
	}

return 0;

end:
	free(buffer);
	if (ret)
		cmds_show(&sh);

	return 0;
}
