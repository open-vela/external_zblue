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
#define CONFIG_LIBC_TMPDIR      "/tmp"
#endif

#define FS_FILE_FLUSH_INTERVAL  10
#define FS_FILE_FLUSH_TIMEOUT   K_MSEC(5000)

struct fs_file_internal_s
{
	sys_snode_t           node;
	char                  orig[MAX_FILE_NAME];
	FILE                  *filep;
	int                   interval;
	struct k_sem          flushsync;
	struct k_delayed_work flushwork;
};

static sys_slist_t g_file_list;

/* File operations */

static int fs_copy(const char *oldpath, const char *newpath)
{
	char buf[256];
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

	while ((ret = read(old, buf, sizeof(buf))) > 0) {
		ret = write(new, buf, ret);
		if (ret < 0)
			break;
	}

	close(old);
	close(new);

	return ret;
}

static void fs_covert_temp_name(const char *file_name, char *temp_name)
{
	const char *rel;

	rel = strrchr(file_name, '/');
	if (rel == NULL)
		rel = file_name;

	snprintf(temp_name, MAX_FILE_NAME,
			"%s%s", CONFIG_LIBC_TMPDIR, rel);
}

static bool fs_refresh(struct fs_file_internal_s *fp, bool force)
{
	char temp[MAX_FILE_NAME];
	bool refresh = false;

	(void)k_sem_take(&fp->flushsync, K_FOREVER);

	if (force || (fp->interval++ > FS_FILE_FLUSH_INTERVAL)) {
		fs_covert_temp_name(fp->orig, temp);
		fs_copy(temp, fp->orig);
		fp->interval = 0;
		refresh = true;
	}

	k_sem_give(&fp->flushsync);

	return refresh;
}

static void fs_refresh_work(struct k_work *work)
{
	struct fs_file_internal_s *fp = work->priv;

	if (!fp)
		return;

	fs_refresh(fp, true);
}

int fs_open(struct fs_file_t *zfp, const char *file_name, fs_mode_t flags)
{
	struct fs_file_internal_s *fp;
	char temp[MAX_FILE_NAME];

	fs_covert_temp_name(file_name, temp);

	SYS_SLIST_FOR_EACH_CONTAINER(&g_file_list, fp, node) {
		if (strncmp(fp->orig, file_name, MAX_FILE_NAME))
			continue;
		fp->filep = fopen(temp, "a+");
		if (!fp->filep)
			return -EINVAL;
		zfp->filep = fp;
		return 0;
	}

	fp = (struct fs_file_internal_s *)
		calloc(1, sizeof(struct fs_file_internal_s));
	if (fp == NULL)
		return -ENOMEM;

	strncpy(fp->orig, file_name, MAX_FILE_NAME);

	if (access(fp->orig, F_OK) == 0 &&
			access(temp, F_OK) != 0)
		fs_copy(fp->orig, temp);

	fp->filep = fopen(temp, "a+");
	if (!fp->filep) {
		free(fp);
		return -EINVAL;
	}

	sys_slist_append(&g_file_list, &fp->node);

	k_sem_init(&fp->flushsync, 1, 1);

	k_delayed_work_init(&fp->flushwork, fs_refresh_work);
	fp->flushwork.work.priv = fp;

	zfp->filep = fp;

	return 0;
}

int fs_close(struct fs_file_t *zfp)
{
	struct fs_file_internal_s *fp = zfp->filep;

	fclose(fp->filep);

	k_delayed_work_cancel(&fp->flushwork);

	if (!fs_refresh(fp, false))
		k_delayed_work_submit_to_queue(NULL, &fp->flushwork,
				FS_FILE_FLUSH_TIMEOUT);

	return 0;
}

ssize_t fs_read(struct fs_file_t *zfp, void *ptr, size_t size)
{
	struct fs_file_internal_s *fp = zfp->filep;

	return fread(ptr, 1, size, fp->filep);
}

ssize_t fs_write(struct fs_file_t *zfp, const void *ptr, size_t size)
{
	struct fs_file_internal_s *fp = zfp->filep;

	return fwrite(ptr, 1, size, fp->filep);
}

int fs_seek(struct fs_file_t *zfp, off_t offset, int whence)
{
	struct fs_file_internal_s *fp = zfp->filep;
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

	ret = fseek(fp->filep, offset, _whence);
	return ret >= 0 ? 0 : ret;
}

off_t fs_tell(struct fs_file_t *zfp)
{
	struct fs_file_internal_s *fp = zfp->filep;

	return ftell(fp->filep);
}

int fs_truncate(struct fs_file_t *zfp, off_t length)
{
	struct fs_file_internal_s *fp = zfp->filep;
	return ftruncate(fileno(fp->filep), length);
}

int fs_sync(struct fs_file_t *zfp)
{
	struct fs_file_internal_s *fp = zfp->filep;
	return fsync(fileno(fp->filep));
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
