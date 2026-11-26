/**
 * @file commands.h
 * @brief User commands: build the protocol messages, talk to the DS and
 *        show the result to the user.
 *
 * Each function validates its arguments locally, sends the request
 * (UDP for all, TCP for "versions"), interprets the status of the reply
 * and prints a human-friendly message.
 *
 * Return convention (all functions): 0 on success, 1 on any error
 * (invalid arguments, no reply, unexpected reply, or a status other
 * than OK). The error is always reported to the user inside the
 * function, so the caller only needs the return value when it affects
 * the session state.
 *
 * Checking whether a user is logged in is NOT done here: it is the
 * caller's job (main), since only main keeps the session.
 */

#ifndef COMMANDS_H
#define COMMANDS_H

#include <netdb.h> // struct addrinfo

/**
 * @brief Logs a user in (LIN / RLI). Registers the user if new.
 *
 * Validates the UID (6 digits) and the password (8 alphanumeric)
 * before sending.
 *
 * @param uid       User ID.
 * @param password  User password.
 * @param peerport  TCP port where this application accepts peer requests.
 * @param fd        UDP socket created by udp_open.
 * @param res       DS address created by udp_open.
 *
 * @return 0 if the DS replied OK (login) or REG (new user registered),
 *         1 otherwise (wrong password, error, timeout...).
 */
int login(const char *uid, const char *password, const char *peerport, int fd, struct addrinfo *res);

/**
 * @brief Logs the user out (LOU / RLO).
 *
 * @param uid       User ID of the logged-in user.
 * @param password  Password of the logged-in user.
 * @param fd        UDP socket.
 * @param res       DS address.
 *
 * @return 0 if the DS replied OK, 1 otherwise.
 */
int logout(const char *uid, const char *password, int fd, struct addrinfo *res);

/**
 * @brief Unregisters the user (UNR / RUR). The DS also logs the user out.
 *
 * @param uid       User ID of the logged-in user.
 * @param password  Password of the logged-in user.
 * @param fd        UDP socket.
 * @param res       DS address.
 *
 * @return 0 if the DS replied OK, 1 otherwise.
 */
int unregister(const char *uid, const char *password, int fd, struct addrinfo *res);

/**
 * @brief Lists the resources known in the network (LST / RLS).
 *
 * Needs no login. Prints at most 50 filenames, one per line, or a message
 * if there are no published resources.
 *
 * @param fd   UDP socket.
 * @param res  DS address.
 *
 * @return 0 on success (including "no resources"), 1 on error.
 */
int list(int fd, struct addrinfo *res);

/**
 * @brief Removes a resource previously published by this user (REM / RRM).
 *
 * Validates the filename before sending.
 *
 * @param uid       User ID of the logged-in user.
 * @param password  Password of the logged-in user.
 * @param filename  Name of the resource to remove.
 * @param fd        UDP socket.
 * @param res       DS address.
 *
 * @return 0 if the DS replied OK, 1 otherwise (e.g. resource not found).
 */
int remove_resource(const char *uid, const char *password, const char *filename, int fd, struct addrinfo *res);

/**
 * @brief Publishes a local file (PUB / RPB).
 *
 * Validates the filename and the label, and checks that the file exists
 * in the current directory, is a regular file and is at most FSIZE_MAX
 * bytes. If any check fails, nothing is sent to the DS. Publishing the
 * same file again updates the existing record.
 *
 * @param uid       User ID of the logged-in user.
 * @param password  Password of the logged-in user.
 * @param filename  Name of the local file to publish.
 * @param label     Description of this version (1-20 chars: letters, digits, '\-' and '_').
 * @param fd        UDP socket.
 * @param res       DS address.
 *
 * @return 0 if the DS replied OK, 1 otherwise.
 */
int publish(const char *uid, const char *password, const char *filename, const char *label, int fd, struct addrinfo *res);

/**
 * @brief Shows who publishes a resource and its metadata (VRS / RVR, over TCP).
 *
 * Needs no login. Prints a table with UID, size, label, publication time
 * and whether each peer is online. Opens a new TCP connection to the DS.
 *
 * @param filename  Name of the resource.
 * @param ip        DS IP address.
 * @param port      DS TCP port.
 *
 * @return 0 on success (including "no peers"), 1 on error.
 */
int versions(const char *filename, const char *ip, const char *port);

#endif /* COMMANDS_H */