/*
 * Copyright (C) 2020 Waldemar Kozaczuk
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef VIRTIO_BLK_DRIVER_H
#define VIRTIO_BLK_DRIVER_H
#include "drivers/virtio.hh"
#include "drivers/virtio-device.hh"

namespace virtio {

class fs : public virtio_driver {
public:

    enum blk_request_type {
        VIRTIO_BLK_T_IN = 0,
        VIRTIO_BLK_T_OUT = 1,
        /* This bit says it's a scsi command, not an actual read or write. */
        VIRTIO_BLK_T_SCSI_CMD = 2,
        /* Cache flush command */
        VIRTIO_BLK_T_FLUSH = 4,
        /* Get device ID command */
        VIRTIO_BLK_T_GET_ID = 8,
        /* Barrier before this op. */
        VIRTIO_BLK_T_BARRIER = 0x80000000,
    };

    enum blk_res_code {
        /* And this is the final byte of the write scatter-gather list. */
        VIRTIO_BLK_S_OK = 0,
        VIRTIO_BLK_S_IOERR = 1,
        VIRTIO_BLK_S_UNSUPP = 2,
    };

    struct virtio_fs_config {
        char tag[36];
        u32 num_queues;
    } __attribute__((packed));

    /* This is the first element of the read scatter-gather list. */
    struct blk_outhdr {
        /* VIRTIO_BLK_T* */
        u32 type;
        /* io priority. */
        u32 ioprio;
        /* Sector (ie. 512 byte offset) */
        u64 sector;
    };

    struct blk_res {
        u8 status;
    };

    explicit fs(virtio_device& dev);
    virtual ~fs();

    virtual std::string get_name() const { return _driver_name; }
    void read_config();

    virtual u32 get_driver_features();

    int make_request(struct bio*);

    void req_done();
    int64_t size();

    void set_readonly() {_ro = true;}
    bool is_readonly() {return _ro;}

    bool ack_irq();

    static hw_driver* probe(hw_device* dev);
private:

    struct blk_req {
        blk_req(struct bio* b) :bio(b) {};
        ~blk_req() {};

        blk_outhdr hdr;
        blk_res res;
        struct bio* bio;
    };

    std::string _driver_name;
    blk_config _config;

    //maintains the virtio instance number for multiple drives
    static int _instance;
    int _id;
    bool _ro;
    // This mutex protects parallel make_request invocations
    mutex _lock;
};

}
#endif

