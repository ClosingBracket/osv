nm -uC --format posix apps/rust-pie-httpserver/target/release/httpserver | cut -d "@" -f 1 | cut -f 1 | awk '// { printf("    %s;\n", $0) }'
