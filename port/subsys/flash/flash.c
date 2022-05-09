/*
 * Copyright (c) 2018 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <zephyr/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <drivers/flash.h>
#include <storage/flash_map.h>
#include <nuttx/fs/fs.h>
#include <nuttx/mtd/mtd.h>
#include <nuttx/fs/ioctl.h>

#include <logging/log.h>

static const struct device dev;

static const struct flash_parameters flash_param = {
	.write_block_size	= 0x04,
	.erase_value		= 0xff,
};

static struct flash_area flash_map = {
        .pad16		= 0xffff,
        .fa_dev_name	= CONFIG_NVS_PATH,
};

static struct mtd_geometry_s geo;
static struct mtd_dev_s *mtd_dev;

extern int find_mtddriver(const char *pathname, struct inode **ppinode);

int flash_area_open(uint8_t id, const struct flash_area **fap)
{
	int ret;
	struct inode *node;
	
	ret = find_mtddriver(flash_map.fa_dev_name, &node);
	if (ret) {
		return ret;
	}

	ret = node->u.i_mtd->ioctl(node->u.i_mtd,
				   MTDIOC_GEOMETRY, (unsigned long)&geo);
	if (ret) {
		return ret;
	}

	if (!geo.erasesize || !geo.neraseblocks) {
		return -EINVAL;
	}

	mtd_dev = node->u.i_mtd;

	flash_map.fa_size = geo.erasesize * geo.neraseblocks;

	*fap = &flash_map;

	return 0;
}

int flash_area_get_sectors(int fa_id, uint32_t *count,
			   struct flash_sector *sectors)
{
	sectors->fs_size = geo.erasesize;

	return 0;
}

const struct flash_parameters *flash_get_parameters(const struct device *dev)
{
	return &flash_param;
}

size_t flash_get_write_block_size(const struct device *dev)
{
	return flash_param.write_block_size;
}

int flash_get_page_info_by_offs(const struct device *dev,
				off_t offset,
				struct flash_pages_info *info)
{
	info->start_offset = 0x00;
	info->index = 0x00;
	info->size = flash_param.write_block_size;

	return 0;
}

const struct device *device_get_binding(const char *name)
{
	return &dev;
}

int flash_read(const struct device *dev, off_t offset, void *data,
	       size_t len)
{
	if (!mtd_dev || !mtd_dev->read) {
		return -ENODEV;
	}

	(void)mtd_dev->read(mtd_dev, offset, len, data);

	return 0;
}

int flash_write(const struct device *dev, off_t offset,
		const void *data, size_t len)
{
	ssize_t ret;

	if (!mtd_dev || !mtd_dev->write) {
		return -ENODEV;
	}

	(void)mtd_dev->write(mtd_dev, offset, len, data);

	return 0;
}

int flash_erase(const struct device *dev, off_t offset, size_t size)
{
	if (!mtd_dev || !mtd_dev->erase) {
		return -ENODEV;
	}

	if ((offset % geo.erasesize) ||
	    (size % geo.erasesize)) {
		return -EINVAL;
	}

	(void)mtd_dev->erase(mtd_dev, offset / geo.erasesize,
			     size / geo.erasesize);

	return 0;
}
