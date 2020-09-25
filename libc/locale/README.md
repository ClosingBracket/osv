The files in this subdirectory and locale OSv specific code in runtime.cc (see __newlocale function)
provide minimal implementation of locale functionality. Unfortunately musl 1.1.24 had enhanced
its implementation to support categories (for example new 'cat' field in locale struct)
and does not quite fit into what we need. For now we have made minimal changes to existing
files and mostly retained musl 0.9.12 copies to make locale-specific logic work.
We may need to adjust it futher and decide what files constitute OSv version
of it and completely cut it off of any future musl upgrades.

Please see following musl commits that should shed some light:
- https://git.musl-libc.org/cgit/musl/commit/?id=0bc03091bb674ebb9fa6fe69e4aec1da3ac484f2 (add locale framework)
- https://git.musl-libc.org/cgit/musl/commit/?id=61a3364d246e72b903da8b76c2e27a225a51351e (overhaul locale internals to treat categories roughly uniformly)

langinfo.c - copied from musl 1.1.24 and adjusted for category handling (‘struct __locale_struct’ has no member named ‘cat’; for now copied from 1.1.24 and changed to drop references to cat field in __locale_struct)

These 4 files symlink to musl 0.9.12 version
intl.c
catopen.c
duplocale.c
setlocale.c

freelocale.c has been tweaked to handle memory destruction in OSv unique way.
langinfo.c has been updated to 1.1.24 copy and tweaked to remove logic referencing loc->cat (new cat field)

These 4 files go together and can possibly be replaced with musl copies.
strtod_l.c
strtof_l.c
strtold_l.c
../stdlib/strtod.c

# These files in 1.1.24 more less map to what intl.c used to be
musl += locale/c_locale.o
musl += locale/__lctrans.o
musl += locale/bind_textdomain_codeset.o
musl += locale/dcngettext.o -> Fails to compile (old musl does not have it)
musl += locale/textdomain.o
