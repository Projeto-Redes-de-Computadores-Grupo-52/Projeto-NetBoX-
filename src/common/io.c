/* io.c
 * Generic read/write helpers on file descriptors (sockets or files).
 *
 * read() and write() may transfer fewer bytes than requested, and may be
 * interrupted by a signal. Every function here loops until the job is done,
 * retrying on EINTR and returning -1 on any other error.
 */

#include <errno.h>      // errno, EINTR
#include <unistd.h>     // read, write

#include "common/io.h"

int io_write_all(int fd, const char *buf, size_t len) {

    size_t sent = 0;    // bytes already written
    ssize_t n;          // bytes written by one write() call

    // write() may accept fewer bytes than asked (e.g. full send buffer),
    // so advance the pointer and keep going until everything is sent
    while (sent < len) {
        n = write(fd, buf + sent, len - sent);
        if (n < 0) {
            if (errno == EINTR) continue;   // signal interruption, try again
            return -1;                      // real error or timeout
        }
        if (n == 0) return -1;              // not expected, treat as error
        sent += (size_t)n;                  // safe cast, n > 0 here
    }
    return 0;
}

ssize_t io_read_line(int fd, char *buf, size_t size) {

    size_t total = 0;   // bytes already read
    ssize_t n;          // bytes read by one read() call

    // TCP has no message boundaries, one read() may return part of a line,
    // so keep appending until the '\n' arrives
    // size-1 keeps one byte free for the '\0' the caller will add

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
    // treat as an error rather than a silently truncated reply
    if (total == 0 || buf[total-1] != '\n') return -1;
    return (ssize_t)total;
}

int io_read_exact(int fd, char *buf, size_t n) {

    size_t total = 0;   // bytes already read
    ssize_t r;          // bytes read by one read() call

    // used for data of known size (e.g. the file in "get"), unlike a line,
    // the end is given by the byte count, not by a terminator
    while (total < n) {
        r = read(fd, buf + total, n - total);
        if (r < 0) {
            if (errno == EINTR) continue;   // retry
            return -1;                      // real error or timeout
        }
        if (r == 0) return -1;              // peer closed before sending n bytes
        total += (size_t)r;
    }
    return 0;
}