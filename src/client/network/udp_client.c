/* udp_client.c
 * UDP communication with the Directory Server (DS).
 * Responsibilities:
 *   - udp_open(): create the UDP socket and resolve the DS address
 *   - communicate(): send one request and receive its reply
 */

#include <string.h>     // memset, strlen
#include <unistd.h>     // close
#include <sys/socket.h> // socket, sendto, recvfrom
#include <netdb.h>      // getaddrinfo, struct addrinfo
#include <sys/time.h>   // struct timeval
#include <errno.h>

#include "common/config.h"
#include "client/network/udp_client.h"

int udp_open(const char *ip, const char *port, int *fd, struct addrinfo **res) {

    struct addrinfo hints; // the kind of address we want
    struct timeval tv;

    *fd = socket(AF_INET, SOCK_DGRAM, 0); // IPv4, UDP, 0 = default protocol
    if (*fd == -1) return 1;

    tv.tv_sec = UDP_TIMEOUT_SEC;
    tv.tv_usec = 0;
    if (setsockopt(*fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) {
        close(*fd);
        return 1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP

    // save addrinfo in res
    if (getaddrinfo(ip, port, &hints, res) != 0) {
        close(*fd);
        return 1;
    }

    return 0;
}

int udp_communicate(int fd, const char *request, char *reply, size_t reply_size, struct addrinfo *res) {

    ssize_t n; // bytes sent/received (-1 on error)
    socklen_t addrlen;
    struct sockaddr_in addr;

    for (int attempt = 0; attempt < UDP_MAX_TRIES; attempt++) {

        do { // send the request, try again if interrupted by signal
            n = sendto(fd, request, strlen(request), 0, res->ai_addr, res->ai_addrlen);
        } while (n == -1 && errno == EINTR);
        if (n == -1) return 1; // real error

        do { // wait for the reply
            addrlen = sizeof(addr);
            n = recvfrom(fd, reply, reply_size-1, 0, (struct sockaddr *)&addr, &addrlen);
        } while (n == -1 && errno == EINTR);

        if (n >= 0) break; // we got a reply

        // real error
        if (errno != EAGAIN && errno != EWOULDBLOCK) return 1;

        // EAGAIN/EWOULDBLOCK - timeout, loop and retransmit
    }

    if (n < 0) return 2; // all attempts timed out

    reply[n] = '\0';
    if (n > 0 && reply[n-1] == '\n') reply[n-1] = '\0'; // remove '\n' from the reply

    return 0;
}