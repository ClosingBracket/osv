#include <sys/socket.h>
#undef socket
#define socket(domain,type,protocol) socket(AF_INET,type,protocol)
