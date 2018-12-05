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

struct vfsops smbfs_vfsops = {
    smbfs_mount,	/* mount */
    smbfs_unmount,	/* unmount */
    smbfs_sync,		/* sync */
    smbfs_vget,     /* vget */
    smbfs_statfs,	/* statfs */
    &smbfs_vnops	/* vnops */
};