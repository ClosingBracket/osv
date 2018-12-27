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

void mmio_device::kick(int queue_num) {
    mmio_setl(_addr_mmio + VIRTIO_MMIO_QUEUE_NOTIFY, queue_num);
}

void mmio_device::select_queue(int queue_num) {
    mmio_setl(_addr_mmio + VIRTIO_MMIO_QUEUE_SEL, queue_num);
}

u16 mmio_device::get_queue_size() {
    return mmio_getl(_addr_mmio + VIRTIO_MMIO_QUEUE_NUM_MAX) & 0xffff;
}

void mmio_device::activate_queue(vring* queue) {
    // Set size
    mmio_setl(_addr_mmio + VIRTIO_MMIO_QUEUE_NUM, queue->size());
    //
    // Pass other parameters
    mmio_setl(_addr_mmio + VIRTIO_MMIO_QUEUE_DESC_LOW, 0 );
    mmio_setl(_addr_mmio + VIRTIO_MMIO_QUEUE_DESC_HIGH, 0 );

    mmio_setl(_addr_mmio + VIRTIO_MMIO_QUEUE_AVAIL_LOW, 0 );
    mmio_setl(_addr_mmio + VIRTIO_MMIO_QUEUE_AVAIL_HIGH, 0 );

    mmio_setl(_addr_mmio + VIRTIO_MMIO_QUEUE_USED_LOW, 0 );
    mmio_setl(_addr_mmio + VIRTIO_MMIO_QUEUE_USED_HIGH, 0 );
    //
    // Make it ready
    mmio_setl(_addr_mmio + VIRTIO_MMIO_QUEUE_READY, 1 );
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
