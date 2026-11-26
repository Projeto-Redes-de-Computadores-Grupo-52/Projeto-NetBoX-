/* main.c
 * Entry point of the user application.
 * Responsibilities:
 *   1. read and validate the command line arguments (-m, -n, -p)
 *   2. open the UDP socket used to talk to the DS
 *   3. keep the current session (logged-in UID and its password)
 *   4. read commands from the keyboard and call the matching function
 *   5. log out automatically on Ctrl+C / Ctrl+D
 */

#include <stdio.h>
#include <stdlib.h>     // strtol
#include <string.h>     // strcmp, strcpy, strlen, memset
#include <unistd.h>     // close, getopt, optarg
#include <signal.h>     // signal, sigaction, sig_atomic_t, SIGPIPE, SIGINT
#include <netdb.h>      // struct addrinfo, freeaddrinfo
#include <errno.h>      // errno, EINTR
#include <arpa/inet.h>  // INET_ADDRSTRLEN

#include "common/config.h"              // BUF_SIZE, DSIP_1, DSPORT, PORT_MIN/MAX
#include "client/commands.h"            // login, logout, publish, ...
#include "client/network/udp_client.h"  // udp_open

// help message for user application start
#define USAGE "Usage: ./user -m (peerport) [-n (ip)] [-p (port)]\n"

// set to 1 by SIGINT handler and checked in the main loop
// volatile sig_atomic_t is the only type safe to write from a handler
static volatile sig_atomic_t got_sigint = 0;

// does the minimum possible: printf/sendto are not async-signal-safe,
// so the real work (logout) is done later, in the main loop
static void handle_sigint(int sig) {
    (void)sig; // unused parameter
    got_sigint = 1; // minimum possible: no printf/sendto allowed here
}

// converts a string into a port number between 1-65535 or 0 if invalid
// use strtol instead of atoi so "123abc" isn't valid, for example
static int parse_port(const char *s) {
    char *end; // first character that was not converted
    long v = strtol(s, &end, 10);

    if (*s == '\0' || *end != '\0' || v < PORT_MIN || v > PORT_MAX) return 0;
    return (int)v;
}

// Everything the commands share: Connection data and current session
typedef struct {
    int fd;                         // UDP socket
    struct addrinfo *res;           // resolved DS address
    const char *ip, *port;          // DS address (also used for TCP)
    const char *peerport;           // -m
    char uid[UID_SIZE+1];           // empty = nobody logged in
    char password[PWD_SIZE+1];
} Ctx; // context

// the session is "logged in" when the uid is not empty.
static int logged_in(const Ctx *c) { return c->uid[0] != '\0'; }

static void clear_session(Ctx *c) { c->uid[0] = c->password[0] = '\0'; }

// COMMAND HANDLERS
// each one receives the full line typed by the user and returns 1 only if
// the application must quit. In every handler, "%*s" reads and discards the
// command name, and the "extra" variable detects too many arguments (the
// sscanf return value would be higher than expected).

static int cmd_login(Ctx *c, const char *line) {
    char a1[BUF_SIZE+1], a2[BUF_SIZE+1], extra[BUF_SIZE+1];

    if (logged_in(c))
        printf("Already logged in as %s. Use logout first.\n", c->uid);
    else if (sscanf(line, "%*s %s %s %s", a1, a2, extra) != 2)
        printf("Usage: login UID password\n");
    else if (login(a1, a2, c->peerport, c->fd, c->res) == 0) {
        // the session is only stored if the DS accepted the login;
        // strcpy is safe, login() validated the sizes
        strcpy(c->uid, a1);
        strcpy(c->password, a2);
    }
    return 0;
}

static int cmd_logout(Ctx *c, const char *line) {
    (void)line;
    if (!logged_in(c)) printf("User not logged in.\n");
    // only clear the session if the DS confirmed, so the local state never
    // diverges from the DS (e.g. after a wrong password)
    else if (logout(c->uid, c->password, c->fd, c->res) == 0) clear_session(c);
    return 0;
}

static int cmd_unregister(Ctx *c, const char *line) {
    (void)line;
    // the local check is needed: without a session there is no
    // UID/password to fill in the request
    if (!logged_in(c)) printf("User not logged in.\n");
    else if (unregister(c->uid, c->password, c->fd, c->res) == 0) clear_session(c);
    return 0;
}

static int cmd_publish(Ctx *c, const char *line) {
    char a1[BUF_SIZE+1], a2[BUF_SIZE+1], extra[BUF_SIZE+1];

     // order: arguments -> login -> (inside publish) file checks
    if (sscanf(line, "%*s %s %s %s", a1, a2, extra) != 2) printf("Usage: publish filename label\n");
    else if (!logged_in(c)) printf("User not logged in.\n");
    else publish(c->uid, c->password, a1, a2, c->fd, c->res);
    // return value ignored: the session does not change and the error was already shown to the user
    return 0;
}

static int cmd_remove(Ctx *c, const char *line) {
    char a1[BUF_SIZE+1], extra[BUF_SIZE+1];

    if (sscanf(line, "%*s %s %s", a1, extra) != 1) printf("Usage: remove filename\n");
    else if (!logged_in(c)) printf("User not logged in.\n");
    else remove_resource(c->uid, c->password, a1, c->fd, c->res);
    return 0;
}

// no login needed
static int cmd_list(Ctx *c, const char *line) {
    (void)line;
    list(c->fd, c->res);
    return 0;
}

// no login needed; uses TCP, so it needs the DS IP and port
static int cmd_versions(Ctx *c, const char *line) {
    char a1[BUF_SIZE+1], extra[BUF_SIZE+1];

    if (sscanf(line, "%*s %s %s", a1, extra) != 1) printf("Usage: versions filename\n");
    else versions(a1, c->ip, c->port);
    return 0;
}

// local command, does not talk to the DS
static int cmd_exit(Ctx *c, const char *line) {
    (void)line;
    if (logged_in(c)) { printf("Please logout first.\n"); return 0; }
    return 1; // quit
}

// TEMPORARY (testing only), "where" is not a keyboard command
// used internally by "get". Remove with the table entry
static int cmd_where(Ctx *c, const char *line) {
    char a1[BUF_SIZE+1], a2[BUF_SIZE+1], extra[BUF_SIZE+1];
    char ip[INET_ADDRSTRLEN], port[6];

    if (sscanf(line, "%*s %s %s %s", a1, a2, extra) != 2)
        printf("Usage: where filename UID\n");
    else if (where(a1, a2, c->fd, c->res, ip, sizeof(ip), port, sizeof(port)) == 0)
        printf("Peer at %s:%s\n", ip, port);
    return 0;
}

// command table: exact name match (so "logoutxyz" is not "logout")
// adding a command = one handler + one line here
static const struct {
    const char *name;               // command name
    int (*fn)(Ctx *, const char *); // pointer to command handler
} COMMANDS[] = {
    {"login", cmd_login},
    {"logout", cmd_logout},
    {"unregister", cmd_unregister},
    {"publish", cmd_publish},
    {"remove", cmd_remove},
    {"list", cmd_list},
    {"versions", cmd_versions},
    {"where", cmd_where}, // TEMPORARY - remove before the final version
    {"exit", cmd_exit},
};

// finds the handler for "cmd" and runs it, returns 1 if the application must quit
static int run_command(Ctx *c, const char *cmd, const char *line) {
    // sizeof(COMMANDS) / sizeof(COMMANDS[0]) = # commands
    for (size_t i = 0; i < sizeof(COMMANDS) / sizeof(COMMANDS[0]); i++)
        if (!strcmp(cmd, COMMANDS[i].name)) return COMMANDS[i].fn(c, line);

    printf("No such command.\n");
    return 0;
}

// parses the arguments, opens the UDP socket and runs the command loop
int main(int argc, char *argv[]) {

    int fd;                 // UDP socket
    struct addrinfo *res;   // resolved DS address

    char buffer[BUF_SIZE+1];    // full line given by user
    char cmd[BUF_SIZE+1];       // command, the first word in the line

    const char *peerport_s = NULL;  // (-m)
    const char *ip = DSIP_1;        // (-n)
    const char *port_s = DSPORT;    // (-p)

    // writing to a TCP connection closed by the DS would kill the process
    // with SIGPIPE, ignoring it makes write() return -1 instead
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

    // COMMAND LINE ARGUMENTS
    // getopt handles "-m value" pairs; "m:n:p:" means each flag takes a value
    int opt;
    while ((opt = getopt(argc, argv, "m:n:p:")) != -1) {
        switch (opt) {
            case 'm': peerport_s = optarg; break;
            case 'n': ip = optarg; break;
            case 'p': port_s = optarg; break;
            default: // unknown flag or missing value (getopt already printed why)
                printf(USAGE);
                return 1;
        }
    }

    // getopt only splits flags from values, it does not validate them
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

    // the IP is not validated here, getaddrinfo fails cleanly if it is invalid
    if (udp_open(ip, port_s, &fd, &res)) {
        printf("Error: udp_open()\n");
        return 1;
    }

    // structure with app state (socket, UID, password...)
    // fd and res only exist after udp_open, so the context is built here
    Ctx ctx = { .ip = ip, .port = port_s, .peerport = peerport_s };
    ctx.fd = fd;
    ctx.res = res;

    // COMMAND LOOP (MENU)

    // got_sigint is also checked here, a Ctrl+C that arrived while a command
    // was running does not interrupt fgets, so it is caught on the next turn
    while (!got_sigint) {
        printf("> ");
        fflush(stdout); // the prompt has no '\n', so it must be flushed

        if (fgets(buffer, sizeof(buffer), stdin) == NULL) { // read line
            if (errno == EINTR) {   // interrupted by a signal, not EOF
                clearerr(stdin);    // otherwise the next fgets fails immediately 
                if (got_sigint) { printf("\n"); break; }
                continue;           // some other signal: just ask again
            }
            break; // Ctrl+D (EOF) or read error
        }

        // read command
        if (sscanf(buffer, "%s", cmd) != 1) continue;   // empty line
        if (run_command(&ctx, cmd, buffer)) break;      // exit
    }

    // CLEANUP

    // leaving with a session open (Ctrl+C / Ctrl+D) would keep the user
    // logged in at the DS, so log out first.
    if (logged_in(&ctx)) {
        printf("Logging out...\n");
        logout(ctx.uid, ctx.password, ctx.fd, ctx.res);
    }

    printf("Closing application...\n");

    freeaddrinfo(res); // allocated by getaddrinfo in udp_open
    close(fd);
    return 0;
}