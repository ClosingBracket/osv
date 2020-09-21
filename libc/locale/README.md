langinfo.c - copied from musl 1.1.24 and adjusted for category handling (‘struct __locale_struct’ has no member named ‘cat’; for now copied from 1.1.24 and changed to drop references to cat field in __locale_struct)
# These files in 1.1.24 more less map to what intl.c used to be
musl += locale/c_locale.o
musl += locale/__lctrans.o
musl += locale/bind_textdomain_codeset.o
musl += locale/dcngettext.o -> Fails to compile (old musl does not have it)
musl += locale/textdomain.o
