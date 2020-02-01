/*
 * Copyright (C) 2020 Waldemar Kozaczuk
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "virtiofs.hh"
#include <sys/types.h>
#include <osv/device.h>
#include <osv/debug.h>
#include <iomanip>
#include <iostream>

static int virtiofs_mount(struct mount *mp, const char *dev, int flags, const void *data);
static int virtiofs_sync(struct mount *mp);
static int virtiofs_statfs(struct mount *mp, struct statfs *statp);
static int virtiofs_unmount(struct mount *mp, int flags);

#define virtiofs_vget ((vfsop_vget_t)vfs_nullop)

#if defined(ROFS_DIAGNOSTICS_ENABLED)
std::atomic<long> virtiofs_block_read_ms(0);
std::atomic<long> virtiofs_block_read_count(0);
std::atomic<long> virtiofs_block_allocated(0);
std::atomic<long> virtiofs_cache_reads(0);
std::atomic<long> virtiofs_cache_misses(0);
#endif

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
    struct virtiofs_info *virtiofs = nullptr;
    struct virtiofs_super_block *sb = nullptr;
    int error = -1;

    error = device_open(dev + 5, DO_RDWR, &device);
    if (error) {
        kprintf("[virtiofs] Error opening device!\n");
        return error;
    }

    void *buf = malloc(BSIZE); //Just enough for single block of 512 bytes
    error = virtiofs_read_blocks(device, ROFS_SUPERBLOCK_BLOCK, 1, buf);
    if (error) {
        kprintf("[virtiofs] Error reading virtiofs superblock\n");
        device_close(device);
        free(buf);
        return error;
    }

    // We see if the file system is ROFS, if not, return error and close everything
    sb = (struct virtiofs_super_block *) buf;
    if (sb->magic != ROFS_MAGIC) {
        print("[virtiofs] Error magics do not match!\n");
        print("[virtiofs] Expecting %016llX but got %016llX\n", ROFS_MAGIC, sb->magic);
        free(buf);
        device_close(device);
        return -1; // TODO: Proper error code
    }

    if (sb->version != ROFS_VERSION) {
        kprintf("[virtiofs] Found virtiofs volume but incompatible version!\n");
        kprintf("[virtiofs] Expecting %llu but found %llu\n", ROFS_VERSION, sb->version);
        free(buf);
        device_close(device);
        return -1;
    }

    print("[virtiofs] Got superblock version:   0x%016llX\n", sb->version);
    print("[virtiofs] Got magic:                0x%016llX\n", sb->magic);
    print("[virtiofs] Got block size:                  %d\n", sb->block_size);
    print("[virtiofs] Got structure info first block:  %d\n", sb->structure_info_first_block);
    print("[virtiofs] Got structure info blocks count: %d\n", sb->structure_info_blocks_count);
    print("[virtiofs] Got directory entries count:     %d\n", sb->directory_entries_count);
    print("[virtiofs] Got symlinks count:              %d\n", sb->symlinks_count);
    print("[virtiofs] Got inode count:                 %d\n", sb->inodes_count);
    //
    // Since we have found ROFS, we can copy the superblock now
    sb = new virtiofs_super_block;
    memcpy(sb, buf, ROFS_SUPERBLOCK_SIZE);
    free(buf);
    //
    // Read structure_info_blocks_count to construct array of directory enries, symlinks and i-nodes
    buf = malloc(BSIZE * sb->structure_info_blocks_count);
    error = virtiofs_read_blocks(device, sb->structure_info_first_block, sb->structure_info_blocks_count, buf);
    if (error) {
        kprintf("[virtiofs] Error reading virtiofs structure info blocks\n");
        device_close(device);
        free(buf);
        return error;
    }

    virtiofs = new struct virtiofs_info;
    virtiofs->sb = sb;
    virtiofs->dir_entries = (struct virtiofs_dir_entry *) malloc(sizeof(struct virtiofs_dir_entry) * sb->directory_entries_count);

    void *data_ptr = buf;
    //
    // Read directory entries
    for (unsigned int idx = 0; idx < sb->directory_entries_count; idx++) {
        struct virtiofs_dir_entry *dir_entry = &(virtiofs->dir_entries[idx]);
        dir_entry->inode_no = *((uint64_t *) data_ptr);
        data_ptr += sizeof(uint64_t);

        unsigned short *filename_size = (unsigned short *) data_ptr;
        data_ptr += sizeof(unsigned short);

        dir_entry->filename = (char *) malloc(*filename_size + 1);
        strncpy(dir_entry->filename, (char *) data_ptr, *filename_size);
        dir_entry->filename[*filename_size] = 0;
        print("[virtiofs] i-node: %d -> directory entry: %s\n", dir_entry->inode_no, dir_entry->filename);
        data_ptr += *filename_size * sizeof(char);
    }
    //
    // Read symbolic links
    virtiofs->symlinks = (char **) malloc(sizeof(char *) * sb->symlinks_count);

    for (unsigned int idx = 0; idx < sb->symlinks_count; idx++) {
        unsigned short *symlink_path_size = (unsigned short *) data_ptr;
        data_ptr += sizeof(unsigned short);

        virtiofs->symlinks[idx] = (char *) malloc(*symlink_path_size + 1);
        strncpy(virtiofs->symlinks[idx], (char *) data_ptr, *symlink_path_size);
        virtiofs->symlinks[idx][*symlink_path_size] = 0;
        print("[virtiofs] symlink: %s\n", virtiofs->symlinks[idx]);
        data_ptr += *symlink_path_size * sizeof(char);
    }
    //
    // Read i-nodes
    virtiofs->inodes = (struct virtiofs_inode *) malloc(sizeof(struct virtiofs_inode) * sb->inodes_count);
    memcpy(virtiofs->inodes, data_ptr, sb->inodes_count * sizeof(struct virtiofs_inode));

    for (unsigned int idx = 0; idx < sb->inodes_count; idx++) {
        print("[virtiofs] inode: %d, size: %d\n", virtiofs->inodes[idx].inode_no, virtiofs->inodes[idx].file_size);
    }

    free(buf);

    // Save a reference to our superblock
    mp->m_data = virtiofs;
    mp->m_dev = device;

    virtiofs_set_vnode(mp->m_root->d_vnode, virtiofs->inodes);

    print("[virtiofs] returning from mount\n");

    return 0;
}

static int virtiofs_sync(struct mount *mp) {
    return 0;
}

static int virtiofs_statfs(struct mount *mp, struct statfs *statp)
{
    struct virtiofs_info *virtiofs = (struct virtiofs_info *) mp->m_data;
    struct virtiofs_super_block *sb = virtiofs->sb;

    statp->f_bsize = sb->block_size;

    // Total blocks
    statp->f_blocks = sb->structure_info_blocks_count + sb->structure_info_first_block;
    // Read only. 0 blocks free
    statp->f_bfree = 0;
    statp->f_bavail = 0;

    statp->f_ffree = 0;
    statp->f_files = sb->inodes_count; //Needs to be inode count

    statp->f_namelen = 0; //FIXME - unlimited ROFS_FILENAME_MAXLEN;

    return 0;
}

static int
virtiofs_unmount(struct mount *mp, int flags)
{
    struct virtiofs_info *virtiofs = (struct virtiofs_info *) mp->m_data;
    struct virtiofs_super_block *sb = virtiofs->sb;
    struct device *dev = mp->m_dev;

    int error = device_close(dev);
    delete sb;
    delete virtiofs;

#if defined(ROFS_DIAGNOSTICS_ENABLED)
    debugf("ROFS: spent %.2f ms reading from disk\n", ((double) virtiofs_block_read_ms.load()) / 1000);
    debugf("ROFS: read %d 512-byte blocks from disk\n", virtiofs_block_read_count.load());
    debugf("ROFS: allocated %d 512-byte blocks of cache memory\n", virtiofs_block_allocated.load());
    long total_cache_reads = virtiofs_cache_reads.load();
    double hit_ratio = total_cache_reads > 0 ? (virtiofs_cache_reads.load() - virtiofs_cache_misses.load()) / ((double)total_cache_reads) : 0;
    debugf("ROFS: hit ratio is %.2f%%\n", hit_ratio * 100);
#endif
    return error;
}
