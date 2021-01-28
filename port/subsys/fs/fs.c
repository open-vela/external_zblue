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

#ifndef CONFIG_LIBC_TMPDIR
#define CONFIG_LIBC_TMPDIR "/tmp"
#endif

struct fs_file_pair_s
{
	char orig[MAX_FILE_NAME];
	char temp[MAX_FILE_NAME];
	void *filep;
};

/* File operations */

static int fs_dup(const char *oldpath, const char *newpath)
{
	char swap[256];
	int old;
	int new;
	int ret;

	old = open(oldpath, O_RDONLY);
	if (old < 0)
		return old;

	new = open(newpath, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (new < 0) {
		close(old);
		return new;
	}

	while ((ret = read(old, swap, sizeof(swap))) > 0) {
		ret = write(new, swap, ret);
		if (ret < 0)
			break;
	}

	close(old);
	close(new);

	return ret;
}

int fs_open(struct fs_file_t *zfp, const char *file_name, fs_mode_t flags)
{
	struct fs_file_pair_s *pair;
	char *rel;

	pair = (struct fs_file_pair_s *)
		calloc(1, sizeof(struct fs_file_pair_s));
	if (pair == NULL)
		return -ENOMEM;

	rel = strrchr(file_name, '/');
	if (rel == NULL)
		rel = (char *)file_name;

	rel = tempnam(CONFIG_LIBC_TMPDIR, rel);
	if (rel == NULL) {
		free(pair);
		return -ENOMEM;
	}

	strncpy(pair->orig, file_name, MAX_FILE_NAME);
	strncpy(pair->temp, rel, MAX_FILE_NAME);
	free(rel);

	if (access(pair->orig, F_OK) == 0 &&
			access(pair->temp, F_OK) != 0)
		fs_dup(pair->orig, pair->temp);

	pair->filep = fopen(pair->temp, "a+");
	if (!pair->filep) {
		free(pair);
		return -EINVAL;
	}

	zfp->filep = pair;

	return 0;
}

int fs_close(struct fs_file_t *zfp)
{
	struct fs_file_pair_s *pair = zfp->filep;

	fclose(pair->filep);
	fs_dup(pair->temp, pair->orig);
	unlink(pair->temp);

	free(pair);

	return 0;
}

ssize_t fs_read(struct fs_file_t *zfp, void *ptr, size_t size)
{
	struct fs_file_pair_s *pair = zfp->filep;

	return fread(ptr, 1, size, pair->filep);
}

ssize_t fs_write(struct fs_file_t *zfp, const void *ptr, size_t size)
{
	struct fs_file_pair_s *pair = zfp->filep;

	return fwrite(ptr, 1, size, pair->filep);
}

int fs_seek(struct fs_file_t *zfp, off_t offset, int whence)
{
	struct fs_file_pair_s *pair = zfp->filep;
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

	ret = fseek(pair->filep, offset, _whence);
	return ret >= 0 ? 0 : ret;
}

off_t fs_tell(struct fs_file_t *zfp)
{
	struct fs_file_pair_s *pair = zfp->filep;

	return ftell(pair->filep);
}

int fs_truncate(struct fs_file_t *zfp, off_t length)
{
	struct fs_file_pair_s *pair = zfp->filep;
	return ftruncate(fileno(pair->filep), length);
}

int fs_sync(struct fs_file_t *zfp)
{
	struct fs_file_pair_s *pair = zfp->filep;
	return fsync(fileno(pair->filep));
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

	if (ret == 0) {
		strncpy(entry->name, abs_path, sizeof(entry->name));
		entry->type = S_ISDIR(buf.st_mode) ? FS_DIR_ENTRY_DIR : FS_DIR_ENTRY_FILE;
		entry->size = buf.st_size;
	}

	return ret;
}
