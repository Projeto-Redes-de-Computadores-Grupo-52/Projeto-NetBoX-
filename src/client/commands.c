#include <stdio.h>
#include <string.h>
#include <netdb.h>

#include "common/config.h"
#include "common/validation.h"
#include "client/network/udp_client.h"
#include "client/commands.h"

static const char *reply_status(const char *message, const char *code) {
    size_t n = strlen(code);

    if (strncmp(message, code, n) != 0 || message[n] != ' ') return NULL;
    return message + n + 1;
}

int login(const char *uid, const char *password, const char *peerport, int fd, struct addrinfo *res) {

    char message[BUF_SIZE+1];
    const char *status;

    if (!check_uid(uid)) {
        printf("Invalid UID.\n");
        return 1;
    }
    if (!check_pwd(password)) {
        printf("Invalid password.\n");
        return 1;
    }

    snprintf(message, sizeof(message), "LIN %s %s %s\n", uid, password, peerport);

    if (communicate(fd, message, res)) {
        printf("Error: communicate()\n");
        return 1;
    }

    /* RLI OK / RLI NOK / RLI REG / RLI ERR */
    status = reply_status(message, "RLI");
    if (status == NULL) {
        printf("Unexpected reply from DS.\n");
        return 1;
    }

    if (!strcmp(status, "OK")) { printf("Successful login.\n"); return 0; }
    if (!strcmp(status, "REG")) { printf("New user registered.\n"); return 0; }
    if (!strcmp(status, "NOK")) printf("Wrong password.\n");
    else if (!strcmp(status, "ERR")) printf("Error.\n");
    else printf("Unexpected reply from DS.\n");

    return 1;
}

int unregister(const char *uid, const char *password, int fd, struct addrinfo *res) {

    char message[BUF_SIZE+1];
    const char *status;

    snprintf(message, sizeof(message), "UNR %s %s\n", uid, password);

    if (communicate(fd, message, res)) {
        printf("Error: communicate()\n");
        return 1;
    }

    /* RUR OK / RUR NOK / RUR UNR / RUR WRP / RUR ERR */
    status = reply_status(message, "RUR");
    if (status == NULL) {
        printf("Unexpected reply from DS.\n");
        return 1;
    }

    if (!strcmp(status, "OK")) { printf("Successful unregister.\n"); return 0; }
    if (!strcmp(status, "NOK")) printf("User not logged in.\n");
    else if (!strcmp(status, "UNR")) printf("User not registered.\n");
    else if (!strcmp(status, "WRP")) printf("Wrong password.\n");
    else if (!strcmp(status, "ERR")) printf("Error.\n");
    else printf("Unexpected reply from DS.\n");

    return 1;
}

int logout(const char *uid, const char *password, int fd, struct addrinfo *res) {

    char message[BUF_SIZE+1];
    const char *status;

    snprintf(message, sizeof(message), "LOU %s %s\n", uid, password);

    if (communicate(fd, message, res)) {
        printf("Error: communicate()\n");
        return 1;
    }

    /* RLO OK / RLO NLG / RLO UNR / RLO WRP / RLO ERR */
    status = reply_status(message, "RLO");
    if (status == NULL) {
        printf("Unexpected reply from DS.\n");
        return 1;
    }

    if (!strcmp(status, "OK")) { printf("Successful logout.\n"); return 0; }
    if (!strcmp(status, "NLG")) printf("User not logged in.\n");
    else if (!strcmp(status, "UNR")) printf("User not registered.\n");
    else if (!strcmp(status, "WRP")) printf("Wrong password.\n");
    else if (!strcmp(status, "ERR")) printf("Error.\n");
    else printf("Unexpected reply from DS.\n");

    return 1;
}

int list(int fd, struct addrinfo *res) {

    char message[BUF_SIZE+1];
    const char *status;

    snprintf(message, sizeof(message), "LST\n");

    if (communicate(fd, message, res)) {
        printf("Error: communicate()\n");
        return 1;
    }

    /* RLS NOK / RLS OK filename filename ... / RLS ERR */
    status = reply_status(message, "RLS");
    if (status == NULL) {
        printf("Unexpected reply from DS.\n");
        return 1;
    }

    if (!strcmp(status, "NOK")) {
        printf("No published resources.\n");
        return 0;
    }
    if (!strncmp(status, "OK ", 3)) {
        
        char *names = (char *)status + 3;
        char *tok = strtok(names, " ");

        printf("Available resources:\n");
        while (tok != NULL) {
            printf("  %s\n", tok);
            tok = strtok(NULL, " ");
        }
    
        return 0;
    }
    if (!strcmp(status, "ERR")) printf("Error.\n");
    else printf("Unexpected reply from DS.\n");

    return 1;
}

int remove_resource(const char *uid, const char *password, const char *filename, int fd, struct addrinfo *res) {

    char message[BUF_SIZE+1];
    const char *status;

    if (!check_filename(filename)) {
        printf("Invalid filename.\n");
        return 1;
    }

    snprintf(message, sizeof(message), "REM %s %s %s\n", uid, password, filename);

    if (communicate(fd, message, res)) {
        printf("Error: communicate()\n");
        return 1;
    }

    /* RRM OK / RRM NOK / RRM NLG / RRM UNR / RRM WRP / RRM ERR */
    status = reply_status(message, "RRM");
    if (status == NULL) {
        printf("Unexpected reply from DS.\n");
        return 1;
    }

    if (!strcmp(status, "OK")) { printf("Resource removed successfully.\n"); return 0; }
    if (!strcmp(status, "NOK")) printf("Resource not found (not published by you).\n");
    else if (!strcmp(status, "NLG")) printf("User not logged in.\n");
    else if (!strcmp(status, "UNR")) printf("User not registered.\n");
    else if (!strcmp(status, "WRP")) printf("Wrong password.\n");
    else if (!strcmp(status, "ERR")) printf("Error.\n");
    else printf("Unexpected reply from DS.\n");

    return 1;
}