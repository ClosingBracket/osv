/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * Copyright (C) 2014 Huawei Technologies Duesseldorf GmbH
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <osv/elf.hh>
#include <osv/sched.hh>

namespace elf {

bool arch_init_reloc_dyn(struct init_table *t, u32 type, u32 sym,
                         void *addr, void *base, Elf64_Sxword addend)
{
    switch (type) {
    case R_X86_64_NONE:
        break;
    case R_X86_64_COPY: {
        const Elf64_Sym *st = t->dyn_tabs.lookup(sym);
        memcpy(addr, (void *)st->st_value, st->st_size);
        break;
    }
    case R_X86_64_64:
        *static_cast<u64*>(addr) = t->dyn_tabs.lookup(sym)->st_value + addend;
        break;
    case R_X86_64_RELATIVE:
        *static_cast<void**>(addr) = base + addend;
        break;
    case R_X86_64_JUMP_SLOT:
    case R_X86_64_GLOB_DAT:
        *static_cast<u64*>(addr) = t->dyn_tabs.lookup(sym)->st_value;
        break;
    case R_X86_64_DPTMOD64:
        abort();
        //*static_cast<u64*>(addr) = symbol_module(sym);
        break;
    case R_X86_64_DTPOFF64:
        *static_cast<u64*>(addr) = t->dyn_tabs.lookup(sym)->st_value;
        break;
    case R_X86_64_TPOFF64:
        // FIXME: assumes TLS segment comes before DYNAMIC segment
        *static_cast<u64*>(addr) = t->dyn_tabs.lookup(sym)->st_value - t->tls.size;
        break;
    case R_X86_64_IRELATIVE:
        *static_cast<void**>(addr) = reinterpret_cast<void *(*)()>(base + addend)();
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
    case R_X86_64_NONE:
        break;
    case R_X86_64_COPY: {
        symbol_module sm = symbol_other(sym);
        memcpy(addr, sm.relocated_addr(), sm.size());
        break;
    }
    case R_X86_64_64:
        *static_cast<void**>(addr) = symbol(sym).relocated_addr() + addend;
        break;
    case R_X86_64_RELATIVE:
        *static_cast<void**>(addr) = _base + addend;
        break;
    case R_X86_64_JUMP_SLOT:
    case R_X86_64_GLOB_DAT:
        *static_cast<void**>(addr) = symbol(sym).relocated_addr();
        break;
    case R_X86_64_DPTMOD64:
        // This and amd R_X86_64_DTPOFF64 is used for tls_get_addr() completely DYNAMIC access
        // This calculates the module index of the ELF contain the variable
        if (sym == STN_UNDEF) {
            // This is for accessing thread-local variable within the SAME shared object
            *static_cast<u64*>(addr) = _module_index; // Return this index
            // No need to calculate variable offset
        } else {
            // This is for accessing thread-local variable in DIFFERENT shared object
            // That is why it has to resolve the symbol and ELF object defining it
	        auto s = symbol(sym);
            *static_cast<u64*>(addr) = s.obj->_module_index;
	        //auto s_name = s.obj->symbol_name(s.symbol);
	        //printf("arch_relocate_rela: R_X86_64_DPTMOD64 %d, %s, module:%d, _module:%d\n",
	        //		    sym, s_name, s.obj->_module_index, _module_index);
        }
        break;
    case R_X86_64_DTPOFF64:
        {
	       // This is ONLY used for accessing thread-local variable in DIFFERENT shared object
	       // because we do not know offset ahead of time (compiler did not produce it)
	        auto s = symbol(sym);
            *static_cast<u64*>(addr) = s.symbol->st_value;
	        //auto s_name = s.obj->symbol_name(s.symbol);
	        //printf("arch_relocate_rela: R_X86_64_DTPOFF64 %d, %s\n", sym, s_name);
        }
        break;
    case R_X86_64_TPOFF64:
	//This seems to resolve relications for TLS initial-exec access
	// and is supposed to calculate full virtual address of thread-local variable
        if (sym) {
            auto sm = symbol(sym);
	    ulong tls_offset;
            if (sm.obj->is_executable()) {
               tls_offset = sm.obj->get_tls_size();
            }
	    else {
               sm.obj->alloc_static_tls();
               tls_offset = sm.obj->static_tls_end() + sched::kernel_tls_size();
            }
	    auto s_name = sm.obj->symbol_name(sm.symbol);

	    printf("arch_relocate_rela: R_X86_64_TPOFF64, sym:%d, name:%s, this module:%d, symbol module:%d, tls_offset:%d, addend:%d, st_value:%d\n", 
               sym, s_name, _module_index, sm.obj->module_index(), tls_offset, addend, sm.symbol->st_value);
            *static_cast<u64*>(addr) = sm.symbol->st_value + addend - tls_offset;
        } else {
            // When does this get used?
            alloc_static_tls();
            auto tls_offset = static_tls_end() + sched::kernel_tls_size();
	    printf("arch_relocate_rela: R_X86_64_TPOFF64, NO sym, tls offset: %d\n", tls_offset);
            *static_cast<u64*>(addr) = addend - tls_offset;
        }
        break;
    default:
        return false;
    }

    return true;
}

bool object::arch_relocate_jump_slot(u32 sym, void *addr, Elf64_Sxword addend, bool ignore_missing)
{
    auto _sym = symbol(sym, ignore_missing);
    if (_sym.symbol) {
        *static_cast<void**>(addr) = _sym.relocated_addr();
         return true;
    } else {
         return false;
    }
}

void object::prepare_initial_tls(void* buffer, size_t size,
                                 std::vector<ptrdiff_t>& offsets)
{
    if (!_static_tls) {
        return;
    }
    auto tls_size = get_tls_size();
    auto ptr = static_cast<char*>(buffer) + size - _static_tls_offset - tls_size;
    memcpy(ptr, _tls_segment, _tls_init_size);
    memset(ptr + _tls_init_size, 0, _tls_uninit_size);

    offsets.resize(std::max(_module_index + 1, offsets.size()));
    auto offset = - _static_tls_offset - tls_size - sched::kernel_tls_size();
    offsets[_module_index] = offset;
    printf("---> prepare_initial_tls: _module_index: %d, offset: %d\n", _module_index, offset);   
}

void object::prepare_local_tls(std::vector<ptrdiff_t>& offsets)
{
    printf("---> prepare_local_tls: _module_index: %d called !!", _module_index);
    if (!_static_tls && !is_executable()) {
        return;
    }

    offsets.resize(std::max(_module_index + 1, offsets.size()));
    auto offset = - get_tls_size();
    offsets[_module_index] = offset;
    printf("---> prepare_local_tls: _module_index: %d, offset: %d\n", _module_index, offset);   
}

}
