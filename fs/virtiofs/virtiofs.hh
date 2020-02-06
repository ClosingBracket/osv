/*
 * Copyright (C) 2020 Waldemar Kozaczuk
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

//
// The Read-Only File System (VIRTIOFS) provides simple implementation of
// a file system where data from disk can only be read from and never
// written to. It is simple enough for many stateless applications
// deployed on OSv which only need to read code from local disk and never
// write to local disk. The VIRTIOFS is inspired and shares some ideas
// from the original MFS implementation by James Root from 2015.
//

#ifndef __INCLUDE_VIRTIOFS_H__
#define __INCLUDE_VIRTIOFS_H__

#include <osv/vnode.h>
#include <osv/mount.h>
#include <osv/dentry.h>
#include <osv/prex.h>
#include <osv/buf.h>
#include "fuse_kernel.h"

//#define VIRTIOFS_DEBUG_ENABLED 1

#if defined(VIRTIOFS_DEBUG_ENABLED)
#define print(...) kprintf(__VA_ARGS__)
#else
#define print(...)
#endif

struct virtiofs_inode {
    uint64_t nodeid;
    struct fuse_attr attr;
};

extern struct vfsops virtiofs_vfsops;
extern struct vnops virtiofs_vnops;

#endif
