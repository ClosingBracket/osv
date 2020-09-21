res_msend.o #For now copy from musl 1.1.24 and uncomment _pthread_cleanup_push/_pthread_cleanup_pop (deal later with it)
freeaddrinfo.o #copied from musl 0.9.12, in 1.1.24 more than LOCK, mess with changes to ai_buf struct -> affects our getaddrinfo.c
