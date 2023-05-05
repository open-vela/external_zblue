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

static const struct device devs[CONFIG_FLASH_MAP];

static const struct flash_parameters flash_param = {
	.write_block_size	= 0x04,
	.erase_value		= 0xff,
};

#define  FLASH_MAP_INIT(i, _) [i] = { .fa_dev_name = CONFIG_FLASH_MAP_##i##_NAME, }
static struct flash_area flash_maps[] = { LISTIFY(CONFIG_FLASH_MAP, FLASH_MAP_INIT, (,)) };

static struct {
	struct mtd_dev_s *mtd;
	size_t erase_size;
	size_t neraseblocks;
} geos[CONFIG_FLASH_MAP];

extern int find_mtddriver(const char *pathname, struct inode **ppinode);

int flash_area_open(uint8_t id, const struct flash_area **fap)
{
	struct mtd_geometry_s geo;
	struct inode *node;
	int ret;

	if (id > ARRAY_SIZE(flash_maps)) {
		return -E2BIG;
	}
	
	ret = find_mtddriver(flash_maps[id].fa_dev_name, &node);
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

	geos[id].erase_size = geo.erasesize;
	geos[id].neraseblocks = geo.neraseblocks;
	geos[id].mtd = node->u.i_mtd;

	flash_maps[id].fa_id = id;
	flash_maps[id].fa_size = geo.erasesize * geo.neraseblocks;

	*fap = &flash_maps[id];

	return 0;
}

int flash_area_get_sectors(int fa_id, uint32_t *count,
			   struct flash_sector *sectors)
{
	sectors->fs_size = geos[fa_id].erase_size;
	if (count) {
		*count = geos[fa_id].neraseblocks;
	}

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
	for (int i = 0; i < ARRAY_SIZE(flash_maps); i++) {
		if (!strcmp(flash_maps[i].fa_dev_name, name)) {
			return &devs[i];
		}
	}

	return NULL;
}

int flash_read(const struct device *dev, off_t offset, void *data,
	       size_t len)
{
	size_t id = dev - devs;

	if (id > ARRAY_SIZE(geos)) {
		return -ENOTSUP;
	}

	if (!geos[id].mtd || !geos[id].mtd->read) {
		return -ENODEV;
	}

	(void)geos[id].mtd->read(geos[id].mtd, offset, len, data);

	return 0;
}

int flash_write(const struct device *dev, off_t offset,
		const void *data, size_t len)
{
	size_t id = dev - devs;

	if (id > ARRAY_SIZE(geos)) {
		return -ENOTSUP;
	}

	if (!geos[id].mtd || !geos[id].mtd->write) {
		return -ENODEV;
	}

	(void)geos[id].mtd->write(geos[id].mtd, offset, len, data);

	return 0;
}

int flash_erase(const struct device *dev, off_t offset, size_t size)
{
	size_t id = dev - devs;

	if (id > ARRAY_SIZE(geos)) {
		return -ENOTSUP;
	}

	if (!geos[id].mtd || !geos[id].mtd->erase) {
		return -ENODEV;
	}

	if ((offset % geos[id].erase_size) ||
	    (size % geos[id].erase_size)) {
		return -EINVAL;
	}

	(void)geos[id].mtd->erase(geos[id].mtd, offset / geos[id].erase_size,
				  size / geos[id].erase_size);

	return 0;
}
