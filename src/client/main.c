/* main.c
 * Entry point for the user application.
 * Responsibilities:
 *   1. read and check command line arguments (-m, -n, -p)
 *   2. initiates the UDP socket to start communication with the DS
 *   3. saves the current logged-in user and it's password
 *   4. read commands from the terminal
 */

#include <stdio.h>
#include <stdlib.h> // strtol
#include <string.h> // strcmp, strcpy, strlen
#include <unistd.h> // close
#include <signal.h> // signal, SIGPIPE
#include <netdb.h> //struct addrinfo, freeaddrinfo
#include <errno.h> // errno, EINTR

#include "common/config.h" // BUF_SIZE, DSIP_1, DSPORT, PORT_MIN/MAX
#include "client/commands.h" // login, logout, publish, ...
#include "client/network/udp_client.h" // udp_init_connection

// help message for user application
#define USAGE "Usage: ./user -m (peerport) [-n (ip)] [-p (port)]\n"

// set to 1 by SIGINT handler and checked in the main loop
// volatile sig_atomic_t is the only type safe to write from a handler
static volatile sig_atomic_t got_sigint = 0;

static void handle_sigint(int sig) {
    (void)sig;
    got_sigint = 1; // minimum possible: no printf/sendto allowed here
}

// Converts a string into a port number between 1-65535 or 0 if invalid.
// Use strtol instead of atoi so "123abc" (for example) isn't valid.
static int parse_port(const char *s) {
    char *end;
    long v = strtol(s, &end, 10);

    if (*s == '\0' || *end != '\0' || v < PORT_MIN || v > PORT_MAX) return 0;
    return (int)v;
}

int main(int argc, char *argv[]) {

    int fd; // UDP socket
    struct addrinfo *res; // resolved DS address

    char buffer[BUF_SIZE+1]; // full line given by user
    char cmd[BUF_SIZE+1]; // command, the first word in the line
    char arg1[BUF_SIZE+1], arg2[BUF_SIZE+1]; // first and second command argument
    char extra[BUF_SIZE+1]; // extra arguments, if necessary

    // empty UID ("") means no user is logged in
    char uid[BUF_SIZE+1] = "";
    char password[BUF_SIZE+1] = "";

    const char *peerport_s = NULL; // (-m)
    const char *ip = DSIP_1; // (-n)
    const char *port_s = DSPORT; // (-p)

    // if the DS closes the TCP connection in the middle of a write(), treat the error as usual
    signal(SIGPIPE, SIG_IGN);

    // sigaction WITHOUT SA_RESTART, so fgets is interrupted by Ctrl+C
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        printf("Error: sigaction()\n");
        return 1;
    }

    // command line arguments parsing
    int opt;
    while ((opt = getopt(argc, argv, "m:n:p:")) != -1) {
        switch (opt) {
            case 'm': peerport_s = optarg; break;
            case 'n': ip = optarg; break;
            case 'p': port_s = optarg; break;
            default: // unknown flag or missing value
                printf(USAGE);
                return 1;
        }
    }

    // check arguments
    if (peerport_s == NULL) {
        printf("No -m flag.\n" USAGE);
        return 1;
    }
    if (!parse_port(peerport_s)) {
        printf("%s: Invalid peer port (1-65535).\n", peerport_s);
        return 1;
    }
    if (!parse_port(port_s)) {
        printf("%s: Invalid DS port (1-65535).\n", port_s);
        return 1;
    }

    // UDP connection
    if (udp_init_connection(ip, port_s, &fd, &res)) {
        printf("Error: udp_init_connection()\n");
        return 1;
    }

    // main loop (commands)
    while (1) {

        if (got_sigint) break; // Ctrl+C

        printf("> ");
        fflush(stdout);

        if (fgets(buffer, sizeof(buffer), stdin) == NULL) { // get line from user
            if (errno == EINTR) { // interrupted by a signal
                clearerr(stdin); // so the next fgets doesn't fail
                if (got_sigint) { printf("\n"); break; } // Ctrl+C
                continue; // other signal, try again
            }
            break; // Ctrl+D (EOF) or read error
        }

        // ignore empty lines
        if (sscanf(buffer, "%s", cmd) != 1) continue;

        // strcmp avoids "logoutxyz" (for example) being valid

        // LOGIN [login UID password]
        if (!strcmp(cmd, "login")) {
            if (strlen(uid) != 0) // user already logged in
                printf("Already logged in as %s. Use logout first.\n", uid);
            else if (sscanf(buffer, "%*s %s %s %s", arg1, arg2, extra) != 2) // %*s rules out the string (cmd)
                printf("Usage: login UID password\n"); // invalid number of arguments
            else if (login(arg1, arg2, peerport_s, fd, res) == 0) { // successful login
                strcpy(uid, arg1);
                strcpy(password, arg2);
            }
        }

        // UNREGISTER [unregister]
        else if (!strcmp(cmd, "unregister")) {
            if (strlen(uid) == 0) printf("User not logged in.\n"); // no user logged in
            else if (unregister(uid, password, fd, res) == 0) { // successful unregister
                strcpy(uid, ""); // local logout
                strcpy(password, "");
            }
        }

        // LOGOUT [logout]
        else if (!strcmp(cmd, "logout")) {
            if (strlen(uid) == 0) printf("User not logged in.\n");
            else if (logout(uid, password, fd, res) == 0) { // successful logout
                strcpy(uid, ""); // local logout
                strcpy(password, "");
            }
        }

        // PUBLISH [publish filename label]
        else if (!strcmp(cmd, "publish")) {
            if (sscanf(buffer, "%*s %s %s %s", arg1, arg2, extra) != 2)
                printf("Usage: publish filename label\n");
            else if (strlen(uid) == 0) printf("User not logged in.\n");
            else publish(uid, password, arg1, arg2, fd, res);
        }

        // REMOVE [remove filename]
        else if (!strcmp(cmd, "remove")) {
            if (sscanf(buffer, "%*s %s %s", arg1, extra) != 1)
                printf("Usage: remove filename\n");
            else if (strlen(uid) == 0) printf("User not logged in.\n");
            else remove_resource(uid, password, arg1, fd, res);
        }

        // LIST [list]
        else if (!strcmp(cmd, "list")) list(fd, res);

        // VERSIONS [versions filename]
        else if (!strcmp(cmd, "versions")) {
            if (sscanf(buffer, "%*s %s %s", arg1, extra) != 1)
                printf("Usage: versions filename\n");
            else
                versions(arg1, ip, port_s);
        }

        // EXIT [exit]
        else if (!strcmp(cmd, "exit")) {
            if (strlen(uid) != 0) printf("Please logout first.\n");
            else break;
        }

        // INVALID COMMAND
        else printf("No such command.\n");
    }

    // logout if user still logged in (Ctrl+C/Ctrl+D)
    if (strlen(uid) != 0) {
        printf("Logging out...\n");
        logout(uid, password, fd, res);
    }

    freeaddrinfo(res);
    close(fd);
    return 0;
}