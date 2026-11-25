/* tcp_client.c
 * TCP communication with the Directory Server (DS).
 *
 * TCP is connection-oriented and stream-based: data is a continuous stream
 * of bytes with NO message boundaries, so one write() on one side does not
 * correspond to one read() on the other. Because of this, sending and
 * receiving are done in loops. The protocol ends each message with '\n'.
 *
 * Blocking calls can fail in three ways, told apart by errno:
 *   - EINTR:              interrupted by a signal -> not an error, retry
 *   - EAGAIN/EWOULDBLOCK: the socket timeout expired -> give up
 *   - anything else:      real error -> give up
 *
 * Functions:
 *   - tcp_connect():    resolve address, create socket, set timeouts, connect
 *   - tcp_request():    one request/reply exchange with the DS
 */

#include <string.h>     // memset, strlen
#include <unistd.h>     // read, write, close
#include <sys/types.h>  // size_t, 
#include <sys/socket.h> // socket, connect, setsockopt
#include <netdb.h>      // getaddrinfo, struct addrinfo
#include <sys/time.h>   // struct timeval

#include "client/network/tcp_client.h"
#include "common/config.h"  // TCP_TIMEOUT_SEC
#include "common/io.h"

int tcp_connect(const char *ip, const char *port) {
    
    struct addrinfo hints;  // IP version (IPv4), protocol (TCP)
    struct addrinfo *res;   // resolved address info
    struct timeval tv;      // timeout value
    int fd;                 // TCP socket

    memset(&hints, 0, sizeof(hints));   // unset fields set to 0 (no trash)
    hints.ai_family = AF_INET;          // IPv4
    hints.ai_socktype = SOCK_STREAM;    // TCP

    if (getaddrinfo(ip, port, &hints, &res) != 0) return -1;

    // SOCKET()
    fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == -1) {
        freeaddrinfo(res);
        return -1;
    }

    // Without timeouts, connect, read and write could block forever.
    // SO_RCVTIMEO covers read(), SO_SNDTIMEO covers write() and, on Linux, connect().
    tv.tv_sec = TCP_TIMEOUT_SEC; 
    tv.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1 ||
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == -1 ||
        connect(fd, res->ai_addr, res->ai_addrlen) == -1) {
        
        close(fd);
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res); // not needed once connected
    return fd;
}

int tcp_request(const char *ip, const char *port, const char *request,
                char *reply, size_t reply_size) {

    int fd;         // TCP socket
    ssize_t total;  // reply size (including '\n')

    if ((fd = tcp_connect(ip, port)) == -1) return 1;
    
    if (io_write_all(fd, request, strlen(request)) == -1) {
        close(fd);
        return 1;
    }

    total = io_read_line(fd, reply, reply_size);
    close(fd); // done with the connection
    
    if (total == -1) return 1;

    reply[total-1] = '\0'; // remove '\n'
    return 0; 
}