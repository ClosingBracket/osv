/*
 * Copyright (C) 2020 Waldemar Kozaczuk
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
#include <osv/mount.h>
#include <osv/debug.h>

#include <sys/types.h>
#include <osv/device.h>
#include <osv/sched.hh>

#include "virtiofs.hh"
#include "virtiofs_i.hh"

#define VERIFY_READ_INPUT_ARGUMENTS() \
    /* Cant read directories */\
    if (vnode->v_type == VDIR) \
        return EISDIR; \
    /* Cant read anything but reg */\
    if (vnode->v_type != VREG) \
        return EINVAL; \
    /* Cant start reading before the first byte */\
    if (uio->uio_offset < 0) \
        return EINVAL; \
    /* Need to read more than 1 byte */\
    if (uio->uio_resid == 0) \
        return 0; \
    /* Cant read after the end of the file */\
    if (uio->uio_offset >= (off_t)vnode->v_size) \
        return 0;

/*
static int
virtiofs_read_blocks(struct device *device, uint64_t starting_block, uint64_t blocks_count, void *buf)
{
    struct bio *bio = alloc_bio();
    if (!bio)
        return ENOMEM;

    bio->bio_cmd = BIO_READ;
    bio->bio_dev = device;
    bio->bio_data = buf;
    bio->bio_offset = starting_block << 9;
    bio->bio_bcount = blocks_count * BSIZE;

    bio->bio_dev->driver->devops->strategy(bio);
    int error = bio_wait(bio);

    destroy_bio(bio);

#if defined(ROFS_DIAGNOSTICS_ENABLED)
    virtiofs_block_read_count += blocks_count;
#endif
    ROFS_STOPWATCH_END(virtiofs_block_read_ms)

    return error;
}*/

int virtiofs_init(void) {
    return 0;
}

//
// This functions looks up directory entry based on the directory information stored in memory
// under virtiofs->dir_entries table
static int virtiofs_lookup(struct vnode *vnode, char *name, struct vnode **vpp)
{
    struct virtiofs_inode *inode = (struct virtiofs_inode *) vnode->v_data;
    struct vnode *vp = nullptr;

    if (*name == '\0') {
        return ENOENT;
    }

    if (!S_ISDIR(inode->attr.mode)) {
        print("[virtiofs] ABORTED lookup up %s at inode %d because not a directory\n", name, inode->inode_no);
        return ENOTDIR;
    }

    auto *out_args = new (std::nothrow) fuse_entry_out();
    auto input = new char[strlen(name) + 1];
    strcpy(input, name);
    auto *req = create_fuse_request(FUSE_LOOKUP, inode->nodeid, input, strlen(name) + 1, out_args, sizeof(*out_args));

    auto *fs_strategy = reinterpret_cast<fuse_strategy*>(vnode->v_mount->m_data);
    assert(fs_strategy->drv);

    fs_strategy->make_request(fs_strategy->drv, req);
    fuse_req_wait(req);

    int error = -req->out_header.error;

    if (!error) {
        if (vget(vnode->v_mount, out_args->nodeid, &vp)) { //TODO: Will it ever work? Revisit
            printf("[virtiofs] rofs_lookup found vp in cache!\n");
            *vpp = vp;
            return 0;
        }

        auto *inode = new virtiofs_inode();
        inode->nodeid = out_args->nodeid;
        printf("[virtiofs] rofs_lookup found inode: %d for %s!\n", inode->nodeid, name);
        memcpy(&inode->attr, &out_args->attr, sizeof(out_args->attr));

        virtiofs_set_vnode(vp, inode);
        *vpp = vp;
    }

    delete req;
    delete input;
    delete out_args;

    return error;
    /*
            if (vget(vnode->v_mount, inode_no, &vp)) { //TODO: Will it ever work? Revisit
                print("[virtiofs] found vp in cache!\n");
                *vpp = vp;
                return 0;
            }

            struct virtiofs_inode *found_inode = virtiofs->inodes + (inode_no - 1); //Check if exists
            virtiofs_set_vnode(vp, found_inode);

            print("[virtiofs] found the directory entry [%s] at at inode %d -> %d!\n", name, inode->inode_no,
                  found_inode->inode_no);

            *vpp = vp;
            return 0;*/

    //print("[virtiofs] FAILED to find up %s\n", name);
    //return ENOENT;
}

static int virtiofs_open(struct file *fp)
{
    if ((file_flags(fp) & FWRITE)) {
        // Do no allow opening files to write
        return (EROFS);
    }

    auto *out_args = new (std::nothrow) fuse_open_out();
    auto *input_args = new (std::nothrow) fuse_open_in();
    //memset(input_args, 0, sizeof(*input_args));
    input_args->flags = file_flags(fp);

    struct vnode *vnode = file_dentry(fp)->d_vnode;
    struct virtiofs_inode *inode = (struct virtiofs_inode *) vnode->v_data;
    auto *req = create_fuse_request(FUSE_OPEN, inode->nodeid, input_args, sizeof(*input_args), out_args, sizeof(*out_args));

    auto *fs_strategy = reinterpret_cast<fuse_strategy*>(vnode->v_mount->m_data);
    assert(fs_strategy->drv);

    fs_strategy->make_request(fs_strategy->drv, req);
    fuse_req_wait(req);

    auto error = -req->out_header.error;
    auto *file_data = new virtiofs_file_data();
    file_data->file_handle = out_args->fh;
    fp->f_data = file_data;

    printf("[virtiofs] virtiofs_open called for inode [%d] and got handle: [%ld]\n",
          ((struct virtiofs_inode *) fp->f_dentry.get()->d_vnode->v_data)->nodeid, file_data->file_handle);

    delete req;
    delete input_args;
    delete out_args;

    return error;
}

static int virtiofs_close(struct vnode *vp, struct file *fp) {
    printf("[virtiofs] virtiofs_close called\n");

    auto *input_args = new (std::nothrow) fuse_release_in();
    auto *file_data = reinterpret_cast<virtiofs_file_data*>(fp->f_data);
    input_args->fh = file_data->file_handle;

    struct virtiofs_inode *inode = (struct virtiofs_inode *) vp->v_data;
    auto *req = create_fuse_request(FUSE_RELEASE, inode->nodeid, input_args, sizeof(*input_args), nullptr, 0);

    auto *fs_strategy = reinterpret_cast<fuse_strategy*>(vp->v_mount->m_data);
    assert(fs_strategy->drv);

    fs_strategy->make_request(fs_strategy->drv, req);
    fuse_req_wait(req);

    auto error = -req->out_header.error;

    delete req;
    delete input_args;
    delete file_data;

    fp->f_data = nullptr;

    return error;
}

//
// This function reads symbolic link information from directory structure in memory
// under virtiofs->symlinks table
static int virtiofs_readlink(struct vnode *vnode, struct uio *uio)
{
    /*
    struct virtiofs_info *virtiofs = (struct virtiofs_info *) vnode->v_mount->m_data;
    struct virtiofs_inode *inode = (struct virtiofs_inode *) vnode->v_data;

    if (!S_ISLNK(inode->mode)) {
        return EINVAL; //This node is not a symbolic link
    }

    assert(inode->data_offset >= 0 && inode->data_offset < virtiofs->sb->symlinks_count);

    char *link_path = virtiofs->symlinks[inode->data_offset];

    print("[virtiofs] virtiofs_readlink returned link [%s]\n", link_path);
    return uiomove(link_path, strlen(link_path), uio);*/
    return 0;
}

//
// This function reads as much data as requested per uio in single read from the disk but
// the data does not get retained for subsequent reads
static int virtiofs_read(struct vnode *vnode, struct file *fp, struct uio *uio, int ioflag)
{
    struct virtiofs_inode *inode = (struct virtiofs_inode *) vnode->v_data;

    VERIFY_READ_INPUT_ARGUMENTS()

    // Total read amount is what they requested, or what is left
    uint64_t read_amt = std::min<uint64_t>(inode->attr.size - uio->uio_offset, uio->uio_resid);
    size_t buf_size = std::min<size_t>(8192,read_amt);
    void *buf = malloc(buf_size);

    auto *input_args = new (std::nothrow) fuse_read_in();
    auto *file_data = reinterpret_cast<virtiofs_file_data*>(fp->f_data);
    input_args->fh = file_data->file_handle;
    input_args->offset = uio->uio_offset; 
    input_args->size = read_amt;

    printf("[virtiofs] virtiofs_read [%d], inode: %d, at %d of %d bytes with handle: %ld\n",
          sched::thread::current()->id(), inode->nodeid, uio->uio_offset, read_amt, file_data->file_handle);
 
    auto *req = create_fuse_request(FUSE_READ, inode->nodeid, input_args, sizeof(*input_args), buf, buf_size);

    auto *fs_strategy = reinterpret_cast<fuse_strategy*>(vnode->v_mount->m_data);
    assert(fs_strategy->drv);

    fs_strategy->make_request(fs_strategy->drv, req);
    fuse_req_wait(req);

    auto error = -req->out_header.error;

    if (error) {
        kprintf("[virtiofs_read] Error reading data\n");
        free(buf);
        return error;
    }

    auto rv = uiomove(buf, read_amt, uio);

    free(buf);
    free(req);
    return rv;
}
//
// This functions reads directory information (dentries) based on information in memory
// under virtiofs->dir_entries table
static int virtiofs_readdir(struct vnode *vnode, struct file *fp, struct dirent *dir)
{
    /*
    struct virtiofs_info *virtiofs = (struct virtiofs_info *) vnode->v_mount->m_data;
    struct virtiofs_inode *inode = (struct virtiofs_inode *) vnode->v_data;

    uint64_t index = 0;

    if (!S_ISDIR(inode->mode)) {
        return ENOTDIR;
    }

    if (fp->f_offset == 0) {
        dir->d_type = DT_DIR;
        strlcpy((char *) &dir->d_name, ".", sizeof(dir->d_name));
    } else if (fp->f_offset == 1) {
        dir->d_type = DT_DIR;
        strlcpy((char *) &dir->d_name, "..", sizeof(dir->d_name));
    } else {
        index = fp->f_offset - 2;
        if (index >= inode->dir_children_count) {
            return ENOENT;
        }

        dir->d_fileno = fp->f_offset;

        // Set the name
        struct virtiofs_dir_entry *directory_entry = virtiofs->dir_entries + (inode->data_offset + index);
        strlcpy((char *) &dir->d_name, directory_entry->filename, sizeof(dir->d_name));
        dir->d_ino = directory_entry->inode_no;

        struct virtiofs_inode *directory_entry_inode = virtiofs->inodes + (dir->d_ino - 1);
        if (S_ISDIR(directory_entry_inode->mode))
            dir->d_type = DT_DIR;
        else if (S_ISLNK(directory_entry_inode->mode))
            dir->d_type = DT_LNK;
        else
            dir->d_type = DT_REG;
    }

    fp->f_offset++;*/

    return 0;
}

static int virtiofs_getattr(struct vnode *vnode, struct vattr *attr)
{
    printf("[virtiofs] virtiofs_getattr called\n");
    struct virtiofs_inode *inode = (struct virtiofs_inode *) vnode->v_data;

    attr->va_mode = 0555;

    if (S_ISDIR(inode->attr.mode)) {
        attr->va_type = VDIR;
    } else if (S_ISREG(inode->attr.mode)) {
        attr->va_type = VREG;
    } else if (S_ISLNK(inode->attr.mode)) {
        attr->va_type = VLNK;
    }

    //attr->va_nodeid = vnode->v_ino;
    attr->va_size = inode->attr.size;

    return 0;
}

#define virtiofs_write       ((vnop_write_t)vop_erofs)
#define virtiofs_seek        ((vnop_seek_t)vop_nullop)
#define virtiofs_ioctl       ((vnop_ioctl_t)vop_nullop)
#define virtiofs_create      ((vnop_create_t)vop_erofs)
#define virtiofs_remove      ((vnop_remove_t)vop_erofs)
#define virtiofs_rename      ((vnop_rename_t)vop_erofs)
#define virtiofs_mkdir       ((vnop_mkdir_t)vop_erofs)
#define virtiofs_rmdir       ((vnop_rmdir_t)vop_erofs)
#define virtiofs_setattr     ((vnop_setattr_t)vop_erofs)
#define virtiofs_inactive    ((vnop_inactive_t)vop_nullop)
#define virtiofs_truncate    ((vnop_truncate_t)vop_erofs)
#define virtiofs_link        ((vnop_link_t)vop_erofs)
#define virtiofs_arc         ((vnop_cache_t) nullptr)
#define virtiofs_fallocate   ((vnop_fallocate_t)vop_erofs)
#define virtiofs_fsync       ((vnop_fsync_t)vop_nullop)
#define virtiofs_symlink     ((vnop_symlink_t)vop_erofs)

struct vnops virtiofs_vnops = {
    virtiofs_open,      /* open */
    virtiofs_close,     /* close */
    virtiofs_read,      /* read */
    virtiofs_write,     /* write - returns error when called */
    virtiofs_seek,      /* seek */
    virtiofs_ioctl,     /* ioctl */
    virtiofs_fsync,     /* fsync */
    virtiofs_readdir,   /* readdir */
    virtiofs_lookup,    /* lookup */
    virtiofs_create,    /* create - returns error when called */
    virtiofs_remove,    /* remove - returns error when called */
    virtiofs_rename,    /* rename - returns error when called */
    virtiofs_mkdir,     /* mkdir - returns error when called */
    virtiofs_rmdir,     /* rmdir - returns error when called */
    virtiofs_getattr,   /* getattr */
    virtiofs_setattr,   /* setattr - returns error when called */
    virtiofs_inactive,  /* inactive */
    virtiofs_truncate,  /* truncate - returns error when called*/
    virtiofs_link,      /* link - returns error when called*/
    virtiofs_arc,       /* arc */ //TODO: Implement to allow memory re-use when mapping files
    virtiofs_fallocate, /* fallocate - returns error when called*/
    virtiofs_readlink,  /* read link */
    virtiofs_symlink    /* symbolic link - returns error when called*/
};
