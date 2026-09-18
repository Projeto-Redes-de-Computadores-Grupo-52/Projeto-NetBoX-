#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFSIZE 128
#define COMSIZE 32
#define DSIP_1 "193.136.138.142"
#define DSIP_2 "192.168.1.1"
#define DSPORT "59000"
#define DSPORT_MIN 1
#define DSPORT_MAX 65535

int main(int argc, char *argv[]) {

    char buffer[BUFSIZE];
    char command[COMSIZE];

    int peerport;

    int fd, errcode;
    ssize_t n;
    socklen_t addrlen;
    struct addrinfo hints, *res;
    struct sockaddr_in addr;

    if (argc != 3) {
        printf("invalid number of arguments\n");
        exit(1);
    }

    if (strncmp(argv[1], "-m", 2) != 0) {
        printf("invalid arguments\n");
        exit(1);
    }

    peerport = atoi(argv[2]);

    if (peerport < DSPORT_MIN || peerport > DSPORT_MAX) {
        printf("invalid peer port\n");
        exit(1);
    }

    // SOCKET()
    fd = socket(AF_INET, SOCK_DGRAM, 0); // UDP socket
    if (fd == -1) exit(1); // error

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; //IPv4
    hints.ai_socktype = SOCK_DGRAM; //UDP socket

    errcode = getaddrinfo(DSIP_1, DSPORT, &hints, &res);
    if (errcode != 0) exit(1);

    while (1) {
        
        printf("> ");

        // read a command from the user using fgets()
        if (fgets(buffer, BUFSIZE, stdin) == NULL) break;

        // get the command from the buffer
        sscanf(buffer, "%s", command);

        // login
        if (strcmp(command, "login") == 0) {printf("LOGIN\n");}
        // unregister
        else if (strcmp(command, "unregister") == 0) {printf("UNREGISTER\n");}
        // logout
        else if (strcmp(command, "logout") == 0) {printf("LOGOUT\n");}
        // exit
        else if (strcmp(command, "exit") == 0) break;
        // unrecognized command
        else printf("%s: no such command\n", command);
    }

    return 0;
}