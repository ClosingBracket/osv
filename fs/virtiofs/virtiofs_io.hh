/*
 * Copyright (C) 2020 Waldemar Kozaczuk
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef VIRTIOFS_IO_H
#define VIRTIOFS_IO_H

#include "fuse_kernel.h"
#include <osv/mutex.h>
#include <osv/waitqueue.hh>

struct fuse_input_arg
{
    unsigned size;
    const void *value;
};

struct fuse_output_arg
{
    unsigned size;
    void *value;
};

struct fuse_request
{   //Combined fuse_req with fuse_args from fuse_i.h
    struct fuse_in_header in_header;
    struct fuse_out_header out_header;

    uint64_t node_id;
    uint32_t opcode;

    unsigned short num_of_input_args;
    unsigned short num_of_output_args;

    struct fuse_input_arg input_args[3];
    struct fuse_output_arg output_args[2];

    mutex_t req_mutex;
    waitqueue req_wait;
};

void fuse_req_wait(struct fuse_request* req);

struct fuse_strategy {
    void *drv;
    int (*make_request)(void*, struct fuse_request*);
};
#endif
