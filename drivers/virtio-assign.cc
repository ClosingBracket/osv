/*
 * Copyright (C) 2014 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */


#include <osv/virtio-assign.hh>
#include <drivers/virtio-device.hh>
#include <drivers/virtio-net.hh>


// Currently we support only one assigned virtio device, but more could
// easily be added later.
static osv::assigned_virtio *the_assigned_virtio_device = nullptr;
namespace osv {
assigned_virtio *assigned_virtio::get() {
    return the_assigned_virtio_device;
}
}

// "impl" is a subclass of virtio::virtio_driver which also implements the
// API we export applications: osv::assigned_virtio.
class impl : public osv::assigned_virtio, public virtio::virtio_driver {
public:
    static hw_driver* probe_net(hw_device* dev);
    explicit impl(virtio::virtio_device& virtio_dev)
        : virtio_driver(virtio_dev)
    {
        assert(!the_assigned_virtio_device);
        the_assigned_virtio_device = this;
    }

    virtual ~impl() {
    }

    virtual std::string get_name() const override {
        return "virtio-assigned";
    }
    virtual u32 get_driver_features() override {
        return _driver_features;
    }

    // osv::assigned_virtio API implementation:

    virtual void kick(int queue) override {
        virtio_driver::kick(queue);
    }

    virtual u32 queue_size(int queue) override
    {
        _dev.select_queue(queue);
        return _dev.get_queue_size();
    }
    virtual u32 init_features(u32 driver_features) override
    {
        _driver_features = driver_features;
        setup_features();
        return get_guest_features();
    }

    virtual void enable_interrupt(unsigned int queue, std::function<void(void)> handler) override
    {
        // Hack to enable_interrupt's handler in a separate thread instead of
        // directly at the interrupt context. We need this when the tries to
        // signal and eventfd, which involves a mutex and not allowed in interrupt
        // context. In the future we must get rid of this ugliess, and make the
        // handler code lock-free and allowed at interrupt context!
        _hack_threads.emplace_back(handler);
        auto *ht = &_hack_threads.back(); // assumes object won't move later
        handler = [ht] { ht->wake(); };

        _dev.register_interrupt(queue,handler);
    }

    virtual void set_queue_pfn(int queue, u64 phys) override
    {
        _dev.select_queue(queue);
        _dev.activate_queue(phys);
    }

    virtual void set_driver_ok() override
    {
        add_dev_status(virtio::VIRTIO_CONFIG_S_DRIVER_OK);
    }

    virtual void conf_read(void *buf, int length) override
    {
        virtio_conf_read(_dev.config_offset(), buf, length);
    }

private:
    u32 _driver_features;

    // This is an inefficient hack, to run enable_interrupt's handler in a
    // separate thread instead of directly at the interrupt context.
    // We need this when the tries to signal and eventfd, which involves a
    // mutex and not allowed in interrupt context. In the future we must get
    // rid of this ugliess, and make the handler code lock-free and allowed
    // at interrupt context!
    class hack_thread {
    private:
        std::atomic<bool> _stop {false};
        std::atomic<bool> _wake {false};
        std::function<void(void)> _handler;
        std::unique_ptr<sched::thread> _thread;
        void stop() {
            _stop.store(true, std::memory_order_relaxed);
            wake();
            _thread = nullptr; // join and clean up the thread
        }
        bool wait() {
            sched::thread::wait_until([&] { return _wake.load(std::memory_order_relaxed); });
            _wake.store(false, std::memory_order_relaxed);
            return _stop.load(std::memory_order_relaxed);
        }
    public:
        void wake() {
            _wake.store(true, std::memory_order_relaxed);
            _thread->wake();
        }
        explicit hack_thread(std::function<void(void)> handler)
                : _handler(handler) {
            _thread = std::unique_ptr<sched::thread>(sched::thread::make([&] {
                while (!_stop.load(std::memory_order_relaxed)) {
                    wait();
                    _handler();
                }
            }));
            _thread->start();
        }
        ~hack_thread() {
            stop();
        }
    };
    std::list<hack_thread> _hack_threads;
};


namespace osv {
mmu::phys assigned_virtio::virt_to_phys(void* p)
{
    return mmu::virt_to_phys(p);
}
assigned_virtio::~assigned_virtio() {
}
}

namespace virtio {
namespace assigned {
hw_driver* probe_net(hw_device* dev)
{
    return virtio::probe<impl, virtio::net::VIRTIO_NET_DEVICE_ID>(dev);
}
}
}
