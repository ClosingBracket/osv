/*
 * Copyright (C) 2014 Huawei Technologies Duesseldorf GmbH
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <osv/elf.hh>
#include <osv/sched.hh>

extern "C" size_t __tlsdesc_static(size_t *);
namespace elf {

bool arch_init_reloc_dyn(struct init_table *t, u32 type, u32 sym,
                         void *addr, void *base, Elf64_Sxword addend)
{
    switch (type) {
    case R_AARCH64_NONE:
    case R_AARCH64_NONE2:
        break;
    case R_AARCH64_GLOB_DAT:
    case R_AARCH64_JUMP_SLOT:
        *static_cast<u64*>(addr) = t->dyn_tabs.lookup(sym)->st_value + addend;
        break;
    case R_AARCH64_TLS_TPREL64:
        *static_cast<u64*>(addr) = t->dyn_tabs.lookup(sym)->st_value + addend;
        break;
    default:
        return false;
    }
    return true;
}

bool object::arch_relocate_rela(u32 type, u32 sym, void *addr,
                                Elf64_Sxword addend)
{
    switch (type) {
    case R_AARCH64_NONE:
    case R_AARCH64_NONE2:
        break;
    case R_AARCH64_ABS64:
        *static_cast<void**>(addr) = symbol(sym).relocated_addr() + addend;
        break;
    case R_AARCH64_COPY:
        abort();
        break;
    case R_AARCH64_GLOB_DAT:
    case R_AARCH64_JUMP_SLOT:
        *static_cast<void**>(addr) = symbol(sym).relocated_addr() + addend;
        break;
    case R_AARCH64_RELATIVE:
        *static_cast<void**>(addr) = _base + addend;
        break;
    case R_AARCH64_TLS_TPREL64:
        if (sym) {
            auto sm = symbol(sym);
            ulong tls_offset;
            if (sm.obj->is_executable()) {
                // TODO: Update comments below
                // If this is an executable (pie or position-dependant one)
                // then the variable is located in the reserved slot of the TLS
                // right where the kernel TLS lives
                // So the offset is negative aligned size of this ELF TLS block
                tls_offset = 0;
            } else {
                // If shared library, the variable is located in one of TLS
                // blocks that are part of the static TLS before kernel part
                // so the offset needs to shift by sum of kernel and size of the user static
                // TLS so far
                sm.obj->alloc_static_tls();
                printf("--> arch_relocate_rela: static_tls_offset: %ld\n", sm.obj->static_tls_offset());
                tls_offset = sm.obj->static_tls_offset() + sched::kernel_tls_size();
            }
            *static_cast<u64*>(addr) = sm.symbol->st_value + addend + tls_offset + sizeof(thread_control_block);
        }
        else {
           ulong tls_offset = _static_tls_offset + sched::kernel_tls_size();
            *static_cast<u64*>(addr) = addend + tls_offset + sizeof(thread_control_block);
        }
        break;
    default:
        return false;
    }

    return true;
}

bool object::arch_relocate_jump_slot(symbol_module& sym, void *addr, Elf64_Sxword addend)
{
    if (sym.symbol) {
        *static_cast<void**>(addr) = sym.relocated_addr() + addend;
        return true;
    } else {
        return false;
    }
}

bool object::arch_relocate_tls_desc(symbol_module& sym, void *addr, Elf64_Sxword addend)
{
    if (sym.symbol) {
        //TODO: Differentiate between DL_NEEDED (static TLS, initial-exec) and dynamic TLS (dlopen)
        sym.obj->alloc_static_tls();
        *static_cast<size_t*>(addr) = (size_t)__tlsdesc_static;
        auto offset = (size_t)sym.symbol->st_value + addend + sched::kernel_tls_size() + sym.obj->static_tls_offset() + sizeof(thread_control_block);
        *(static_cast<size_t*>(addr) + 1) = offset;
        printf("arch_relocate_tls_desc: offset:%ld\n", offset);
        /*
        ulong tls_offset;
        if (sm.obj->is_executable()) {
            // If this is an executable (pie or position-dependant one)
            // then the variable is located in the reserved slot of the TLS
            // right where the kernel TLS lives
            // So the offset is negative aligned size of this ELF TLS block
            tls_offset = sm.obj->get_aligned_tls_size();
        } else {
            // If shared library, the variable is located in one of TLS
            // blocks that are part of the static TLS before kernel part
            // so the offset needs to shift by sum of kernel and size of the user static
            // TLS so far
            sm.obj->alloc_static_tls();
            tls_offset = sm.obj->static_tls_end() + sched::kernel_tls_size();
        }
        *static_cast<u64*>(addr) = sm.symbol->st_value + addend - tls_offset;*/
        return true;
    } else {
        // TODO: Which case does this handle?
        //alloc_static_tls();
        //ls_descauto tls_offset = static_tls_end() + sched::kernel_tls_size();
        //*static_cast<u64*>(addr) = addend - tls_offset;
        //*static_cast<void**>(addr) = sym.relocated_addr() + addend;
        printf("______> BOLO1\n");
        return false;
    }
}

void object::prepare_initial_tls(void* buffer, size_t size,
                                 std::vector<ptrdiff_t>& offsets)
{
    if (!_static_tls) {
        return;
    }
    //auto offset = sched::kernel_tls_size() + _static_tls_offset;
    auto offset = _static_tls_offset;
    auto ptr = static_cast<char*>(buffer) + offset;
    memcpy(ptr, _tls_segment, _tls_init_size);
    memset(ptr + _tls_init_size, 0, _tls_uninit_size);

    offsets.resize(std::max(_module_index + 1, offsets.size()));
    offsets[_module_index] = offset;
    printf("--> prepare_initial_tls: size: %ld\n", size);
    printf("--> prepare_initial_tls: val:  %ld\n", *((int*)ptr));
}

void object::prepare_local_tls(std::vector<ptrdiff_t>& offsets)
{
    if (!_static_tls && !is_executable()) {
        return;
    }

    offsets.resize(std::max(_module_index + 1, offsets.size()));
    offsets[_module_index] = 0;
}

void object::copy_local_tls(void* to_addr)
{
    memcpy(to_addr, _tls_segment, _tls_init_size); //file size - 48 (0x30) for example and 80 (0x50) for httpserver
    memset(to_addr + _tls_init_size, 0, _tls_uninit_size);
}

}
