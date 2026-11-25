/* tcp_client.h
 * Public interface for the TCP client module.
 *
 * Unlike UDP (one socket created at startup and reused), TCP opens a new
 * connection for each request: connect, send, receive, close.
 */

#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <stddef.h> // size_t

/**
 * @brief Opens a TCP connection to ip:port, with send/receive timeouts.
 *
 * @param ip    Server IP address.
 * @param port  Server TCP port.
 *
 * @return The connected socket, or -1 on error. The caller must close() it.
 */
int tcp_connect(const char *ip, const char *port);

/** (tcp_request mantém-se como está) */
int tcp_request(const char *ip, const char *port, const char *request,
                char *reply, size_t reply_size);

#endif