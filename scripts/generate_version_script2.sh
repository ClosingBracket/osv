nm -uC --format posix apps/redis-memonly/redis-server | cut -d "@" -f 1 | cut -f 1 | awk '// { printf("    %s;\n", $0) }'
