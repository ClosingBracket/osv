/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef VIRTIO_DRIVER2_H
#define VIRTIO_DRIVER2_H

#include "driver.hh"
#include "drivers/driver.hh"
#include "drivers/virtio.hh"
#include "drivers/virtio-mmio.hh"
#include "drivers/virtio-vring.hh"
#include <osv/interrupt.hh>

namespace virtio {

class virtio_mmio_driver : public hw_driver, public vdriver {
public:
    explicit virtio_mmio_driver(mmio_device& dev);
    virtual ~virtio_mmio_driver();

    virtual std::string get_name() const = 0;

    virtual void dump_config();

    // The remaining space is defined by each driver as the per-driver
    // configuration space
    //int virtio_pci_config_offset() {return (_dev.is_msix_enabled())? 24 : 20;}

    //bool parse_pci_config();

    void probe_virt_queues();
    vring* get_virt_queue(unsigned idx);

    // block the calling thread until the queue has some used elements in it.
    void wait_for_queue(vring* queue, bool (vring::*pred)() const);

    // guest/host features physical access
    // LEGACY NOTE:
    //      guest equivalent to driver
    //      host equivalent to device
    u64 get_device_features();
    bool get_device_feature_bit(int bit);

    void set_drv_features(u64 features);
    u32 get_drv_features();
    bool get_drv_feature_bit(int bit);

    // device status
    u8 get_dev_status();
    void set_dev_status(u8 status);
    void add_dev_status(u8 status);
    void del_dev_status(u8 status);

    // Access the virtio conf address space set by pci bar 1
    //bool get_virtio_config_bit(u32 offset, int bit);
    //void set_virtio_config_bit(u32 offset, int bit, bool on);

    // Access virtio config space
    //void virtio_conf_read(u32 offset, void* buf, int length);
    //void virtio_conf_write(u32 offset, void* buf, int length);
    //u8 virtio_conf_readb(u32 offset) { return _bar1->readb(offset);};
    //u16 virtio_conf_readw(u32 offset) { return _bar1->readw(offset);};
    //u32 virtio_conf_readl(u32 offset) { return _bar1->readl(offset);};
    //void virtio_conf_writeb(u32 offset, u8 val) { _bar1->writeb(offset, val);};
    //void virtio_conf_writew(u32 offset, u16 val) { _bar1->writew(offset, val);};
    //void virtio_conf_writel(u32 offset, u32 val) { _bar1->writel(offset, val);};

    bool kick(int queue);
    void reset_host_side();
    void free_queues();

    bool get_indirect_buf_cap() {return _cap_indirect_buf;}
    void set_indirect_buf_cap(bool on) {_cap_indirect_buf = on;}
    bool get_event_idx_cap() {return _cap_event_idx;}
    void set_event_idx_cap(bool on) {_cap_event_idx = on;}

    mmio_device& device() { return _dev; }
protected:
    // Actual drivers should implement this on top of the basic ring features
    virtual u32 get_driver_features() { return 1 << VIRTIO_RING_F_INDIRECT_DESC | 1 << VIRTIO_RING_F_EVENT_IDX; }
    void setup_features();
protected:
    mmio_device& _dev;
    //pci::device& _dev;
    //interrupt_manager _msi;
    vring* _queues[max_virtqueues_nr];
    u32 _num_queues;
    u64 _enabled_features;
    //pci::bar* _bar1;
    bool _cap_indirect_buf;
    bool _cap_event_idx = false;
    static int _disk_idx;
};

template <typename T, u16 ID>
hw_driver* probe_mmio(hw_device* dev)
{
    if (auto mmio_dev = dynamic_cast<mmio_device*>(dev)) {
        if (mmio_dev->get_id() == hw_device_id(0x0, ID)) {
            return new T(*mmio_dev);
        }
    }
    return nullptr;
}

}

#endif

