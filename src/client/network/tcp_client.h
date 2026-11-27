/**
 * @file tcp_client.h
 * @brief Public interface of the TCP client module.
 *
 * Unlike UDP (one socket created at startup and reused), TCP opens a new
 * connection for each request: connect, send, receive, close.
 *
 * tcp_request() is used by the "versions" command (VRS / RVR).
 * tcp_connect() is used by the "get" command. Reading and writing on the resulting
 * socket is done with the helpers in common/io.h.
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

/**
 * @brief Sends one request over a new TCP connection and reads one reply line.
 *
 * Connects, sends @p request, reads until '\n' and closes the connection.
 * Prints nothing: the caller decides what to show the user.
 *
 * @param ip          Server IP address.
 * @param port        Server TCP port.
 * @param request     Message to send, ending in "\n".
 * @param reply       [out] Buffer that receives the reply as a C string,
 *                    without the trailing '\n'.
 * @param reply_size  Total size of @p reply, including space for '\0'.
 *
 * @return 0 on success, 1 on error (connection failure, timeout, or an
 *         incomplete reply that does not end in '\n').
 */
int tcp_request(const char *ip, const char *port, const char *request,
                char *reply, size_t reply_size);

#endif /* TCP_CLIENT_H */