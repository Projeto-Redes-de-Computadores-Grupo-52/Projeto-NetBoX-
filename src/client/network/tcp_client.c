#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "client/network/tcp_client.h"

/* Devolve 0 se correu bem. reply fica terminada em '\0', sem o '\n' final. */
int tcp_request(const char *ip, const char *port, const char *request,
                char *reply, size_t reply_size) {

    struct addrinfo hints, *res;
    int fd;
    size_t sent = 0, len = strlen(request), total = 0;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;   /* TCP */

    if (getaddrinfo(ip, port, &hints, &res) != 0) return 1;

    fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == -1) {
        freeaddrinfo(res);
        return 1;
    }

    if (connect(fd, res->ai_addr, res->ai_addrlen) == -1) {
        close(fd);
        freeaddrinfo(res);
        return 1;
    }
    freeaddrinfo(res);

    /* write pode escrever menos do que pedido: ciclo até enviar tudo */
    while (sent < len) {
        ssize_t n = write(fd, request + sent, len - sent);
        if (n <= 0) {
            close(fd);
            return 1;
        }
        sent += (size_t)n;
    }

    /* read pode devolver menos: ciclo até '\n' ou o DS fechar a ligação */
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

    if (total == 0) return 1;                 /* nada recebido */
    if (reply[total-1] == '\n') total--;
    reply[total] = '\0';

    return 0;
}