/*
 * Copyright (C) 2018 Waldemar Kozaczuk
 * Loosely based on ROFS and OSv NFS implementation by Waldemar Kozaczuk and Benoit Canet respectively
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <osv/vnode.h>
#include <osv/mount.h>
#include <osv/dentry.h>

#include "smbfs.hh"

using namespace smbfs;

/*
 * Mount a file system.
 */
static int
smbfs_mount(struct mount *mp, const char *dev, int flags, const void *data)
{
    assert(mp);

    // build a temporary mount context to check the SMB2/3 server is alive
    int err_no;
    std::unique_ptr<mount_context> ctx(new mount_context(mp->m_special));

    if (!ctx->is_valid(err_no)) {
        return err_no;
    }

    return 0;
}

static int
smbfs_unmount(struct mount *mp, int flags)
{
    assert(mp);

    // Make sure nothing is used under this mount point.
    if (mp->m_count > 1) {
        return EBUSY;
    }

    return 0;
}

// For the following let's rely on operations on individual files
#define smbfs_sync    ((vfsop_sync_t)vfs_nullop)
#define smbfs_vget    ((vfsop_vget_t)vfs_nullop)
#define smbfs_statfs  ((vfsop_statfs_t)vfs_nullop)

extern struct vfsops smbfs_vfsops;

void __attribute__((constructor)) initialize_vfsops() {
    smbfs_vfsops.vfs_mount = smbfs_mount;
    smbfs_vfsops.vfs_unmount = smbfs_unmount;
    smbfs_vfsops.vfs_sync = smbfs_sync;
    smbfs_vfsops.vfs_vget = smbfs_vget;
    smbfs_vfsops.vfs_statfs = smbfs_statfs;
    smbfs_vfsops.vfs_vnops = &smbfs_vnops;
}
