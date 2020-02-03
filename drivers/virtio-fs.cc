/*
 * Copyright (C) 2020 Waldemar Kozaczuk
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <sys/cdefs.h>

#include "drivers/virtio.hh"
#include "drivers/virtio-fs.hh"
#include <osv/interrupt.hh>

#include <osv/mempool.hh>
#include <osv/mmu.hh>

#include <string>
#include <string.h>
#include <map>
#include <errno.h>
#include <osv/debug.h>

#include <osv/sched.hh>
#include "osv/trace.hh"
#include "osv/aligned_new.hh"

#include <osv/device.h>

//TRACEPOINT(trace_virtio_blk_make_request_seg_max, "request of size %d needs more segment than the max %d", size_t, u32);
//TRACEPOINT(trace_virtio_blk_make_request_readonly, "write on readonly device");
//TRACEPOINT(trace_virtio_blk_wake, "");
//TRACEPOINT(trace_virtio_blk_strategy, "bio=%p", struct bio*);
//TRACEPOINT(trace_virtio_blk_req_ok, "bio=%p, sector=%lu, len=%lu, type=%x", struct bio*, u64, size_t, u32);
//TRACEPOINT(trace_virtio_blk_req_unsupp, "bio=%p, sector=%lu, len=%lu, type=%x", struct bio*, u64, size_t, u32);
//TRACEPOINT(trace_virtio_blk_req_err, "bio=%p, sector=%lu, len=%lu, type=%x", struct bio*, u64, size_t, u32);

using namespace memory;


namespace virtio {

void fuse_req_wait(struct fuse_request* req)
{
    WITH_LOCK(req->req_mutex) {
        req->req_wait.wait(req->req_mutex);
    }
}

void fuse_req_done(struct fuse_request* req)
{
    WITH_LOCK(req->req_mutex) {
        req->req_wait.wake_one(req->req_mutex);
    }
}

int fs::_instance = 0;

bool fs::ack_irq()
{
    auto isr = _dev.read_and_ack_isr();
    auto queue = get_virt_queue(0);

    if (isr) {
        queue->disable_interrupts();
        return true;
    } else {
        return false;
    }

}

fs::fs(virtio_device& virtio_dev)
    : virtio_driver(virtio_dev), _ro(false)
{
    _driver_name = "virtio-fs";
    _id = _instance++;
    virtio_i("VIRTIO FS INSTANCE %d\n", _id);

    // Steps 4, 5 & 6 - negotiate and confirm features
    setup_features();
    read_config();

    // Step 7 - generic init of virtqueues
    probe_virt_queues();

    //register the single irq callback for the block
    sched::thread* t = sched::thread::make([this] { this->req_done(); },
            sched::thread::attr().name("virtio-fs"));
    t->start();
    auto queue = get_virt_queue(0);

    interrupt_factory int_factory;
    int_factory.register_msi_bindings = [queue, t](interrupt_manager &msi) {
        msi.easy_register( {{ 0, [=] { queue->disable_interrupts(); }, t }});
    };

    int_factory.create_pci_interrupt = [this,t](pci::device &pci_dev) {
        return new pci_interrupt(
            pci_dev,
            [=] { return this->ack_irq(); },
            [=] { t->wake(); });
    };

#ifndef AARCH64_PORT_STUB
    int_factory.create_gsi_edge_interrupt = [this,t]() {
        return new gsi_edge_interrupt(
                _dev.get_irq(),
                [=] { if (this->ack_irq()) t->wake(); });
    };
#endif

    _dev.register_interrupt(int_factory);

    // Enable indirect descriptor
    queue->set_use_indirect(true);

    // Step 8
    add_dev_status(VIRTIO_CONFIG_S_DRIVER_OK);

    /*
    struct blk_priv* prv;
    struct device *dev;
    std::string dev_name("vblk");
    dev_name += std::to_string(_disk_idx++);

    dev = device_create(&blk_driver, dev_name.c_str(), D_BLK);
    prv = reinterpret_cast<struct blk_priv*>(dev->private_data);
    prv->drv = this;
    dev->size = prv->drv->size();
    read_partition_table(dev);*/

    //debugf("virtio-fs: Add blk device instances %d as %s, devsize=%lld\n", _id, dev_name.c_str(), dev->size);
}

fs::~fs()
{
    //TODO: In theory maintain the list of free instances and gc it
    // including the thread objects and their stack
}

#define READ_CONFIGURATION_FIELD(config,field_name,field) \
    virtio_conf_read(offsetof(config,field_name), &field, sizeof(field));

void fs::read_config()
{
    /*
    READ_CONFIGURATION_FIELD(blk_config,capacity,_config.capacity)
    trace_virtio_blk_read_config_capacity(_config.capacity);

    if (get_guest_feature_bit(VIRTIO_BLK_F_SIZE_MAX)) {
        READ_CONFIGURATION_FIELD(blk_config,size_max,_config.size_max)
        trace_virtio_blk_read_config_size_max(_config.size_max);
    }
    if (get_guest_feature_bit(VIRTIO_BLK_F_RO)) {
        set_readonly();
        trace_virtio_blk_read_config_ro();
    }*/
}

void fs::req_done()
{
    auto* queue = get_virt_queue(0);
    fs_req* req;

    while (1) {
        virtio_driver::wait_for_queue(queue, &vring::used_ring_not_empty);
        //trace_virtio_blk_wake();

        u32 len;
        while((req = static_cast<fs_req*>(queue->get_buf_elem(&len))) != nullptr) {
            fuse_req_done(req->fuse_req);
            delete req;
            queue->get_buf_finalize();
        }

        // wake up the requesting thread in case the ring was full before
        queue->wakeup_waiter();
    }
}

int fs::make_request(struct fuse_request* req)
{
    // The lock is here for parallel requests protection
    WITH_LOCK(_lock) {

        if (!req) return EIO;

        auto* queue = get_virt_queue(0);

        // LOOK at fs/fuse/virtio_fs.c:virtio_fs_enqueue_req()
        queue->init_sg();
        // INPUT
        queue->add_out_sg(&req->in_header, sizeof(struct fuse_in_header));
        //
        // Add fuse in arguments as out sg
        size_t input_args_size = 0;
        for (int i = 0; i < req->num_of_input_args; i++) {
            input_args_size += req->input_args[i].size;
        }
        queue->add_out_sg(req->input_args, input_args_size);

        // OUTPUT
        queue->add_in_sg(&req->out_header, sizeof(struct fuse_out_header));
        //
        // Add fuse out arguments as in sg
        size_t output_args_size = 0;
        for (int i = 0; i < req->num_of_output_args; i++) {
            output_args_size += req->output_args[i].size;
        }
        queue->add_in_sg(req->output_args, output_args_size);

        auto* fs_request = new fs_req(req);
        queue->add_buf_wait(fs_request);
        queue->kick();

        return 0;
    }
}

u32 fs::get_driver_features()
{
    return virtio_driver::get_driver_features();
    /*
    auto base = virtio_driver::get_driver_features();
    return (base | ( 1 << VIRTIO_BLK_F_SIZE_MAX)
                 | ( 1 << VIRTIO_BLK_F_SEG_MAX)
                 | ( 1 << VIRTIO_BLK_F_GEOMETRY)
                 | ( 1 << VIRTIO_BLK_F_RO)
                 | ( 1 << VIRTIO_BLK_F_BLK_SIZE)
                 | ( 1 << VIRTIO_BLK_F_CONFIG_WCE)
                 | ( 1 << VIRTIO_BLK_F_WCE));*/
}

hw_driver* fs::probe(hw_device* dev)
{
    return virtio::probe<fs, VIRTIO_ID_FS>(dev);
}

}
