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

static inline struct smb2_context *get_smb2_context(struct vnode *node,
                                                    int &err_no)
{
    return smbfs::get_mount_context(node->v_mount, err_no)->smbfs();
}

static inline struct smb2fh *get_file_handle(struct vnode *node)
{
    return static_cast<struct smb2fh *>(node->v_data);
}

static inline struct smb2dir *get_dir_handle(struct vnode *node)
{
    return static_cast<struct smb2dir *>(node->v_data);
}

static int smbfs_open(struct file *fp)
{
    struct vnode *vp = file_dentry(fp)->d_vnode;
    std::string path(fp->f_dentry->d_path);
    int err_no;
    auto smb2 = get_smb2_context(vp, err_no);
    int flags = file_flags(fp);
    //int ret = 0;

    if (err_no) {
        return err_no;
    }

    // already opened reuse the nfs handle
    if (vp->v_data) {
        return 0;
    }

    int type = vp->v_type;

    // clear read write flags
    flags &= ~(O_RDONLY | O_WRONLY | O_RDWR);

    // check our rights
    bool read  = !vn_access(vp, VREAD);
    bool write = !vn_access(vp, VWRITE);

    // Set updated flags
    if (read && write) {
        flags |= O_RDWR;
    } else if (read) {
        flags |= O_RDONLY;
    } else if (write) {
        flags |= O_WRONLY;
    }

    // It's a directory or a file.
    if (type == VDIR) {
        struct smb2dir *handle = smb2_opendir(smb2, path.c_str());
        if (handle) {
            vp->v_data = handle;
        }
        //TODO pass error and set errno
    } else if (type == VREG) {
        struct smb2fh *handle = smb2_open(smb2, path.c_str(), flags);
        if (handle) {
            vp->v_data = handle;
        }
        //TODO pass error and set errno
    } else {
        return EIO;
    }
    /*
    if (ret) {
        return -ret;
    }*/

    return 0;
}

static int smbfs_close(struct vnode *vp, struct file *fp)
{
    // not opened - return error
    if (!vp->v_data) {
        return -1; //TODO: Set proper errno
    }

    int err_no;
    auto smb2 = get_smb2_context(vp, err_no);
    if (err_no) {
        return err_no;
    }

    int type = vp->v_type;
    //
    // It's a directory or a file.
    if (type == VDIR) {
        smb2_closedir(smb2, get_dir_handle(vp));
        vp->v_data = nullptr;
        //TODO set errno
    } else if (type == VREG) {
        smb2_close(smb2, get_file_handle(vp));
        vp->v_data = nullptr;
        //TODO set errno
    } else {
        return EIO;
    }
    /*
    if (ret) {
        return -ret;
    }*/

    return 0;
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
#define smbfs_readlink    ((vnop_readlink_t)vop_nullop)

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
