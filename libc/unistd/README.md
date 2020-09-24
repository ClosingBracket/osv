All sources here never came from musl so where either fully implemented
on OSv side or are just stubs.

Implemented on OSv side:
----------
getppid.c
getsid.c
getpgid.c
getpgrp.c
sethostname.c
ttyname_r.c

Stubs:
-----
setpgid.cc
setsid.cc
sync.cc
