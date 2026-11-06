#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include "common/config.h"
#include "client/network/udp_client.h"

int init_connection(const char *ip, const char *port, int *fd, struct addrinfo **res) {

    struct addrinfo hints;

    *fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (*fd == -1) return 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(ip, port, &hints, res) != 0) {
        close(*fd);
        return 1;
    }

    return 0;
}

int communicate(int fd, char *message, struct addrinfo *res) {

    ssize_t n;
    socklen_t addrlen;
    struct sockaddr_in addr;

    n = sendto(fd, message, strlen(message), 0, res->ai_addr, res->ai_addrlen);
    if (n == -1) return 1;

    addrlen = sizeof(addr);
    n = recvfrom(fd, message, BUF_SIZE, 0, (struct sockaddr *)&addr, &addrlen);
    if (n == -1) return 1;   /* erro ou timeout */

    message[n] = '\0';
    if (n > 0 && message[n-1] == '\n') message[n-1] = '\0';

    return 0;
}