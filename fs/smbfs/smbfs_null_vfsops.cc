/*
 * Copyright (C) 2015 Scylla, Ltd.
 *
 * Based on ramfs code Copyright (c) 2006-2007, Kohsuke Ohtani
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <dlfcn.h>
#include <osv/mount.h>
#include <osv/debug.h>

#define smbfs_mount   ((vfsop_mount_t)vfs_nullop)
#define smbfs_umount  ((vfsop_umount_t)vfs_nullop)
#define smbfs_sync    ((vfsop_sync_t)vfs_nullop)
#define smbfs_vget    ((vfsop_vget_t)vfs_nullop)
#define smbfs_statfs  ((vfsop_statfs_t)vfs_nullop)

/*
 * File system operations
 *
 * This desactivate the NFS file system when libsmbfs is not compiled in.
 *
 */
struct vfsops smbfs_vfsops = {
    smbfs_mount,      /* mount */
    smbfs_umount,     /* umount */
    smbfs_sync,       /* sync */
    smbfs_vget,       /* vget */
    smbfs_statfs,     /* statfs */
    nullptr,          /* vnops */
};

int smbfs_init(void)
{
    return 0;
}

int smbfs_set_vfsops(void)
{
    debug("VFS: Starting to initialize SMBFS\n");

    void* module = dlopen("smbfs.so", RTLD_LAZY);
    if (module == nullptr) {
        debug("VFS: Failed to find smbfs.so\n");
        return 0;
    }

    using get_vfsops_func_t = struct vfsops* ();
    get_vfsops_func_t* get_vfsops = reinterpret_cast<get_vfsops_func_t*>(dlsym(module, "get_vfsops"));
    if (get_vfsops == nullptr) {
        debug("VFS: Failed to find function get_vfsops in smbfs.so\n");
        dlclose(module);
        return 0;
    }

    struct vfsops* _vfsops = get_vfsops();
    if (_vfsops) {
       smbfs_vfsops.vfs_mount = _vfsops->vfs_mount;
       smbfs_vfsops.vfs_unmount = _vfsops->vfs_unmount;
       smbfs_vfsops.vfs_sync = _vfsops->vfs_sync;
       smbfs_vfsops.vfs_vget = _vfsops->vfs_vget;
       smbfs_vfsops.vfs_statfs = _vfsops->vfs_statfs;
       smbfs_vfsops.vfs_vnops = _vfsops->vfs_vnops;
       debug("VFS: Initialized SMBFS\n");
    }

    debug("VFS: Done initializing SMBFS\n");
    return 0;
}
