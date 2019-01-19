/*
 * Copyright (C) 2019 Waldemar Kozaczuk.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "drivers/virtio-pci-device.hh"

namespace virtio {

virtio_pci_device::virtio_pci_device(pci::device *dev)
    : virtio_device()
    , _dev(dev)
    , _msi(dev)
{
}

virtio_pci_device::~virtio_pci_device() {
    delete _dev;
}

virtio_legacy_pci_device::virtio_legacy_pci_device(pci::device *dev)
    : virtio_pci_device(dev)
{
}

void virtio_legacy_pci_device::kick_queue(int queue)
{
    virtio_conf_writew(VIRTIO_PCI_QUEUE_NOTIFY, queue);
}

void virtio_legacy_pci_device::setup_queue(int queue)
{
    if (_dev->is_msix()) {
        // Setup queue_id:entry_id 1:1 correlation...
        virtio_conf_writew(VIRTIO_MSI_QUEUE_VECTOR, queue);
        if (virtio_conf_readw(VIRTIO_MSI_QUEUE_VECTOR) != queue) {
            virtio_e("Setting MSIx entry for queue %d failed.", queue);
            return;
        }
    }
}

void virtio_legacy_pci_device::activate_queue(vring *queue)
{
    // Tell host about pfn
    // TODO: Yak, this is a bug in the design, on large memory we'll have PFNs > 32 bit
    // Dor to notify Rusty
    virtio_conf_writel(VIRTIO_PCI_QUEUE_PFN, (u32)(queue->get_paddr() >> VIRTIO_PCI_QUEUE_ADDR_SHIFT));
}

void virtio_legacy_pci_device::select_queue(int queue)
{
    virtio_conf_writew(VIRTIO_PCI_QUEUE_SEL, queue);
}

u16 virtio_legacy_pci_device::get_queue_size()
{
    return virtio_conf_readw(VIRTIO_PCI_QUEUE_NUM);
}

bool virtio_legacy_pci_device::get_virtio_config_bit(u32 offset, int bit)
{
    return virtio_conf_readl(offset) & (1 << bit);
}

void virtio_legacy_pci_device::set_virtio_config_bit(u32 offset, int bit, bool on)
{
    u32 val = virtio_conf_readl(offset);
    u32 newval = ( val & ~(1 << bit) ) | ((int)(on)<<bit);
    virtio_conf_writel(offset, newval);
}

u64 virtio_legacy_pci_device::get_available_features()
{
    return virtio_conf_readl(VIRTIO_PCI_HOST_FEATURES);
}

bool virtio_legacy_pci_device::get_available_feature_bit(int bit)
{
    return get_virtio_config_bit(VIRTIO_PCI_HOST_FEATURES, bit);
}

void virtio_legacy_pci_device::set_enabled_features(u64 features)
{
    virtio_conf_writel(VIRTIO_PCI_GUEST_FEATURES, (u32)features);
}

void virtio_legacy_pci_device::set_enabled_feature_bit(int bit, bool on)
{
    set_virtio_config_bit(VIRTIO_PCI_GUEST_FEATURES, bit, on);
}

u64 virtio_legacy_pci_device::get_enabled_features()
{
    return virtio_conf_readl(VIRTIO_PCI_GUEST_FEATURES);
}

bool virtio_legacy_pci_device::get_enabled_feature_bit(int bit)
{
    return get_virtio_config_bit(VIRTIO_PCI_GUEST_FEATURES, bit);
}

u8 virtio_legacy_pci_device::get_status()
{
    return virtio_conf_readb(VIRTIO_PCI_STATUS);
}

void virtio_legacy_pci_device::set_status(u8 status)
{
    virtio_conf_writeb(VIRTIO_PCI_STATUS, status);
}

u8 virtio_legacy_pci_device::read_config(u32 offset)
{
    return _bar1->readb(offset);
}

void virtio_legacy_pci_device::write_config(u32 offset, u8 byte)
{
    _bar1->writeb(offset, byte);
}

void virtio_legacy_pci_device::dump_config()
{
    u8 B, D, F;
    _dev->get_bdf(B, D, F);

    _dev->dump_config();
    virtio_d("%s [%x:%x.%x] vid:id=%x:%x", get_name().c_str(),
             (u16)B, (u16)D, (u16)F,
             _dev.get_vendor_id(),
             _dev.get_device_id());
}

void virtio_legacy_pci_device::init()
{
    bool status = parse_pci_config();
    assert(status);

    _dev->set_bus_master(true);

    _dev->msix_enable();
}

void virtio_legacy_pci_device::register_interrupt(interrupt_factory irq_factory)
{
    if (irq_factory.register_msi_bindings && _dev->is_msix()) {
        irq_factory.register_msi_bindings(_msi);
    } else {
        _irq.reset(irq_factory.create_pci_interrupt(*_dev));
    }
}

u8 virtio_legacy_pci_device::read_and_ack_isr()
{
    return virtio_conf_readb(VIRTIO_PCI_ISR);
}

bool virtio_legacy_pci_device::parse_pci_config()
{
    // Test whether bar1 is present
    _bar1 = _dev->get_bar(1);
    if (_bar1 == nullptr) {
        return false;
    }

    // Check ABI version
    u8 rev = _dev->get_revision_id();
    if (rev != VIRTIO_PCI_ABI_VERSION) {
        virtio_e("Wrong virtio revision=%x", rev);
        return false;
    }

    // Check device ID
    u16 dev_id = _dev->get_device_id();
    if ((dev_id < VIRTIO_PCI_ID_MIN) || (dev_id > VIRTIO_PCI_ID_MAX)) {
        virtio_e("Wrong virtio dev id %x", dev_id);
        return false;
    }

    return true;
}

virtio_device* create_virtio_device(pci::device *dev) {
    //TODO Read PCI device configuration to create instance
    // of virtio_modern_pci_device or virtio_legacy_pci_device
    return new virtio_legacy_pci_device(dev);
}

}
