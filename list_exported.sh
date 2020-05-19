#nm -g -D -C --defined-only $1
#nm --extern-only --dynamic -C --defined-only $1
#---------------------------------------------------
nm --extern-only --dynamic -C --defined-only /lib/x86_64-linux-gnu/libresolv-2.31.so > /tmp/libresolv.so.2
nm --extern-only --dynamic -C --defined-only /lib/x86_64-linux-gnu/libc-2.31.so > /tmp/libc.so.6
nm --extern-only --dynamic -C --defined-only /lib/x86_64-linux-gnu/libm-2.31.so > /tmp/libm.so.6
nm --extern-only --dynamic -C --defined-only /lib/x86_64-linux-gnu/ld-2.31.so > /tmp/ld-linux-x86-64.so.2
nm --extern-only --dynamic -C --defined-only /lib/x86_64-linux-gnu/libpthread-2.31.so > /tmp/libpthread.so.0
nm --extern-only --dynamic -C --defined-only /lib/x86_64-linux-gnu/libdl-2.31.so > /tmp/libdl.so.2
nm --extern-only --dynamic -C --defined-only /lib/x86_64-linux-gnu/librt-2.31.so > /tmp/librt.so.1
nm --extern-only --dynamic -C --defined-only /usr/lib/x86_64-linux-gnu/libaio.so.1.0.1 > /tmp/libaio.so.1
nm --extern-only --dynamic -C --defined-only /usr/lib/x86_64-linux-gnu/libxenstore.so.3.0.3 > /tmp/libxenstore.so.3.0
nm --extern-only --dynamic -C --defined-only /lib/x86_64-linux-gnu/libcrypt.so.1.1.0 > /tmp/libcrypt.so.1
#----------------------------
#/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.28
