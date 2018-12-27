//
// Created by wkozaczuk on 12/25/18.
//

#ifndef VIRTIO_MMIO_DEVICE_HH
#define VIRTIO_MMIO_DEVICE_HH

#include <osv/types.h>
#include <osv/mmio.hh>
#include "device.hh"

using namespace hw;

/* Magic value ("virt" string) - Read Only */
#define VIRTIO_MMIO_MAGIC_VALUE		0x000

/* Virtio device version - Read Only */
#define VIRTIO_MMIO_VERSION		0x004

/* Virtio device ID - Read Only */
#define VIRTIO_MMIO_DEVICE_ID		0x008

/* Virtio vendor ID - Read Only */
#define VIRTIO_MMIO_VENDOR_ID		0x00c

/* Bitmask of the features supported by the device (host)
 * (32 bits per set) - Read Only */
#define VIRTIO_MMIO_DEVICE_FEATURES	0x010

/* Device (host) features set selector - Write Only */
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL	0x014

/* Bitmask of features activated by the driver (guest)
 * (32 bits per set) - Write Only */
#define VIRTIO_MMIO_DRIVER_FEATURES	0x020

/* Activated features set selector - Write Only */
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL	0x024

namespace virtio {

class mmio_device : public hw_device {
public:
    mmio_device(u64 address, u64 size, unsigned int irq) :
        _address(address), _size(size), _irq(irq),
        _vendor_id(0), _device_id(0), _addr_mmio(0) {}

    virtual ~mmio_device() {}

    virtual hw_device_id get_id();
    virtual void print();
    virtual void reset();

    bool parse_config();

private:
    u64 _address;
    u64 _size;
    unsigned int _irq;
    //u64 _id;
    u16 _vendor_id;
    u16 _device_id;

    mmioaddr_t _addr_mmio;
};

}

#endif //VIRTIO_MMIO_DEVICE_HH
