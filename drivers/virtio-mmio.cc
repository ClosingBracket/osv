//
// Created by wkozaczuk on 12/25/18.
//

#include <osv/debug.hh>
#include "virtio-mmio.hh"

namespace virtio {

hw_device_id mmio_device::get_id()
{
    return hw_device_id(_vendor_id, _device_id);
}

void mmio_device::print() {}
void mmio_device::reset() {}

u8 mmio_device::get_status() {
    return mmio_getl(_addr_mmio + VIRTIO_MMIO_STATUS) & 0xff;
}

void mmio_device::set_status(u8 status) {
    mmio_setl(_addr_mmio + VIRTIO_MMIO_STATUS, status);
}

void mmio_device::add_status(u8 status) {
    mmio_setl(_addr_mmio + VIRTIO_MMIO_STATUS, status | get_status());
}

u64 mmio_device::get_features() {
    u64 features;

    mmio_setl(_addr_mmio + VIRTIO_MMIO_DEVICE_FEATURES_SEL, 1);
    features = mmio_getl(_addr_mmio + VIRTIO_MMIO_DEVICE_FEATURES);
    features <<= 32;

    mmio_setl(_addr_mmio + VIRTIO_MMIO_DEVICE_FEATURES_SEL, 0);
    features |= mmio_getl(_addr_mmio + VIRTIO_MMIO_DEVICE_FEATURES);

    return features;
}

void mmio_device::set_features(u64 features) {
    mmio_setl(_addr_mmio + VIRTIO_MMIO_DRIVER_FEATURES_SEL, 1);
    mmio_setl(_addr_mmio + VIRTIO_MMIO_DRIVER_FEATURES, (u32)(features >> 32));

    mmio_setl(_addr_mmio + VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
    mmio_setl(_addr_mmio + VIRTIO_MMIO_DRIVER_FEATURES, (u32)features);
}

void mmio_device::kick(int queue_num) {
    mmio_setl(_addr_mmio + VIRTIO_MMIO_QUEUE_NOTIFY, queue_num);
    //debugf("mmio_device:kick() -> kicked queue %d\n", queue_num);
}

void mmio_device::select_queue(int queue_num) {
    mmio_setl(_addr_mmio + VIRTIO_MMIO_QUEUE_SEL, queue_num);
    assert(!mmio_getl(_addr_mmio + VIRTIO_MMIO_QUEUE_READY));
}

u16 mmio_device::get_queue_size() {
    return mmio_getl(_addr_mmio + VIRTIO_MMIO_QUEUE_NUM_MAX) & 0xffff;
}

void mmio_device::activate_queue(vring* queue) {
    // Set size
    mmio_setl(_addr_mmio + VIRTIO_MMIO_QUEUE_NUM, queue->size());
    //
    // Pass addresses
    mmio_setl(_addr_mmio + VIRTIO_MMIO_QUEUE_DESC_LOW, (u32)queue->get_desc_addr());
    mmio_setl(_addr_mmio + VIRTIO_MMIO_QUEUE_DESC_HIGH, (u32)(queue->get_desc_addr() >> 32));

    mmio_setl(_addr_mmio + VIRTIO_MMIO_QUEUE_AVAIL_LOW, (u32)queue->get_avail_addr());
    mmio_setl(_addr_mmio + VIRTIO_MMIO_QUEUE_AVAIL_HIGH, (u32)(queue->get_avail_addr() >> 32));

    mmio_setl(_addr_mmio + VIRTIO_MMIO_QUEUE_USED_LOW, (u32)queue->get_used_addr());
    mmio_setl(_addr_mmio + VIRTIO_MMIO_QUEUE_USED_HIGH, (u32)(queue->get_used_addr() >> 32));
    //
    // Make it ready
    mmio_setl(_addr_mmio + VIRTIO_MMIO_QUEUE_READY, 1 );
}

bool mmio_device::ack_irq() {
    unsigned long status = mmio_getl(_addr_mmio + VIRTIO_MMIO_INTERRUPT_STATUS);
    //assert(status & VIRTIO_MMIO_INT_VRING);
    mmio_setl(_addr_mmio + VIRTIO_MMIO_INTERRUPT_ACK, status);
    //return true;
    return (status & VIRTIO_MMIO_INT_VRING) != 0;
}

u8 mmio_device::read_config(u64 offset) {
    return mmio_getb(_addr_mmio + VIRTIO_MMIO_CONFIG + offset);
}

bool mmio_device::parse_config() {
    _addr_mmio = mmio_map(_address, _size);

    u32 magic = mmio_getl(_addr_mmio + VIRTIO_MMIO_MAGIC_VALUE);
    if (magic != ('v' | 'i' << 8 | 'r' << 16 | 't' << 24)) {
        return false;
    }

    // Check device version
    u32 version = mmio_getl(_addr_mmio + VIRTIO_MMIO_VERSION);
    if (version < 1 || version > 2) {
        debugf( "Version %ld not supported!\n", version);
        return false;
    }

    _device_id = mmio_getl(_addr_mmio + VIRTIO_MMIO_DEVICE_ID);
    if (_device_id == 0) {
        //
        // virtio-mmio device with an ID 0 is a (dummy) placeholder
        // with no function. End probing now with no error reported.
        debug( "Dummy virtio-mmio device detected!\n");
        return false;
    }
    _vendor_id = mmio_getl(_addr_mmio + VIRTIO_MMIO_VENDOR_ID);

    debugf("Detected virtio-mmio device: (%ld,%ld)\n", _device_id, _vendor_id);
    return true;
}
}
