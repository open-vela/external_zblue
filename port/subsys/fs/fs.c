/*
 * Copyright (c) 2018 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <zephyr/types.h>
#include <errno.h>
#include <init.h>
#include <fs/fs.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

/* File operations */
int fs_open(struct fs_file_t *zfp, const char *file_name, fs_mode_t flags)
{
	zfp->filep = fopen(file_name, "a+");
	if (!zfp->filep) {
		return -EINVAL;
	}

	return 0;
}

int fs_close(struct fs_file_t *zfp)
{
	return fclose(zfp->filep);
}

ssize_t fs_read(struct fs_file_t *zfp, void *ptr, size_t size)
{
	return fread(ptr, 1, size, zfp->filep);
}

ssize_t fs_write(struct fs_file_t *zfp, const void *ptr, size_t size)
{
	return fwrite(ptr, 1, size, zfp->filep);
}

int fs_seek(struct fs_file_t *zfp, off_t offset, int whence)
{
	int _whence;
	int ret;

	switch (whence) {
		case FS_SEEK_SET:
			_whence = SEEK_SET;
			break;
		case FS_SEEK_CUR:
			_whence = SEEK_CUR;
			break;
		case FS_SEEK_END:
			_whence = SEEK_END;
			break;
		default:
			return -EINVAL;
	}

	ret = fseek(zfp->filep, offset, _whence);
	return ret >= 0 ? 0 : ret;
}

off_t fs_tell(struct fs_file_t *zfp)
{
	return ftell(zfp->filep);
}

int fs_truncate(struct fs_file_t *zfp, off_t length)
{
	return ftruncate(fileno(zfp->filep), length);
}

int fs_sync(struct fs_file_t *zfp)
{
	return fsync(fileno(zfp->filep));
}

/* Filesystem operations */

int fs_mkdir(const char *abs_path)
{
	if (mkdir(abs_path, 0777) < 0)
		return -errno;

	return 0;
}

int fs_unlink(const char *abs_path)
{
	return unlink(abs_path);
}

int fs_rename(const char *from, const char *to)
{
	return rename(from, to);
}

int fs_stat(const char *abs_path, struct fs_dirent *entry)
{
	struct stat buf;
	int ret;

	ret = lstat(abs_path, &buf);
	if (ret != 0) {
		return ret;
	}

	strncpy(entry->name, abs_path, sizeof(entry->name));
	entry->type = S_ISDIR(buf.st_mode) ? FS_DIR_ENTRY_DIR : FS_DIR_ENTRY_FILE;
	entry->size = buf.st_size;

	return ret;
}

/* TODO un-implementation function */
int fs_opendir(struct fs_dir_t *zdp, const char *abs_path)
{
	return -ENODEV;
}

int fs_readdir(struct fs_dir_t *zdp, struct fs_dirent *entry)
{
	return -ENODEV;
}

int fs_closedir(struct fs_dir_t *zdp)
{
	return -ENODEV;
}

int fs_statvfs(const char *abs_path, struct fs_statvfs *stat)
{
	return -ENODEV;
}
