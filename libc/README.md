Version 1.1.24

577 files from musl as is.
``` grep -P '^musl \+=' Makefile```

Following modules from musl/src:

* crypt (all from musl)
* ctype (all from musl)
* dirent (all from musl)
* env
* errno
* fenv (x86 from musl)
* locale
* math (all, but `finitel.c` from musl)
* misc
* multibyte (all from musl)
* network
* prng
* process
* regex (all from musl)
* setjmp/x86_64 (all from musl)
* setjmp/aarch64 (all from musl)
* signal (most from musl + `libc/signal.cc`)
* stdio
* stdlib
* string
* temp (all from musl)
* termios (all from musl)
* time (most from musl)
* unistd 
