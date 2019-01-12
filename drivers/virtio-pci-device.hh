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
#include <osv/msi.hh>

#include "virtio-device.hh"

namespace virtio {
    class virtio_pci_device : public virtio_device {
    public:
        explicit virtio_pci_device(pci::device *dev);
        ~virtio_pci_device();

        virtual hw_device_id get_id() { return _dev->get_id(); }
        virtual void print() { _dev->print(); }
        virtual void reset() { _dev->reset(); }

        bool is_attached()  { return _dev->is_attached(); }
        void set_attached() { _dev->set_attached(); }

    private:
        pci::device *_dev;
        interrupt_manager _msi;
    };

    class virtio_legacy_pci_device : public virtio_pci_device {
    public:
        explicit virtio_legacy_pci_device(pci::device dev);
        ~virtio_legacy_pci_device();
    };

    class virtio_modern_pci_device : public virtio_pci_device {
    public:
        explicit virtio_modern_pci_device(pci::device dev);
        ~virtio_modern_pci_device();
    };
}

#endif //VIRTIO_PCI_DEVICE_HH
