/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * Copyright (C) 2014 Huawei Technologies Duesseldorf GmbH
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef ARCH_SWITCH_HH_
#define ARCH_SWITCH_HH_

#include <osv/barrier.hh>
#include <string.h>
#include "arch-setup.hh"

extern "C" {
void thread_main_c(sched::thread* t);
}

namespace sched {

void thread::switch_to()
{
    thread* old = current();
    asm volatile ("msr tpidr_el0, %0; isb; " :: "r"(_tcb) : "memory");

    asm volatile("\n"
                 "str x29,     %0  \n"
                 "mov x2, sp       \n"
                 "adr x1, 1f       \n" /* address of label */
                 "stp x2, x1,  %1  \n"

                 "ldp x29, x0, %2  \n"
                 "ldp x2, x1,  %3  \n"

                 "mov sp, x2       \n"
                 "blr x1           \n"

                 "1:               \n" /* label */
                 :
                 : "Q"(old->_state.fp), "Ump"(old->_state.sp),
                   "Ump"(this->_state.fp), "Ump"(this->_state.sp)
                 : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8",
                   "x9", "x10", "x11", "x12", "x13", "x14", "x15",
                   "x16", "x17", "x18", "x30", "memory");
}

void thread::switch_to_first()
{
    asm volatile ("msr tpidr_el0, %0; isb; " :: "r"(_tcb) : "memory");

    /* check that the tls variable preempt_counter is correct */
    assert(sched::get_preempt_counter() == 1);

    s_current = this;
    current_cpu = _detached_state->_cpu;
    remote_thread_local_var(percpu_base) = _detached_state->_cpu->percpu_base;

    asm volatile("\n"
                 "ldp x29, x0, %0  \n"
                 "ldp x2, x1, %1   \n"
                 "mov sp, x2       \n"
                 "blr x1           \n"
                 :
                 : "Ump"(this->_state.fp), "Ump"(this->_state.sp)
                 : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8",
                   "x9", "x10", "x11", "x12", "x13", "x14", "x15",
                   "x16", "x17", "x18", "x30", "memory");
}

void thread::init_stack()
{
    auto& stack = _attr._stack;
    if (!stack.size) {
        stack.size = 65536;
    }
    if (!stack.begin) {
        stack.begin = malloc(stack.size);
        stack.deleter = stack.default_deleter;
    }
    void** stacktop = reinterpret_cast<void**>(stack.begin + stack.size);
    _state.fp = 0;
    _state.thread = this;
    _state.sp = stacktop;
    _state.pc = reinterpret_cast<void*>(thread_main_c);
}

void thread::setup_tcb()
{   //
    // Most importantly this method allocates TLS memory region and
    // sets up TCB (Thread Control Block) that points to that allocated
    // memory region. The TLS memory region is designated to a specific thread
    // and holds thread local variables (with __thread modifier) defined
    // in OSv kernel and the application ELF objects including dependant ones
    // through DT_NEEDED tag.
    //
    // Each ELF object and OSv kernel gets its own TLS block with offsets
    // specified in DTV structure (the offsets get calculated as ELF is loaded and symbols
    // resolved before we get to this point).
    //
    // Because both OSv kernel and position-in-dependant (pie) or position-dependant
    // executable (non library) are compiled to use local-exec mode to access the thread
    // local variables, we need to setup the offsets and TLS blocks in a special way
    // to avoid any collisions. Specifically we define OSv TLS segment
    // (see arch/aarch64/loader.ld for specifics) with an extra buffer at
    // the beginning of the kernel TLS to accommodate TLS block of pies and
    // position-dependant executables.
    //
    // Please note that the TLS layout conforms to the variant I (1),
    // which means for example that all variable offsets are positive.
    // It also means that individual objects are laid out from the left to the right.

    // (1) - TLS memory area layout with app shared library
    // |------|--------------|-----|-----|-----|
    // |<NONE>|KERNEL        |SO_1 |SO_2 |SO_3 |
    // |------|--------------|-----|-----|-----|

    // (2) - TLS memory area layout with pie or
    // position dependant executable
    // |------|--------------|-----|-----|
    // | EXE  |KERNEL        |SO_2 |SO_3 |
    // |------|--------------|-----|-----|

    assert(tls.size);

    /*
    debug_early_u64("---> setup_tcb:        p: ", (ulong)p);
    debug_early_u64("---> setup_tcb:     _tcb: ", (ulong)(void*)_tcb);
    debug_early_u64("---> setup_tcb: &(_tcb[0].tls_base): ", (ulong)(void*)(&(_tcb[0].tls_base)));
    debug_early_u64("---> setup_tcb: &_tcb[1]: ", (ulong)(void*)(&_tcb[1]));
    */

    void* user_tls_data;
    size_t user_tls_size = 0;
    size_t executable_tls_size = 0;
    if (_app_runtime) {
        auto obj = _app_runtime->app.lib();
        assert(obj);
        user_tls_size = obj->initial_tls_size();
        user_tls_data = obj->initial_tls();
        if (obj->is_executable()) {
           executable_tls_size = obj->get_tls_size();
        }
    }

    // In arch/aarch64/loader.ld, the TLS template segment is aligned to 64
    // bytes, and that's what the objects placed in it assume. So make
    // sure our copy is allocated with the same 64-byte alignment, and
    // verify that object::init_static_tls() ensured that user_tls_size
    // also doesn't break this alignment.
    auto kernel_tls_size = sched::tls.size;
    assert(align_check(kernel_tls_size, (size_t)64));
    assert(align_check(user_tls_size, (size_t)64));

    auto total_tls_size = kernel_tls_size + user_tls_size;
    void* p = aligned_alloc(64, total_tls_size + sizeof(*_tcb));
    _tcb = (thread_control_block *)p;
    _tcb[0].tls_base = &_tcb[1];
    //
    // First goes kernel TLS data
    auto kernel_tls = _tcb[0].tls_base;
    memcpy(kernel_tls, sched::tls.start, sched::tls.filesize);
    memset(kernel_tls + sched::tls.filesize, 0,
           kernel_tls_size - sched::tls.filesize);
    //
    // Next goes user TLS data
    if (user_tls_size) {
        memcpy(kernel_tls + kernel_tls_size, user_tls_data, user_tls_size);
    }

    if (executable_tls_size) {
        // If executable, then copy its TLS block data at the designated offset
        // at the beginning of the area as described in the ascii art for executables
        // TLS layout
        _app_runtime->app.lib()->copy_local_tls(kernel_tls);
    }
}

void thread::free_tcb()
{
    free(_tcb);
}

void thread_main_c(thread* t)
{
    arch::irq_enable();

#ifdef CONF_preempt
    preempt_enable();
#endif

    t->main();
    t->complete();
}

}

#endif /* ARCH_SWITCH_HH_ */
