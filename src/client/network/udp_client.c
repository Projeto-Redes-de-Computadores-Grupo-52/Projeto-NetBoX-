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

#include "common/config.h" // BUF_SIZE
#include "client/network/udp_client.h"

int udp_init_connection(const char *ip, const char *port, int *fd, struct addrinfo **res) {

    struct addrinfo hints; // the kind of address we want

    *fd = socket(AF_INET, SOCK_DGRAM, 0); // IPv4, UDP, 0 = default protocol
    if (*fd == -1) return 1;

    // TO-DO timeout

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

    ssize_t n; // bytes sent/received (-1 on error)
    socklen_t addrlen;
    struct sockaddr_in addr;

    // SEND
    n = sendto(fd, message, strlen(message), 0, res->ai_addr, res->ai_addrlen);
    if (n == -1) return 1; // error sending

    // RECEIVE
    addrlen = sizeof(addr);
    n = recvfrom(fd, message, BUF_SIZE, 0, (struct sockaddr *)&addr, &addrlen);
    if (n == -1) return 1;   //error or TIMEOUT receiving

    message[n] = '\0';
    if (n > 0 && message[n-1] == '\n') message[n-1] = '\0'; // remove the '\n' from the reply

    return 0;
}