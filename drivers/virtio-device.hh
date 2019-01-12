/*
 * Copyright (C) 2019 Waldemar Kozaczuk.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef VIRTIO_DEVICE_HH
#define VIRTIO_DEVICE_HH

#include "drivers/virtio-vring.hh"

namespace virtio {
    class virtio_device {
        explicit virtio_device() {}
        virtual ~virtio_device() = 0;

        virtual vring *probe_queue(int number) = 0;
        virtual void activate_queue(vring *queue) = 0;
        virtual void kick_queue(int number) = 0;

        virtual u64 get_features() = 0;
        virtual bool get_feature_bit(int bit) = 0;

        virtual u8 get_status() = 0;
        virtual void set_status(u8 status) = 0;
        void add_status(u8 status);
    };
}

#endif //VIRTIO_DEVICE_HH
