The only strerror.c provides deprecated sys_errlist and sys_nerr symbols
that musl does not provide. So should stay as is and unaffected by musl upgrade.
