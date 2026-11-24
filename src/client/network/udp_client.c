/* udp_client.c
 * UDP communication with the Directory Server (DS).
 * Responsibilities:
 *   - init_connection(): create the UDP socket and resolve the DS address
 *   - communicate(): send one request and receive its reply
 */

#include <string.h> // memset, strlen
#include <unistd.h> // close
#include <sys/socket.h> // socket, sendto, recvfrom
#include <netdb.h> // getaddrinfo, struct addrinfo
#include <sys/time.h> // struct timeval
#include <errno.h>
#include <stdio.h>

#include "common/config.h" // BUF_SIZE
#include "client/network/udp_client.h"

int udp_init_connection(const char *ip, const char *port, int *fd, struct addrinfo **res) {

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

// message is used twice - on input it holds the request (ending in '\n'), on output it holds the reply (no '\n')
int udp_communicate(int fd, char *message, struct addrinfo *res) {

    char request[BUF_SIZE+1]; // copy of request for retransmission
    ssize_t n; // bytes sent/received (-1 on error)
    socklen_t addrlen;
    struct sockaddr_in addr;

    snprintf(request, sizeof(request), "%s", message); // copy

    for (int attempt = 0; attempt < UDP_MAX_TRIES; attempt++) {

            do { // send the request, try again if interrupted by signal
                n = sendto(fd, request, strlen(request), 0, res->ai_addr, res->ai_addrlen);
            } while (n == -1 && errno == EINTR);
            if (n == -1) return 1; // real error

            do { // wait for the reply
                addrlen = sizeof(addr);
                n = recvfrom(fd, message, BUF_SIZE, 0, (struct sockaddr *)&addr, &addrlen);
            } while (n == -1 && errno == EINTR);

            if (n >= 0) break; // we got a reply

            // real error
            if (errno != EAGAIN && errno != EWOULDBLOCK) return 1;

            // EAGAIN/EWOULDBLOCK timeout, loop and retransmit
    }

    if (n < 0) {
        printf("No reply from DS (timeout).\n");
        return 1;
    }

    message[n] = '\0';
    if (n > 0 && message[n-1] == '\n') message[n-1] = '\0'; // remove the '\n' from the reply

    return 0;
}