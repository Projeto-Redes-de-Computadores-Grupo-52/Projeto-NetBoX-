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
#include "client/network/udp_client.h" // udp_open

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

// Everything the commands share
typedef struct {
    int fd;                         // UDP socket
    struct addrinfo *res;           // resolved DS address
    const char *ip, *port;          // DS address (also used for TCP)
    const char *peerport;           // -m
    char uid[UID_SIZE+1];           // empty = nobody logged in
    char password[PWD_SIZE+1];
} Ctx;

static int logged_in(const Ctx *c) { return c->uid[0] != '\0'; }

static void clear_session(Ctx *c) { c->uid[0] = c->password[0] = '\0'; }

// Each handler receives the full line; returns 1 only if the app must quit.
static int cmd_login(Ctx *c, const char *line) {
    char a1[BUF_SIZE+1], a2[BUF_SIZE+1], extra[BUF_SIZE+1];

    if (logged_in(c))
        printf("Already logged in as %s. Use logout first.\n", c->uid);
    else if (sscanf(line, "%*s %s %s %s", a1, a2, extra) != 2)
        printf("Usage: login UID password\n");
    else if (login(a1, a2, c->peerport, c->fd, c->res) == 0) {
        strcpy(c->uid, a1); // already validated
        strcpy(c->password, a2);
    }
    return 0;
}

static int cmd_logout(Ctx *c, const char *line) {
    (void)line;
    if (!logged_in(c)) printf("User not logged in.\n");
    else if (logout(c->uid, c->password, c->fd, c->res) == 0) clear_session(c);
    return 0;
}

static int cmd_unregister(Ctx *c, const char *line) {
    (void)line;
    if (!logged_in(c)) printf("User not logged in.\n");
    else if (unregister(c->uid, c->password, c->fd, c->res) == 0) clear_session(c);
    return 0;
}

static int cmd_publish(Ctx *c, const char *line) {
    char a1[BUF_SIZE+1], a2[BUF_SIZE+1], extra[BUF_SIZE+1];

    if (sscanf(line, "%*s %s %s %s", a1, a2, extra) != 2) printf("Usage: publish filename label\n");
    else if (!logged_in(c)) printf("User not logged in.\n");
    else publish(c->uid, c->password, a1, a2, c->fd, c->res);
    return 0;
}

static int cmd_remove(Ctx *c, const char *line) {
    char a1[BUF_SIZE+1], extra[BUF_SIZE+1];

    if (sscanf(line, "%*s %s %s", a1, extra) != 1) printf("Usage: remove filename\n");
    else if (!logged_in(c)) printf("User not logged in.\n");
    else remove_resource(c->uid, c->password, a1, c->fd, c->res);
    return 0;
}

static int cmd_list(Ctx *c, const char *line) {
    (void)line;
    list(c->fd, c->res);
    return 0;
}

static int cmd_versions(Ctx *c, const char *line) {
    char a1[BUF_SIZE+1], extra[BUF_SIZE+1];

    if (sscanf(line, "%*s %s %s", a1, extra) != 1) printf("Usage: versions filename\n");
    else versions(a1, c->ip, c->port);
    return 0;
}

static int cmd_exit(Ctx *c, const char *line) {
    (void)line;
    if (logged_in(c)) { printf("Please logout first.\n"); return 0; }
    return 1;   // quit
}

// Command table: exact name match (so "logoutxyz" is not "logout")
static const struct {
    const char *name; // command name
    int (*fn)(Ctx *, const char *); // pointer to command handler
} COMMANDS[] = {
    {"login", cmd_login},
    {"logout", cmd_logout},
    {"unregister", cmd_unregister},
    {"publish", cmd_publish},
    {"remove", cmd_remove},
    {"list", cmd_list},
    {"versions", cmd_versions},
    {"exit", cmd_exit},
};

// Returns 1 if the application must quit.
static int run_command(Ctx *c, const char *cmd, const char *line) {
    // sizeof(COMMANDS) / sizeof(COMMANDS[0]) = # commands
    for (size_t i = 0; i < sizeof(COMMANDS) / sizeof(COMMANDS[0]); i++)
        if (!strcmp(cmd, COMMANDS[i].name)) return COMMANDS[i].fn(c, line);

    printf("No such command.\n");
    return 0;
}

int main(int argc, char *argv[]) {

    int fd; // UDP socket
    struct addrinfo *res; // resolved DS address

    char buffer[BUF_SIZE+1]; // full line given by user
    char cmd[BUF_SIZE+1]; // command, the first word in the line

    const char *peerport_s = NULL; // (-m)
    const char *ip = DSIP_1; // (-n)
    const char *port_s = DSPORT; // (-p)

    // if the DS closes the TCP connection in the middle of a write(), treat the error as usual
    signal(SIGPIPE, SIG_IGN);

    // sigaction without SA_RESTART, so fgets is interrupted by Ctrl+C
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
    if (udp_open(ip, port_s, &fd, &res)) {
        printf("Error: udp_open()\n");
        return 1;
    }

    // structure with app state (socket, UID, password...)
    Ctx ctx = { .ip = ip, .port = port_s, .peerport = peerport_s };
    ctx.fd = fd;
    ctx.res = res;

    // main loop (commands)
    while (!got_sigint) {
        printf("> ");
        fflush(stdout);

        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            if (errno == EINTR) {
                clearerr(stdin);
                if (got_sigint) { printf("\n"); break; }
                continue;
            }
            break;                                  // Ctrl+D or read error
        }

        if (sscanf(buffer, "%s", cmd) != 1) continue;   // empty line
        if (run_command(&ctx, cmd, buffer)) break;      // exit
    }

    // logout if user still logged in (Ctrl+C/Ctrl+D)
    if (logged_in(&ctx)) {
        printf("Logging out...\n");
        logout(ctx.uid, ctx.password, ctx.fd, ctx.res);
    }

    printf("Closing application...\n");

    freeaddrinfo(res);
    close(fd);
    return 0;
}