#include <string.h> // memset, strlen
#include <unistd.h> // read, write, close
#include <sys/types.h>
#include <sys/socket.h> // socket, connect
#include <netdb.h> // getaddrinfo, struct addrinfo

#include "client/network/tcp_client.h"

int tcp_request(const char *ip, const char *port, const char *request,
                char *reply, size_t reply_size) {

    struct addrinfo hints, *res; // hints - what we want; res - result list
    int fd; // TCP socket
    size_t sent = 0; // bytes already written
    size_t len = strlen(request); // total bytes to write
    size_t total = 0; // bytes already read (we know the reply ends with '\n')

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP

    if (getaddrinfo(ip, port, &hints, &res) != 0) return 1;

    // SOCKET()
    fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == -1) {
        freeaddrinfo(res);
        return 1;
    }

    // CONNECT() - TCP handshake
    if (connect(fd, res->ai_addr, res->ai_addrlen) == -1) {
        close(fd);
        freeaddrinfo(res);
        return 1;
    }
    freeaddrinfo(res); // we don't need it once connected

    // loop so we make sure we send everything
    while (sent < len) {
        ssize_t n = write(fd, request + sent, len - sent);
        if (n <= 0) {
            close(fd); // free addr + EINTR
            return 1;
        }
        sent += (size_t)n;
    }

    // loop until we find 'n' in the reply or the DS ends the connection
    while (total < reply_size - 1) {
        ssize_t n = read(fd, reply + total, reply_size - 1 - total);
        if (n < 0) {
            close(fd);
            return 1;
        }
        if (n == 0) break;
        total += (size_t)n;
        if (reply[total-1] == '\n') break;
    }
    close(fd);

    if (total == 0) return 1; // nothing received
    
    // remove '\n' from the reply
    if (reply[total-1] == '\n') total--; 
    reply[total] = '\0';

    return 0;
}