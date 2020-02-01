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

#define VIRTIOFS_VERSION            1
#define VIRTIOFS_MAGIC              0xDEADBEAD

#define VIRTIOFS_INODE_SIZE ((uint64_t)sizeof(struct virtiofs_inode))

#define VIRTIOFS_SUPERBLOCK_SIZE sizeof(struct virtiofs_super_block)
#define VIRTIOFS_SUPERBLOCK_BLOCK 0

//#define VIRTIOFS_DEBUG_ENABLED 1

#if defined(VIRTIOFS_DEBUG_ENABLED)
#define print(...) kprintf(__VA_ARGS__)
#else
#define print(...)
#endif

#define VIRTIOFS_DIAGNOSTICS_ENABLED 1

#if defined(VIRTIOFS_DIAGNOSTICS_ENABLED)
#define VIRTIOFS_STOPWATCH_START auto begin = std::chrono::high_resolution_clock::now();
#define VIRTIOFS_STOPWATCH_END(total) auto end = std::chrono::high_resolution_clock::now(); \
std::chrono::duration<double> sec = end - begin; \
total += ((long)(sec.count() * 1000000));
//TODO: Review - avoid conversions
#else
#define VIRTIOFS_STOPWATCH_START
#define VIRTIOFS_STOPWATCH_END(...)
#endif

extern struct vfsops virtiofs_vfsops;
extern struct vnops virtiofs_vnops;

#endif
