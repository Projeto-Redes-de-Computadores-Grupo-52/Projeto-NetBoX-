/**
 * @file io.h
 * @brief Generic I/O helpers on file descriptors (sockets or files).
 *
 * read() and write() may transfer fewer bytes than requested, and may be
 * interrupted by a signal (EINTR). These helpers hide both cases.
 * They know nothing about UDP, TCP or the DS protocol.
 */

#ifndef IO_H
#define IO_H

#include <stddef.h> // size_t, ssize_t

/**
 * @brief Writes exactly len bytes (loops on partial writes and EINTR).
 *
 * @param fd   Descriptor to write to.
 * @param buf  Data to write.
 * @param len  Number of bytes to write.
 *
 * @return 0 on success, -1 on error (including send timeout).
 */
int io_write_all(int fd, const char *buf, size_t len);

/**
 * @brief Reads until '\\n', until the peer closes, or until the buffer is full.
 *
 * One byte of the buffer is kept free for the '\\0', which is NOT added
 * here: the caller terminates the string.
 *
 * @param fd    Descriptor to read from.
 * @param buf   [out] Buffer that receives the data.
 * @param size  Total size of @p buf.
 *
 * @return Number of bytes read (including the '\n'), or -1 on error or if
 *         the message is incomplete (no '\\n' at the end).
 */
ssize_t io_read_line(int fd, char *buf, size_t size);

/**
 * @brief Reads exactly n bytes (loops on partial reads and EINTR).
 *
 * Meant for data of known size, such as the file in the "get" command.
 *
 * @param fd   Descriptor to read from.
 * @param buf  [out] Buffer that receives the data (at least n bytes).
 * @param n    Number of bytes to read.
 *
 * @return 0 on success, -1 on error or if the peer closed before n bytes.
 */
int io_read_exact(int fd, char *buf, size_t n);

#endif /* IO_H */