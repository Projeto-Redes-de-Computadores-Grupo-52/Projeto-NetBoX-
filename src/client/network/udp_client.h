#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

#include <netdb.h>

int init_connection(const char *ip, const char *port, int *fd, struct addrinfo **res);
int communicate(int fd, char *message, struct addrinfo *res);

#endif