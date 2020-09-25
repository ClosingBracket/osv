# Intro
The files in this subdirectory and musl C source files referenced in the [main Makefile](/Makefile) constitute the subset of libc implementation in OSv. Most of the network related functions (`bind()`, `listen()`, `accept()`, `socket()`, etc) are actually located under the `bsd/` part of the tree (see  [`/bsd/sys/kern/uipc_syscalls_wrap.cc`](/bsd/sys/kern/uipc_syscalls_wrap.cc) as an example). Please note that our libc implementation aims to be **glibc** (GNU libc) compatible even though much of the implementation comes from musl. For more details please read the [Linux ABI Compatibility](https://github.com/cloudius-systems/osv/wiki/OSv-Linux-ABI-Compatibility) and the [Components of OSv](https://github.com/cloudius-systems/osv/wiki/Components-of-OSv) wikis.

Please note that because OSv is a unikernel, much of its libc functionality has been implemented from scratch (all the C++ files in this directory). In ideal world the source files would either come from musl *as-is* or be implemented natively. But in reality some of the files in this directory originate from musl and have been adapted to make it work with OSv internals for following major reasons:
* `syscall()` invocations in many `stdio`, `stdlib`, `network` functions have been replaced with direct local functions like `SYS_open(filename, flags, ...)` to `open(filename, flags, ...)` (see [syscall_to_function.h](libc/syscall_to_function.h) for details).
* the locking logic in some of the files in musl stdio have been tweaked to use OSv mutexes

# History

# State

Version 1.1.24. Aims to be glibc compatible. But inherently (unless changed in specific places) is musl compatible (for musl-based) Linux like Apline.

Original commit

Previous is 0.9.12

libc modules natively implemented in OSv (no musl)
* thread
* malloc
* mman
* ldso (dynamic linker)
* sched

MUSL headers (most symlinks):
* include/api
* inlcude/api/internal...
Which are not?

All C++ (`*.cc/*.hh`) files under `libc/` are natively implemented in OSv. Also all FORTIFY functions for glibc compatibility (files ending with `_chk.c`) are also implemented natively.

577 files from musl as is.
``` grep -P '^musl \+=' Makefile```

Following modules from musl/src:

* crypt (all from musl)
* ctype (all from musl)
* dirent (all from musl)
* env - ADD readme describing 2 files under `libc/env`
* errno - ADD readme describing `libc/errno/strerror.c`
* fenv (x86 from musl) - DELETE `libc/fenv/aarch64/fenv.s` - all from musl?
* locale - ADD readme describing the files under `libc/locale` - one big MESS
* math (all, but `finitel.c` from musl)
* misc - ADD readme describing the files under `libc/misc` (most (all?) are implemented natively or from other places, `getopt.c`?)
* multibyte (all from musl)
* network (most are implemented natively or come from freebsd) - ADD readme describing files under `libc/network`
* prng - ADD readme describing `libc/prng/random.c`
* process - ADD readme describing 2 files under `libc/process`
* regex (all from musl)
* setjmp/x86_64 (all from musl)
* setjmp/aarch64 (all from musl)
* signal (most from musl + `libc/signal.cc`)
* stdio - ADD readme describing the files under `libc/stdio` 
* stdlib - ADD readme describing 4 files under `libc/stdlib` 
* string - ADD readme describing 4 files under `libc/string` (non `_chk.c`)
* temp (all from musl)
* termios (all from musl)
* time (most from musl)
* unistd - ADD readme describing the files under `libc/unistd` (anything?)

Files that should never change (besides C++) - TODO (list them)

Files in `libc/` subject to musl upgrade.

Symlinks to musl files under `libc/`

Internal musl files under `libc/`

TODO (as far as upgrade goes).

Possibly syslog.c might ever get updated.

# Upgrades
