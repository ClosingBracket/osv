/*
 * Copyright (C) 2020 Waldemar Kozaczuk
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <sys/types.h>
#include <osv/device.h>
#include <osv/debug.h>
#include <iomanip>
#include <iostream>
#include "virtiofs.hh"
#include "virtiofs_io.hh"

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
    int error = -1;

    error = device_open(dev + 5, DO_RDWR, &device);
    if (error) {
        kprintf("[virtiofs] Error opening device!\n");
        return error;
    }

    mp->m_dev = device;

    auto req = new fuse_request();
    req->in_header.len = 0; //TODO
    req->in_header.opcode = FUSE_INIT;
    req->in_header.unique = 1; //TODO
    req->in_header.nodeid = 1; //???
    req->in_header.uid = 0;
    req->in_header.gid = 0;
    req->in_header.pid = 0;

    auto fuse_init = new fuse_init_in();
    fuse_init->major = FUSE_KERNEL_VERSION;
    fuse_init->minor = FUSE_KERNEL_MINOR_VERSION;
    fuse_init->max_readahead = PAGE_SIZE;
    fuse_init->flags = 0;
   /* ia->in.flags |=
		FUSE_ASYNC_READ | FUSE_POSIX_LOCKS | FUSE_ATOMIC_O_TRUNC |
		FUSE_EXPORT_SUPPORT | FUSE_BIG_WRITES | FUSE_DONT_MASK |
		FUSE_SPLICE_WRITE | FUSE_SPLICE_MOVE | FUSE_SPLICE_READ |
		FUSE_FLOCK_LOCKS | FUSE_HAS_IOCTL_DIR | FUSE_AUTO_INVAL_DATA |
		FUSE_DO_READDIRPLUS | FUSE_READDIRPLUS_AUTO | FUSE_ASYNC_DIO |
		FUSE_WRITEBACK_CACHE | FUSE_NO_OPEN_SUPPORT |
		FUSE_PARALLEL_DIROPS | FUSE_HANDLE_KILLPRIV | FUSE_POSIX_ACL |
		FUSE_ABORT_ERROR | FUSE_MAX_PAGES | FUSE_CACHE_SYMLINKS |
		FUSE_NO_OPENDIR_SUPPORT | FUSE_EXPLICIT_INVAL_DATA;*/
    //req->args.in_numargs = 1;

    auto fs_strategy = reinterpret_cast<fuse_strategy*>(device->private_data);
    assert(fs_strategy->drv);

    fs_strategy->make_request(fs_strategy->drv, req);
    fuse_req_wait(req);
    // TODO: Save a reference to the virtio::fs drivers instance above
    //mp->m_data = virtiofs;

    //virtiofs_set_vnode(mp->m_root->d_vnode, virtiofs->inodes);

    print("[virtiofs] returning from mount\n");

    return 0;
}

static int virtiofs_sync(struct mount *mp) {
    return 0;
}

static int virtiofs_statfs(struct mount *mp, struct statfs *statp)
{
    //struct virtiofs_info *virtiofs = (struct virtiofs_info *) mp->m_data;
    //struct virtiofs_super_block *sb = virtiofs->sb;

    //statp->f_bsize = sb->block_size;

    // Total blocks
    //statp->f_blocks = sb->structure_info_blocks_count + sb->structure_info_first_block;
    // Read only. 0 blocks free
    statp->f_bfree = 0;
    statp->f_bavail = 0;

    statp->f_ffree = 0;
    //statp->f_files = sb->inodes_count; //Needs to be inode count

    statp->f_namelen = 0; //FIXME - unlimited ROFS_FILENAME_MAXLEN;

    return 0;
}

static int
virtiofs_unmount(struct mount *mp, int flags)
{
    //struct virtiofs_info *virtiofs = (struct virtiofs_info *) mp->m_data;
    //struct virtiofs_super_block *sb = virtiofs->sb;
    struct device *dev = mp->m_dev;

    int error = device_close(dev);
    //delete sb;
    //delete virtiofs;

    return error;
}
