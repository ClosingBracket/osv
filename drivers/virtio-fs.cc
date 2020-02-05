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

using namespace memory;

void fuse_req_wait(struct fuse_request* req)
{
    WITH_LOCK(req->req_mutex) {
        req->req_wait.wait(req->req_mutex);
    }
}

namespace virtio {

static int fuse_make_request(void *driver, struct fuse_request* req)
{
    auto fs_driver = reinterpret_cast<fs*>(driver);
    return fs_driver->make_request(req);
}

static void fuse_req_done(struct fuse_request* req)
{
    WITH_LOCK(req->req_mutex) {
        req->req_wait.wake_one(req->req_mutex);
    }
}

static void fuse_req_enqueue_input(vring* queue, struct fuse_request* req)
{
    // Header goes first
    queue->add_out_sg(&req->in_header, sizeof(struct fuse_in_header));
    //
    // Add fuse in arguments as out sg
    size_t input_args_size = 0;
    for (int i = 0; i < req->num_of_input_args; i++) {
            input_args_size += req->input_args[i].size;
    }
    queue->add_out_sg(req->input_args, input_args_size);
}

static void fuse_req_enqueue_output(vring* queue, struct fuse_request* req)
{
    // Header goes first
    queue->add_in_sg(&req->out_header, sizeof(struct fuse_out_header));
    //
    // Add fuse out arguments as in sg
    size_t output_args_size = 0;
    for (int i = 0; i < req->num_of_output_args; i++) {
        output_args_size += req->output_args[i].size;
    }
    queue->add_in_sg(req->output_args, output_args_size);
}

int fs::_instance = 0;

static struct devops fs_devops {
    no_open, //TODO: This possibly in future could point to a function that does FUSE_INIT
    no_close,
    no_read,
    no_write,
    no_ioctl,
    no_devctl,
    no_strategy,
};

struct driver fs_driver = {
    "virtio_fs",
    &fs_devops,
    sizeof(struct fuse_strategy),
};

bool fs::ack_irq()
{
    auto isr = _dev.read_and_ack_isr();
    auto queue = get_virt_queue(VQ_REQUEST);

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

    if (_config.num_queues < 1) {
        printf("--> Expected at least one request queue -> baling out!\n");
        return;
    }

    // Step 7 - generic init of virtqueues
    probe_virt_queues();

    //register the single irq callback for the block
    sched::thread* t = sched::thread::make([this] { this->req_done(); },
            sched::thread::attr().name("virtio-fs"));
    t->start();
    auto queue = get_virt_queue(VQ_REQUEST);

    interrupt_factory int_factory;
    int_factory.register_msi_bindings = [queue, t](interrupt_manager &msi) {
        msi.easy_register( {{ VQ_REQUEST, [=] { queue->disable_interrupts(); }, t }});
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

    std::string dev_name("virtiofs");
    dev_name += std::to_string(_disk_idx++);

    struct device *dev = device_create(&fs_driver, dev_name.c_str(), D_BLK); //TODO Is it really 
    struct fuse_strategy *strategy = reinterpret_cast<struct fuse_strategy*>(dev->private_data);
    strategy->drv = this;
    strategy->make_request = fuse_make_request;
    //dev->size = prv->drv->size(); --> TODO: Add this somewhere in the mount routine
    //read_partition_table(dev);

    //debugf("virtio-fs: Add blk device instances %d as %s, devsize=%lld\n", _id, dev_name.c_str(), dev->size);
}

fs::~fs()
{
    //TODO: In theory maintain the list of free instances and gc it
    // including the thread objects and their stack
}

void fs::read_config()
{
    virtio_conf_read(0, &(_config.tag[0]), sizeof(_config.tag));
    printf("-> virtio-fs: tag [%s]\n", _config.tag);
    virtio_conf_read(offsetof(fs_config,num_queues), &(_config.num_queues), sizeof(_config.num_queues));
    printf("-> virtio-fs: num_queues [%d]\n", _config.num_queues);
}

void fs::req_done()
{
    auto* queue = get_virt_queue(VQ_REQUEST);
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

        auto* queue = get_virt_queue(VQ_REQUEST);

        // LOOK at fs/fuse/virtio_fs.c:virtio_fs_enqueue_req()
        queue->init_sg();

        fuse_req_enqueue_input(queue, req);
        fuse_req_enqueue_output(queue, req);

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
