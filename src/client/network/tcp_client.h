#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <stddef.h>

int tcp_request(const char *ip, const char *port, const char *request,
                char *reply, size_t reply_size);

#endif