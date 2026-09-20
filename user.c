#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUF_SIZE 128

#define DSIP_1 "193.136.138.142"
#define DSIP_2 "192.168.1.1"

#define DSPORT "59000"
#define DSPORT_MIN 1
#define DSPORT_MAX 65535

int main(int argc, char *argv[]) {

    char buffer[BUF_SIZE];
    char command[BUF_SIZE];

    int peerport;
    char *dsip = DSIP_1; // default set to public ip
    char *dsport = DSPORT;

    int fd, errcode;
    ssize_t n;
    socklen_t addrlen;
    struct addrinfo hints, *res;
    struct sockaddr_in addr;

    char message[BUF_SIZE];

    char uid[7];
    char password[9];
    
    int k;

    // assumes the commands are all valid, only checks the peer port value !!! (change)
    peerport = atoi(argv[2]);

    if (peerport < DSPORT_MIN || peerport > DSPORT_MAX) {
        printf("invalid peer port\n");
        exit(1);
    }

    if (argc == 5) {

        if (!strcmp(argv[3],"-n")) dsip = argv[4];

        else dsport = argv[4];
    }

    if (argc == 7) {

        if (!strcmp(argv[3],"-n")) {
            dsip = argv[4];
            dsport = argv[6];
        }
        else {
            dsip = argv[6];
            dsport = argv[4];
        }
    }

    // SOCKET()
    fd = socket(AF_INET, SOCK_DGRAM, 0); // UDP socket
    if (fd == -1) exit(1); // error

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; //IPv4
    hints.ai_socktype = SOCK_DGRAM; //UDP socket

    errcode = getaddrinfo(dsip, dsport, &hints, &res);
    if (errcode != 0) exit(1);

    while (1) {
        
        printf("> ");

        // read a command from the user using fgets()
        if (fgets(buffer, BUF_SIZE, stdin) == NULL) break;

        // get the command from the buffer
        sscanf(buffer, "%s", command);

        // login
        if (!strcmp(command, "login")) {
            sscanf(buffer, "%*s%s%s", uid, password); // %*s ignores the first string (command)

            if (strlen(uid) != 6) {
                printf("invalid user id\n");
                continue;   
            }

            if (strlen(password) != 8) {
                printf("invalid password\n");
                continue;
            }

            snprintf(message, BUF_SIZE, "LIN %s %s %d\n", uid, password, peerport);
            //printf("sending: %s\n", message);

            k = sendto(fd, message, strlen(message), 0, res->ai_addr, res->ai_addrlen);
            if (k == -1) exit(1);

            addrlen = sizeof(addr);
            k = recvfrom(fd, message, BUF_SIZE, 0, (struct sockaddr*) &addr, &addrlen);
            if (k == 1) /*error*/ exit(1);

            message[k-1] = '\0';
            //printf("received: %s\n", message);

            // RLI OK / RLI NOK / RLI REG
            if (!strcmp(&message[4],"OK")) printf("sucessful login\n");
            else if (!strcmp(&message[4],"NOK")) printf("wrong password\n");
            else if (!strcmp(&message[4],"REG")) printf("new user registered\n");
            else if (!strcmp(&message[4],"ERR")) printf("error");
        }
        // unregister
        else if (!strcmp(command, "unregister")) {
            
            snprintf(message, BUF_SIZE, "UNR %s %s\n", uid, password);

            k = sendto(fd, message, strlen(message), 0, res->ai_addr, res->ai_addrlen);
            if (k == -1) exit(1);

            addrlen = sizeof(addr);
            k = recvfrom(fd, message, BUF_SIZE, 0, (struct sockaddr*) &addr, &addrlen);
            if (k == 1) /*error*/ exit(1);

            message[k-1] = '\0';
            //printf("received: %s\n", message);

            // RUR OK / RUR NOK/ RUR UNR / RUR WRP
            if (!strcmp(&message[4],"OK")) printf("sucessful unregister\n");
            else if (!strcmp(&message[4],"NOK")) printf("user not logged in\n");
            else if (!strcmp(&message[4],"UNR")) printf("user not registered\n");
            else if (!strcmp(&message[4],"WRP")) printf("wrong password\n");
            else if (!strcmp(&message[4],"ERR")) printf("error");
        }
        // logout
        else if (!strcmp(command, "logout")) {

            snprintf(message, BUF_SIZE, "LOU %s %s\n", uid, password);
            //printf("sending: %s\n", message);

            k = sendto(fd, message, strlen(message), 0, res->ai_addr, res->ai_addrlen);
            if (k == -1) exit(1);

            addrlen = sizeof(addr);
            k = recvfrom(fd, message, BUF_SIZE, 0, (struct sockaddr*) &addr, &addrlen);
            if (k == 1) /*error*/ exit(1);

            message[k-1] = '\0';
            //printf("received: %s\n", message);

            // RLO OK / RLO NLG/ RLO UNR / RLO WRP
            if (!strcmp(&message[4],"OK")) printf("sucessful logout\n");
            else if (!strcmp(&message[4],"NLG")) printf("user not logged in\n");
            else if (!strcmp(&message[4],"UNR")) printf("user not registered\n");
            else if (!strcmp(&message[4],"WRP")) printf("wrong password\n");
            else if (!strcmp(&message[4],"ERR")) printf("error");
        }
        // exit
        else if (!strcmp(command, "exit")) {
            freeaddrinfo(res);
            close(fd);
            break;
        }
        // unrecognized command
        else printf("%s: no such command\n", command);
    }

    return 0;
}