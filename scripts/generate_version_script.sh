#!/bin/bash

VERSION_SCRIPT_FILE=$1

extract_symbols_from_library()
{
  local LIB_NAME=$1
  local LIB_PATH=$(gcc -print-file-name=$LIB_NAME)
  echo "/*------- $LIB_PATH */" >> $VERSION_SCRIPT_FILE
  nm -g -CD --defined-only $LIB_PATH | awk '// { printf("    %s;\n",$3) }' >> $VERSION_SCRIPT_FILE
}


VERSION_SCRIPT_START=$(cat <<"EOF"
{
  global:
EOF
)

VERSION_SCRIPT_END=$(cat <<"EOF"
  local:
    *;
};
EOF
)

echo "$VERSION_SCRIPT_START" > $VERSION_SCRIPT_FILE

extract_symbols_from_library "libresolv.so.2"
extract_symbols_from_library "libc.so.6"
extract_symbols_from_library "libm.so.6"
extract_symbols_from_library "ld-linux-x86-64.so.2"
extract_symbols_from_library "libpthread.so.0"
extract_symbols_from_library "libdl.so.2"
extract_symbols_from_library "librt.so.1"
extract_symbols_from_library "libcrypt.so.1"
extract_symbols_from_library "libaio.so.1"
extract_symbols_from_library "libxenstore.so.3.0"
#extract_symbols_from_library "libboost_system.so"

echo "$VERSION_SCRIPT_END" >> $VERSION_SCRIPT_FILE
#-fvisibility=hidden -fvisibility-inlines-hidden
