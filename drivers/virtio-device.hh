/*
 * Copyright (C) 2019 Waldemar Kozaczuk.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef VIRTIO_DEVICE_HH
#define VIRTIO_DEVICE_HH

#include <osv/types.h>
#include "virtio-vring.hh"
#include "device.hh"

using namespace hw;

namespace virtio {

class virtio_device : public hw_device {
public:
    virtual ~virtio_device() {};

    virtual void init() = 0;

    virtual u8 ack_irq() = 0;

    virtual vring *probe_queue(int queue) = 0;
    virtual void select_queue(int queue) = 0;
    virtual u16 get_queue_size() = 0;
    virtual void activate_queue(vring *queue) = 0;
    virtual void kick_queue(int queue) = 0;

    virtual u64 get_available_features() = 0;
    virtual bool get_available_feature_bit(int bit) = 0;

    virtual void set_enabled_features(u64 features) = 0;
    virtual void set_enabled_feature_bit(int bit, bool on) = 0;
    virtual u64 get_enabled_features() = 0;
    virtual bool get_enabled_feature_bit(int bit) = 0;

    virtual u8 get_status() = 0;
    virtual void set_status(u8 status) = 0;

    virtual u8 read_config(u32 offset) = 0;
};

}

#endif //VIRTIO_DEVICE_HH
