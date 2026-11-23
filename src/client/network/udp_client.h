#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

#include <netdb.h>

int udp_init_connection(const char *ip, const char *port, int *fd, struct addrinfo **res);
int udp_communicate(int fd, char *message, struct addrinfo *res);

#endif