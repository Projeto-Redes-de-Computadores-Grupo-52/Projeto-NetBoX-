/**
 * @file udp_client.h
 * @brief Public interface of the UDP client module.
 *
 * UDP is connectionless: there is no connect()/accept(). The socket is
 * created once at startup (udp_open) and reused for every request. Each
 * request is one datagram and each reply is one datagram.
 *
 * Used by the commands that talk to the DS over UDP :
 * login, logout, unregister, publish, remove and list.
 */

#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

#include <netdb.h> // struct addrinfo
#include <stddef.h> // size_t

/**
 * @brief Creates the UDP socket (with a receive timeout) and resolves the DS address.
 *
 * The caller must later call freeaddrinfo(*res) and close(*fd).
 *
 * @param ip    DS IP address (e.g. "193.136.138.142").
 * @param port  DS UDP port (e.g. "59000").
 * @param fd    [out] Socket file descriptor.
 * @param res   [out] Resolved DS address, allocated by getaddrinfo.
 *
 * @return 0 on success, 1 on error.
 */
int udp_open(const char *ip, const char *port, int *fd, struct addrinfo **res);

/**
 * @brief Sends a request to the DS and waits for the reply.
 *
 * If no reply arrives before the timeout, the request is retransmitted,
 * up to UDP_MAX_TRIES attempts in total. Prints nothing: the caller
 * decides what to show the user.
 *
 * @param fd          Socket created by udp_open.
 * @param request     Message to send, ending in '\\n' (not modified).
 * @param reply       [out] Buffer that receives the reply as a C string,
 *                    without the trailing '\\n'.
 * @param reply_size  Total size of @p reply, including space for '\\0'.
 * @param res         DS address created by udp_open.
 *
 * @return 0 on success,
 *         1 on error (sendto/recvfrom failure),
 *         2 if no reply was received after all attempts (timeout).
 */
int udp_communicate(int fd, const char *request, char *reply, size_t reply_size, struct addrinfo *res);

#endif /* UDP_CLIENT_H */