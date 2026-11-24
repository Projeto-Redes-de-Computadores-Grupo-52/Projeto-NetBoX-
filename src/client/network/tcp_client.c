#include <string.h> // memset, strlen
#include <unistd.h> // read, write, close
#include <sys/types.h>
#include <sys/socket.h> // socket, connect
#include <netdb.h> // getaddrinfo, struct addrinfo
#include <errno.h>
#include <sys/time.h>

#include "client/network/tcp_client.h"
#include "common/config.h"

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

    struct timeval tv;
    tv.tv_sec = TCP_TIMEOUT_SEC; 
    tv.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1
        || setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == -1) {
        close(fd);
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

    // WRITE - loop so we make sure we send everything
    while (sent < len) {
        ssize_t n = write(fd, request + sent, len - sent);
        if (n < 0) {
            if (errno == EINTR) continue; // signal interruption, just try again
            close(fd);
            return 1;
        }
        if (n == 0) { // free addr + EINTR
            close(fd);
            return 1;
        }
        sent += (size_t)n;
    }

    // READ - loop until we find '\n' in the reply or the DS ends the connection
    while (total < reply_size - 1) {
        ssize_t n = read(fd, reply + total, reply_size - 1 - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            close(fd); // real error or timeout
            return 1;
        }
        if (n == 0) break; // connection closed by DS
        total += (size_t)n;
        if (reply[total-1] == '\n') break; // complete reply received
    }
    close(fd);

    if (total == 0) return 1; // nothing received

    // buffer full or connection closed early
    if (reply[total-1] != '\n') return 1;
    
    // remove '\n' from the reply
    reply[--total] = '\0';

    return 0;
}