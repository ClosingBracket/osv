/*
 * Copyright (C) 2019 Waldemar Kozaczuk.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef VIRTIO_PCI_DEVICE_HH
#define VIRTIO_PCI_DEVICE_HH

#include <osv/pci.hh>
#include <osv/interrupt.hh>
#include "drivers/virtio-device.hh"

namespace virtio {
    class virtio_pci_device : public virtio_device {
    public:
    private:
        pci::device &_dev;
        interrupt_manager _msi;
    };

    class virtio_legacy_pci_device : public virtio_pci_device {
    };

    class virtio_modern_pci_device : public virtio_pci_device {
    };
}

#endif //VIRTIO_PCI_DEVICE_HH
