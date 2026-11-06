#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>

#include "common/config.h"
#include "client/commands.h"
#include "client/network/udp_client.h"

#define USAGE "Usage: ./user -m (peerport) [-n (ip)] [-p (port)]\n"

static int parse_port(const char *s) {
    char *end;
    long v = strtol(s, &end, 10);

    if (*s == '\0' || *end != '\0' || v < PORT_MIN || v > PORT_MAX) return 0;
    return (int)v;
}

int main(int argc, char *argv[]) {

    int fd;
    struct addrinfo *res;

    char buffer[BUF_SIZE+1];
    char cmd[BUF_SIZE+1], arg1[BUF_SIZE+1], arg2[BUF_SIZE+1], extra[BUF_SIZE+1];

    char uid[BUF_SIZE+1] = "";
    char password[BUF_SIZE+1] = "";

    const char *peerport_s = NULL;
    const char *ip = DSIP_1;
    const char *port_s = DSPORT;

    if (argc < 3 || argc % 2 == 0) {
        printf("Invalid number of arguments.\n" USAGE);
        return 1;
    }

    for (int i = 1; i < argc - 1; i += 2) {
        if (!strcmp(argv[i], "-m")) peerport_s = argv[i+1];
        else if (!strcmp(argv[i], "-n")) ip = argv[i+1];
        else if (!strcmp(argv[i], "-p")) port_s = argv[i+1];
        else {
            printf("%s: Unrecognized flag.\n" USAGE, argv[i]);
            return 1;
        }
    }

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

    if (init_connection(ip, port_s, &fd, &res)) {
        printf("Error: init_connection()\n");
        return 1;
    }

    while (1) {

        printf("> ");
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) break;

        if (sscanf(buffer, "%s", cmd) != 1) continue;

        if (!strcmp(cmd, "login")) {
            if (strlen(uid) != 0)
                printf("Already logged in as %s. Use logout first.\n", uid);
            else if (sscanf(buffer, "%*s %s %s %s", arg1, arg2, extra) != 2)
                printf("Usage: login UID password\n");
            else if (login(arg1, arg2, peerport_s, fd, res) == 0) {
                strcpy(uid, arg1);
                strcpy(password, arg2);
            }
        }

        else if (!strcmp(cmd, "unregister")) {
            if (strlen(uid) == 0) printf("User not logged in.\n");
            else if (unregister(uid, password, fd, res) == 0) {
                strcpy(uid, "");
                strcpy(password, "");
            }
        }

        else if (!strcmp(cmd, "logout")) {
            if (strlen(uid) == 0) printf("User not logged in.\n");
            else if (logout(uid, password, fd, res) == 0) {
                strcpy(uid, "");
                strcpy(password, "");
            }
        }

        else if (!strcmp(cmd, "publish")) {
            if (sscanf(buffer, "%*s %s %s %s", arg1, arg2, extra) != 2)
                printf("Usage: publish filename label\n");
            else if (strlen(uid) == 0)
                printf("User not logged in.\n");
            else
                publish(uid, password, arg1, arg2, fd, res);
        }

        else if (!strcmp(cmd, "remove")) {
            if (sscanf(buffer, "%*s %s %s", arg1, extra) != 1)
                printf("Usage: remove filename\n");
            else if (strlen(uid) == 0)
                printf("User not logged in.\n");
            else
                remove_resource(uid, password, arg1, fd, res);
        }

        else if (!strcmp(cmd, "list")) {
            list(fd, res);
        }

        else if (!strcmp(cmd, "versions")) {
            if (sscanf(buffer, "%*s %s %s", arg1, extra) != 1)
                printf("Usage: versions filename\n");
            else
                versions(arg1, ip, port_s);
        }

        else if (!strcmp(cmd, "exit")) {
            if (strlen(uid) != 0) printf("Please logout first.\n");
            else break;
        }

        else printf("No such command.\n");
    }

    freeaddrinfo(res);
    close(fd);
    return 0;
}