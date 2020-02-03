/*
 * Copyright (C) 2020 Waldemar Kozaczuk
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef VIRTIO_BLK_DRIVER_H
#define VIRTIO_BLK_DRIVER_H

#include <osv/mutex.h>
#include <osv/waitqueue.hh>
#include "drivers/virtio.hh"
#include "drivers/virtio-device.hh"
#include "fs/virtiofs/fuse_kernel.h"

namespace virtio {

enum {
   VQ_HIPRIO,
   VQ_REQUEST
};

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

class fs : public virtio_driver {
public:

    struct fs_config {
        char tag[36];
        u32 num_queues;
    } __attribute__((packed));

    explicit fs(virtio_device& dev);
    virtual ~fs();

    virtual std::string get_name() const { return _driver_name; }
    void read_config();

    virtual u32 get_driver_features();

    int make_request(struct fuse_request*);

    void req_done();
    int64_t size();

    void set_readonly() {_ro = true;}
    bool is_readonly() {return _ro;}

    bool ack_irq();

    static hw_driver* probe(hw_device* dev);
private:
    struct fs_req {
        fs_req(struct fuse_request* f) :fuse_req(f) {};
        ~fs_req() {};

        struct fuse_request* fuse_req;
    };

    std::string _driver_name;
    fs_config _config;

    //maintains the virtio instance number for multiple drives
    static int _instance;
    int _id;
    bool _ro;
    // This mutex protects parallel make_request invocations
    mutex _lock;
};

}
#endif

