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

static const char *get_node_name(struct vnode *node)
{
    if (LIST_EMPTY(&node->v_names) == 1) {
        return nullptr;
    }

    return LIST_FIRST(&node->v_names)->d_path;
}

static inline struct timespec to_timespec(uint64_t sec, uint64_t nsec)
{
    struct timespec t;

    t.tv_sec = sec;
    t.tv_nsec = nsec;

    return t;
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
static int smbfs_read(struct vnode *vp, struct file *fp, struct uio *uio, int ioflag)
{
}

//
// This functions reads directory information (dentries) based on information in memory
// under rofs->dir_entries table
static int smbfs_readdir(struct vnode *vp, struct file *fp, struct dirent *dir)
{
}

//
// This functions looks up directory entry based on the directory information stored in memory
// under rofs->dir_entries table
static int smbfs_lookup(struct vnode *vp, char *name, struct vnode **vpp)
{
}

static int smbfs_getattr(struct vnode *vp, struct vattr *attr)
{
    int err_no;
    auto smb2 = get_smb2_context(vp, err_no);
    if (err_no) {
        return err_no;
    }

    auto path = get_node_name(vp);
    if (!path) {
        return ENOENT;
    }

    // Get the file infos.
    struct smb2_stat_64 st;
    int ret = smb2_stat(smb2, path, &st);
    if (ret) {
        return -ret;
    }

    //uint32_t smb2_type;

    //uint64_t type = st.nfs_mode & S_IFMT;
    //uint64_t mode = st.nfs_mode & ~S_IFMT;

    // Copy the file infos.
    //attr->va_mask    =;
    //TODO: attr->va_type    = IFTOVT(type);
    //TODO: attr->va_mode    = mode;
    attr->va_nlink   = st.smb2_nlink;
    //attr->va_uid     = st.smb2_uid;
    //attr->va_gid     = st.smb2_gid;
    //attr->va_fsid    = st.smb2_dev; // FIXME: not sure about this one
    attr->va_nodeid  = st.smb2_ino;
    attr->va_atime   = to_timespec(st.smb2_atime, st.smb2_atime_nsec);
    attr->va_mtime   = to_timespec(st.smb2_mtime, st.smb2_mtime_nsec);
    attr->va_ctime   = to_timespec(st.smb2_ctime, st.smb2_ctime_nsec);
    //attr->va_rdev    = st.smb2_rdev;
    //attr->va_nblocks = st.smb2_blocks;
    attr->va_size    = st.smb2_size;

    return 0;
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
