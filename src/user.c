#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUF_SIZE 4096

#define DSIP_1 "193.136.138.142"
#define DSIP_2 "192.168.1.1"

#define DSPORT "59000"
#define PORT_MIN 1
#define PORT_MAX 65535

#define UID_SIZE 6
#define PWD_SIZE 8

#define FILENAME_MAX_LEN 24

int check_filename(char *filename) {

    size_t len = strlen(filename);

    // check filename length
    if (len < 5 || len > FILENAME_MAX_LEN)
        return 0;
    
    // check for .type
    if (filename[len-4] != '.')
        return 0;

    // check filename
    for (size_t i = 0; i < len-4; i++)
        if (!isalnum((unsigned char)filename[i]) && filename[i] != '-' && filename[i] != '_')
            return 0;

    // check .type
    for (size_t i = len-3; i < len; i++)
        if (!isalnum((unsigned char)filename[i]))
            return 0;

    return 1;
}

int init_connection(char *ip, char *port, int *fd, struct addrinfo **res) { // we use fd and res from the main since we'll need them for every command

    struct addrinfo hints;

    *fd = socket(AF_INET, SOCK_DGRAM, 0); // UDP socket
    if (*fd == -1) return 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; //IPv4
    hints.ai_socktype = SOCK_DGRAM; //UDP socket

    if (getaddrinfo(ip, port, &hints, res) != 0) return 1;

    return 0;
}

int check_uid(char *uid, int size) {
    
    if (size != UID_SIZE) return 0;

    for (int i = 0; i < size; i++)
        if (uid[i] < '0' || uid[i] > '9') return 0;

    return 1;
}

int check_pwd(char *password, int size) {

    if (size != PWD_SIZE) return 0;

    for (int i = 0; i < size; i++)
        if ((password[i] < '0' || password[i] > '9')
            && (password[i] < 'A' || password[i] > 'Z')
            && (password[i] < 'a' || password[i] > 'z'))
            
            return 0;

    return 1;
}

int communicate(int fd, char *message, struct addrinfo *res) {

    ssize_t n;
    socklen_t addrlen;
    struct sockaddr_in addr;

    n = sendto(fd,message,strlen(message),0,res->ai_addr,res->ai_addrlen);
    if (n == -1) return 1;

    addrlen = sizeof(addr);
    n = recvfrom(fd,message,BUF_SIZE,0,(struct sockaddr*)&addr,&addrlen);
    if (n == -1) return 1;

    // removes '\n' from the answer
    if (n > 0) message[n-1] = '\0'; // just in case we get an empty datagram back
    else message[n] = '\0'; 

    return 0;
}

int login(char *uid, char* password, char *peerport, int fd, struct addrinfo *res) {

    char message[BUF_SIZE+1];

    if (!check_uid(uid, strlen(uid))) {
        printf("Invalid UID.\n");
        return 1;
    }

    if (!check_pwd(password,strlen(password))) {
        printf("Invalid password.\n");
        return 1;
    }
      
    sprintf(message,"LIN %s %s %s\n",uid,password,peerport);

    if (communicate(fd,message,res)) {
        printf("Error: comunicate()");
        return 1;
    }

    // RLI OK / RLI NOK / RLI REG
    if (!strcmp(message+4,"OK")) printf("Sucessful login.\n");
    else if (!strcmp(message+4,"NOK")) printf("Wrong password.\n");
    else if (!strcmp(message+4,"REG")) printf("New user registered.\n");
    else if (!strcmp(message+4,"ERR")) {
        printf("Error.\n"); 
        return 1;
    }
    
    return 0;
}

int unregister(char *uid, char* password, int fd, struct addrinfo *res) {

    char message[BUF_SIZE+1];

    sprintf(message,"UNR %s %s\n", uid, password);

    if (communicate(fd,message,res)) {
        printf("Error: comunicate()");
        return 1;
    }

    // RUR OK / RUR NOK/ RUR UNR / RUR WRP
    if (!strcmp(message+4,"OK")) printf("Sucessful unregister.\n");
    else if (!strcmp(message+4,"NOK")) printf("User not logged in.\n");
    else if (!strcmp(message+4,"UNR")) printf("User not registered.\n");
    else if (!strcmp(message+4,"WRP")) printf("Wrong password.\n");
    else if (!strcmp(message+4,"ERR")) printf("Error.\n");
    
    return 0;
}

int logout(char *uid, char* password, int fd, struct addrinfo *res) {

    char message[BUF_SIZE+1];

    sprintf(message,"LOU %s %s\n", uid, password);

    if (communicate(fd,message,res)) {
        printf("Error: comunicate()");
        return 1;
    }

    // RLO OK / RLO NLG / RLO UNR / RLO WRP
    if (!strcmp(message+4,"OK")) printf("Sucessful logout.\n");
    else if (!strcmp(message+4,"NLG")) printf("User not logged in.\n");
    else if (!strcmp(message+4,"UNR")) printf("User not registered.\n");
    else if (!strcmp(message+4,"WRP")) printf("Wrong password.\n");
    else if (!strcmp(message+4,"ERR")) printf("Error.\n");
    
    return 0;
}

int list(int fd, struct addrinfo *res) {

    char message[BUF_SIZE+1];

    sprintf(message,"LST\n");

    if (communicate(fd,message,res)) {
        printf("Error: comunicate()");
        return 1;
    } 

    // RLS NOK / RLS OK
    if (!strcmp(message+4,"NOK")) printf("No published resources.\n");
    // mostrar os ficheiros linha a linha
    else if (!strncmp(message+4,"OK", 2)) fprintf(stdout, "%s\n", message+7); // evitar printf / scanf

    return 0;
}

int remove_resource(char *uid, char *password, char *filename, int fd, struct addrinfo *res) {

    char message[BUF_SIZE+1];

    if (strlen(uid) == 0) {
        printf("User not logged in.\n");
        return 1;
    }

    if (!check_filename(filename)) {
        printf("Invalid filename.\n");
        return 1;
    }

    sprintf(message, "REM %s %s %s\n", uid, password, filename);

    if (communicate(fd, message, res)) {
        printf("Error: communicate().\n");
        return 1;
    }

    // RRM OK / RRM NOK / RRM NLG / RRM UNR / RRM WRP / RRM ERR
    if (strncmp(message,"RRM ",4)) printf("Unexpected reply from DS.\n");
    else if (!strcmp(message+4,"OK")) printf("Resource removed successfully.\n");
    else if (!strcmp(message+4,"NOK")) printf("Resource not found (not published by you).\n");
    else if (!strcmp(message+4,"NLG")) printf("User not logged in.\n");
    else if (!strcmp(message+4,"UNR")) printf("User not registered.\n");
    else if (!strcmp(message+4,"WRP")) printf("Wrong password.\n");
    else if (!strcmp(message+4,"ERR")) printf("Error.\n");
    else printf("Unexpected reply from DS.\n");

    return 0;
}

int main(int argc, char *argv[]) {

    int fd;
    struct addrinfo *res;

    char buffer[BUF_SIZE+1]; // input from the user
    char uid[BUF_SIZE+1] = "";
    char password[BUF_SIZE+1] = "";

    char *peerport_s;
    char *ip = DSIP_1;
    char *port_s = DSPORT;

    int m_flag = 0; // check for -m
    int i = 1;
    
    // only an odd number of arguments >=3 is valid
    if (argc < 3 || argc % 2 == 0) {
        printf("Invalid number of arguments.\n");
        return 1;
    }

    // check two-by-two for flag and argument: i and i+1
    while (i < argc - 1) {
        if (!strcmp(argv[i],"-m")) {
            m_flag = 1; // -m flag present
            peerport_s = argv[i+1];
        }
        else if (!strcmp(argv[i],"-n")) ip = argv[i+1];
        else if (!strcmp(argv[i],"-p")) port_s = argv[i+1];
        else {
            printf("%s: Unrecognized flag.\nUsage: ./user -m (peerport) [-n (ip)] [-p (port)]",argv[i]);
            return 1;
        }
        
        i+=2;
    }

    // check for -m
    if (!m_flag) {
        printf("No -m flag.\nUsage: ./user -m (peerport) [-n (ip)] [-p (port)]");
        return 1;   
    }

    // check if peerport is inside the limits
    int peerport_i = atoi(peerport_s); // not really safe, should be changed (allows for "12345abc" for example, check out strtol())
    if (peerport_i < PORT_MIN || peerport_i > PORT_MAX) {
        printf("%d: Invalid port (1-65535)\n",peerport_i);
        return 1;
    }

    // check if port is inside the limits
    int port_i = atoi(port_s); // not really safe, should be changed
    if (port_i < PORT_MIN || port_i > PORT_MAX) {
        printf("%d: Invalid port (1-65535)\n",port_i);
        return 1;
    }

    // start the UDP connection
    if (init_connection(ip,port_s,&fd,&res)) {
        printf("Error: init_connection()\n");
        return 1;
    }

    // menu
    while (1) {
        
        printf("> ");

        // read a command from the user
        if (fgets(buffer, BUF_SIZE, stdin) == NULL) return 1;

        // login
        if (!strncmp(buffer,"login ",6)) {

            char new_uid[BUF_SIZE+1], new_pwd[BUF_SIZE+1], extra[BUF_SIZE+1];
            
            if (strlen(uid) != 0)
                printf("Already logged in as %s. Use logout first.\n", uid);
            else if (sscanf(buffer+6,"%s %s %s",new_uid,new_pwd,extra) != 2)
                printf("Usage: login UID password\n");
            else if (login(new_uid,new_pwd,peerport_s,fd,res) == 0) {
                strcpy(uid,new_uid);        // only save uid and pwd after successful login
                strcpy(password,new_pwd);
            }
        }

        // unregister
        else if (!strncmp(buffer, "unregister", 10)) {
            if (strlen(uid) == 0)
                printf("User not logged in.\n");
            else if (unregister(uid,password,fd,res) == 0) {
                strcpy(uid,"");
                strcpy(password,"");
            }
        }
        
        // logout
        else if (!strncmp(buffer, "logout", 6)) {
            if (strlen(uid) == 0)
                printf("User not logged in.\n");
            else if (logout(uid,password,fd,res) == 0) {
                strcpy(uid,"");
                strcpy(password,"");
            }
        }
        
        // publish
        else if (!strncmp(buffer, "publish", 7)) {
        }

        // remove
        else if (!strncmp(buffer, "remove", 6)) {

            char filename[BUF_SIZE+1], extra[BUF_SIZE+1];

            if (sscanf(buffer+7, "%s %s", filename, extra) != 1)
                printf("Usage: remove filename\n");
            else
                remove_resource(uid, password, filename, fd, res);
        }

        // versions
        else if (!strncmp(buffer, "versions", 8)) {
        }

        // list
        else if (!strncmp(buffer, "list", 4)) {
            list(fd,res);
        }
        
        // exit
        else if (!strncmp(buffer, "exit", 4)) {
            
            if (strlen(uid) != 0) {
                printf("Please logout first.\n");
                continue;
            }
            
            freeaddrinfo(res);
            close(fd);
            break;
        }
        // unrecognized command
        else printf("No such command.\n");
    }

    return 0;
}