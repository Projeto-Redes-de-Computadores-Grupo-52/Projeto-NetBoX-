#ifndef COMMANDS_H
#define COMMANDS_H

#include <netdb.h>

int login(const char *uid, const char *password, const char *peerport, int fd, struct addrinfo *res);
int logout(const char *uid, const char *password, int fd, struct addrinfo *res);
int unregister(const char *uid, const char *password, int fd, struct addrinfo *res);
int list(int fd, struct addrinfo *res);
int remove_resource(const char *uid, const char *password, const char *filename, int fd, struct addrinfo *res);

#endif