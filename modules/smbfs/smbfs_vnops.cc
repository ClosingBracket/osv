/*
 * Copyright (C) 2018 Waldemar Kozaczuk
 * Loosely based on ROFS and OSv NFS implementation by Waldemar Kozaczuk and Benoit Canet respectively
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <sys/stat.h>
#include <dirent.h>
#include <sys/param.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#include <osv/prex.h>
#include <osv/vnode.h>
#include <osv/file.h>

#include <sys/types.h>
#include "smbfs.hh"

static int smbfs_open(struct file *fp)
{
}

static int smbfs_close(struct vnode *vp, struct file *fp)
{
}

//
// This function reads symbolic link information
static int smbfs_readlink(struct vnode *vnode, struct uio *uio)
{
}

//
// This function reads as much data as requested per uio
static int smbfs_read(struct vnode *vnode, struct file *fp, struct uio *uio, int ioflag)
{
}

//
// This functions reads directory information (dentries) based on information in memory
// under rofs->dir_entries table
static int smbfs_readdir(struct vnode *vnode, struct file *fp, struct dirent *dir)
{
}

//
// This functions looks up directory entry based on the directory information stored in memory
// under rofs->dir_entries table
static int smbfs_lookup(struct vnode *vnode, char *name, struct vnode **vpp)
{
}

static int smbfs_getattr(struct vnode *vnode, struct vattr *attr)
{
}

#define smbfs_write       ((vnop_write_t)vop_erofs)
#define smbfs_seek        ((vnop_seek_t)vop_nullop)
#define smbfs_ioctl       ((vnop_ioctl_t)vop_nullop)
#define smbfs_create      ((vnop_create_t)vop_erofs)
#define smbfs_remove      ((vnop_remove_t)vop_erofs)
#define smbfs_rename      ((vnop_rename_t)vop_erofs)
#define smbfs_mkdir       ((vnop_mkdir_t)vop_erofs)
#define smbfs_rmdir       ((vnop_rmdir_t)vop_erofs)
#define smbfs_setattr     ((vnop_setattr_t)vop_erofs)
#define smbfs_inactive    ((vnop_inactive_t)vop_nullop)
#define smbfs_truncate    ((vnop_truncate_t)vop_erofs)
#define smbfs_link        ((vnop_link_t)vop_erofs)
#define smbfs_arc         ((vnop_cache_t) nullptr)
#define smbfs_fallocate   ((vnop_fallocate_t)vop_erofs)
#define smbfs_fsync       ((vnop_fsync_t)vop_nullop)
#define smbfs_symlink     ((vnop_symlink_t)vop_erofs)

struct vnops smbfs_vnops = {
    smbfs_open,       /* open */
    smbfs_close,      /* close */
    smbfs_read,       /* read */
    smbfs_write,      /* write - returns error when called */
    smbfs_seek,       /* seek */
    smbfs_ioctl,      /* ioctl */
    smbfs_fsync,      /* fsync */
    smbfs_readdir,    /* readdir */
    smbfs_lookup,     /* lookup */
    smbfs_create,     /* create - returns error when called */
    smbfs_remove,     /* remove - returns error when called */
    smbfs_rename,     /* rename - returns error when called */
    smbfs_mkdir,      /* mkdir - returns error when called */
    smbfs_rmdir,      /* rmdir - returns error when called */
    smbfs_getattr,    /* getattr */
    smbfs_setattr,    /* setattr - returns error when called */
    smbfs_inactive,   /* inactive */
    smbfs_truncate,   /* truncate - returns error when called*/
    smbfs_link,       /* link - returns error when called*/
    smbfs_arc,        /* arc */
    smbfs_fallocate,  /* fallocate - returns error when called*/
    smbfs_readlink,   /* read link */
    smbfs_symlink     /* symbolic link - returns error when called*/
};
