/*
 * Copyright (C) 2020 Waldemar Kozaczuk
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "virtiofs.hh"
#include <sys/types.h>
#include <osv/device.h>
#include <osv/debug.h>
#include <iomanip>
#include <iostream>

static int virtiofs_mount(struct mount *mp, const char *dev, int flags, const void *data);
static int virtiofs_sync(struct mount *mp);
static int virtiofs_statfs(struct mount *mp, struct statfs *statp);
static int virtiofs_unmount(struct mount *mp, int flags);

#define virtiofs_vget ((vfsop_vget_t)vfs_nullop)

struct vfsops virtiofs_vfsops = {
    virtiofs_mount,		/* mount */
    virtiofs_unmount,	/* unmount */
    virtiofs_sync,		/* sync */
    virtiofs_vget,      /* vget */
    virtiofs_statfs,	/* statfs */
    &virtiofs_vnops	    /* vnops */
};

static int
virtiofs_mount(struct mount *mp, const char *dev, int flags, const void *data)
{
    struct device *device;
    struct virtiofs_info *virtiofs = nullptr;
    struct virtiofs_super_block *sb = nullptr;
    int error = -1;

    error = device_open(dev + 5, DO_RDWR, &device);
    if (error) {
        kprintf("[virtiofs] Error opening device!\n");
        return error;
    }

    // Save a reference to our superblock
    mp->m_data = virtiofs;
    mp->m_dev = device;

    virtiofs_set_vnode(mp->m_root->d_vnode, virtiofs->inodes);

    print("[virtiofs] returning from mount\n");

    return 0;
}

static int virtiofs_sync(struct mount *mp) {
    return 0;
}

static int virtiofs_statfs(struct mount *mp, struct statfs *statp)
{
    struct virtiofs_info *virtiofs = (struct virtiofs_info *) mp->m_data;
    struct virtiofs_super_block *sb = virtiofs->sb;

    statp->f_bsize = sb->block_size;

    // Total blocks
    statp->f_blocks = sb->structure_info_blocks_count + sb->structure_info_first_block;
    // Read only. 0 blocks free
    statp->f_bfree = 0;
    statp->f_bavail = 0;

    statp->f_ffree = 0;
    statp->f_files = sb->inodes_count; //Needs to be inode count

    statp->f_namelen = 0; //FIXME - unlimited ROFS_FILENAME_MAXLEN;

    return 0;
}

static int
virtiofs_unmount(struct mount *mp, int flags)
{
    struct virtiofs_info *virtiofs = (struct virtiofs_info *) mp->m_data;
    struct virtiofs_super_block *sb = virtiofs->sb;
    struct device *dev = mp->m_dev;

    int error = device_close(dev);
    delete sb;
    delete virtiofs;

    return error;
}
