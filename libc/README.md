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
