/**
 * @file config.h
 * @brief Constants shared by the whole project: buffer sizes, DS address,
 *        protocol limits and timeouts.
 */

#ifndef CONFIG_H
#define CONFIG_H

// BUFFERS
#define BUF_SIZE 4096               // general buffer (user input, UDP request/reply)
#define VERSIONS_BUF (4 * BUF_SIZE) // the "versions" table could have many entries

// DS
#define DSIP_1 "193.136.138.142" // outside
#define DSIP_2 "192.168.1.1" // LT5
#define DSPORT "59000"

// PORTS
#define PORT_MIN 1
#define PORT_MAX 65535

// PROTOCOL LIMITS
#define UID_SIZE 6
#define PWD_SIZE 8
#define FILENAME_MAX_LEN 24
#define LABEL_MAX_LEN 20
#define FSIZE_MAX 10000000

// TIMEOUTS AND RETRANSMISSION
#define UDP_TIMEOUT_SEC 5
#define UDP_MAX_TRIES 3
#define TCP_TIMEOUT_SEC 5

#endif /* CONFIG_H */