/*
 * Copyright (C) 2001 Sistina Software
 *
 * This LVM library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This LVM library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this LVM library; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA
 */

#if 0
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

#include <errno.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <linux/major.h>
#include <linux/genhd.h>

#include "dbg_malloc.h"
#include "log.h"
#include "dev-cache.h"
#include "metadata.h"
#include "device.h"

int _get_partition_type(struct dev_filter *filter, struct device *d);

#define MINOR_PART(dm, d) (MINOR((d)->dev) % dev_max_partitions(dm, (d)->dev))

int is_whole_disk(struct dev_filter *filter, struct device *d)
{
	return (MINOR_PART(dm, d)) ? 0 : 1;
}

int is_extended_partition(struct dev_mgr *dm, struct device *d)
{
	return (MINOR_PART(dm, d) > 4) ? 1 : 0;
}

struct device *dev_primary(struct dev_mgr *dm, struct device *d)
{
	struct device *ret;

	ret = dev_by_dev(dm, d->dev - MINOR_PART(dm, d));
	/* FIXME: Needs replacing with a 'refresh' */
	if (!ret) {
		init_dev_scan(dm);
		ret = dev_by_dev(dm, d->dev - MINOR_PART(dm, d));
	}

	return ret;

}

int partition_type_is_lvm(struct dev_mgr *dm, struct device *d)
{
	int pt;

	pt = _get_partition_type(dm, d);

	if (!pt) {
		if (is_whole_disk(dm, d))
			/* FIXME: Overloaded pt=0 in error cases */
			return 1;
		else {
			log_error
			    ("%s: missing partition table "
			     "on partitioned device", d->name);
			return 0;
		}
	}

	if (is_whole_disk(dm, d)) {
		log_error("%s: looks to possess partition table", d->name);
		return 0;
	}

	/* check part type */
	if (pt != LVM_PARTITION && pt != LVM_NEW_PARTITION) {
		log_error("%s: invalid partition type 0x%x "
			  "(must be 0x%x)", d->name, pt, LVM_NEW_PARTITION);
		return 0;
	}

	if (pt == LVM_PARTITION) {
		log_error
		    ("%s: old LVM partition type found - please change to 0x%x",
		     d->name, LVM_NEW_PARTITION);
		return 0;
	}

	return 1;
}

int _get_partition_type(struct dev_mgr *dm, struct device *d)
{
	int pv_handle = -1;
	struct device *primary;
	ssize_t read_ret;
	ssize_t bytes_read = 0;
	char *buffer;
	unsigned short *s_buffer;
	struct partition *part;
	loff_t offset = 0;
	loff_t extended_offset = 0;
	int part_sought;
	int part_found = 0;
	int first_partition = 1;
	int extended_partition = 0;
	int p;

	if (!(primary = dev_primary(dm, d))) {
		log_error
		    ("Failed to find main device containing partition %s",
		     d->name);
		return 0;
	}

	if (!(buffer = dbg_malloc(SECTOR_SIZE))) {
		log_error("Failed to allocate partition table buffer");
		return 0;
	}

	/* Get partition table */
	if ((pv_handle = open(primary->name, O_RDONLY)) < 0) {
		log_error("%s: open failed: %s", primary->name,
			  strerror(errno));
		return 0;
	}

	s_buffer = (unsigned short *) buffer;
	part = (struct partition *) (buffer + 0x1be);
	part_sought = MINOR_PART(dm, d);

	do {
		bytes_read = 0;

		if (llseek(pv_handle, offset * SECTOR_SIZE, SEEK_SET) == -1) {
			log_error("%s: llseek failed: %s",
				  primary->name, strerror(errno));
			return 0;
		}

		while ((bytes_read < SECTOR_SIZE) &&
		       (read_ret =
			read(pv_handle, buffer + bytes_read,
			     SECTOR_SIZE - bytes_read)) != -1)
			bytes_read += read_ret;

		if (read_ret == -1) {
			log_error("%s: read failed: %s", primary->name,
				  strerror(errno));
			return 0;
		}

		if (s_buffer[255] == 0xAA55) {
			if (is_whole_disk(dm, d))
				return -1;
		} else
			return 0;

		extended_partition = 0;

		/* Loop through primary partitions */
		for (p = 0; p < 4; p++) {
			if (part[p].sys_ind == DOS_EXTENDED_PARTITION ||
			    part[p].sys_ind == LINUX_EXTENDED_PARTITION
			    || part[p].sys_ind == WIN98_EXTENDED_PARTITION) {
				extended_partition = 1;
				offset = extended_offset + part[p].start_sect;
				if (extended_offset == 0)
					extended_offset = part[p].start_sect;
				if (first_partition == 1)
					part_found++;
			} else if (first_partition == 1) {
				if (p == part_sought) {
					if (part[p].sys_ind == 0) {
						/* missing primary? */
						return 0;
					}
				} else
					part_found++;
			} else if (!part[p].sys_ind)
				part_found++;

			if (part_sought == part_found)
				return part[p].sys_ind;

		}
		first_partition = 0;
	}
	while (extended_partition == 1);

	return 0;
}
#endif
