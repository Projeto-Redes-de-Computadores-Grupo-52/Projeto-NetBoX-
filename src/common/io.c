/* io.c
 * Generic read/write helpers shared by the network modules.
 * Blocking calls are retried on EINTR; any other error is returned.
 */

#include <errno.h>      // errno, EINTR
#include <unistd.h>     // read, write

#include "common/io.h"

int io_write_all(int fd, const char *buf, size_t len) {

    size_t sent = 0;    // bytes already written
    ssize_t n;          // bytes written by one write() call

    while (sent < len) {
        n = write(fd, buf + sent, len - sent);
        if (n < 0) {
            if (errno == EINTR) continue;   // signal interruption, try again
            return -1;                      // real error or timeout
        }
        if (n == 0) return -1;              // not expected, treat as error
        sent += (size_t)n;
    }
    return 0;
}

ssize_t io_read_line(int fd, char *buf, size_t size) {

    size_t total = 0;   // bytes already read
    ssize_t n;          // bytes read by one read() call

    while (total < size - 1) {              // 1 byte kept free for '\0'
        n = read(fd, buf + total, size - 1 - total);
        if (n < 0) {
            if (errno == EINTR) continue;   // retry
            return -1;                      // real error or timeout
        }
        if (n == 0) break;                  // closed by the peer
        total += (size_t)n;
        if (buf[total-1] == '\n') break;    // complete message
    }

    // nothing received, or buffer full / peer closed before the '\n'
    if (total == 0 || buf[total-1] != '\n') return -1;
    return (ssize_t)total;
}

int io_read_exact(int fd, char *buf, size_t n) {

    size_t total = 0;   // bytes already read
    ssize_t r;          // bytes read by one read() call

    while (total < n) {
        r = read(fd, buf + total, n - total);
        if (r < 0) {
            if (errno == EINTR) continue;   // retry
            return -1;                      // real error or timeout
        }
        if (r == 0) return -1;              // peer closed before n bytes
        total += (size_t)r;
    }
    return 0;
}