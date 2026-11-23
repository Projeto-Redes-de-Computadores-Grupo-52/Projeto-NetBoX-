#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/stat.h>

#include "common/config.h"
#include "common/validation.h"
#include "client/network/udp_client.h"
#include "client/commands.h"
#include "client/network/tcp_client.h"

// this function returns a pointer to the reply status after the reply code
static const char *reply_status(const char *message, const char *code) {
    size_t n = strlen(code);
    // code doesn't match or doesn't have a space after, return NULL
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

    // LIN UID password peerTCPport
    snprintf(message, sizeof(message), "LIN %s %s %s\n", uid, password, peerport);

    if (udp_communicate(fd, message, res)) {
        printf("Error: udp_communicate()\n");
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

    // UNR UID password
    snprintf(message, sizeof(message), "UNR %s %s\n", uid, password);

    if (udp_communicate(fd, message, res)) {
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

    // LOU UID password
    snprintf(message, sizeof(message), "LOU %s %s\n", uid, password);

    if (udp_communicate(fd, message, res)) {
        printf("Error: udp_communicate()\n");
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

    if (udp_communicate(fd, message, res)) {
        printf("Error: udp_communicate()\n");
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

    if (udp_communicate(fd, message, res)) {
        printf("Error: udp_communicate()\n");
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

int publish(const char *uid, const char *password, const char *filename, const char *label, int fd, struct addrinfo *res) {

    char message[BUF_SIZE+1];
    const char *status;
    struct stat st; // file attributes

    // validation.c - check filename and label
    if (!check_filename(filename)) {
        printf("Invalid filename.\n");
        return 1;
    }
    if (!check_label(label)) {
        printf("Invalid label (1-20 characters: letters, digits, - and _).\n");
        return 1;
    }

    if (stat(filename, &st) == -1 || !S_ISREG(st.st_mode)) {
        printf("File '%s' not found in the local directory. May not be a regular file.\n", filename);
        return 1;
    }
    if (st.st_size > FSIZE_MAX) {
        printf("File too large (maximum %d bytes).\n", FSIZE_MAX);
        return 1;
    }

    // PUB UID password filename Fsize label
    snprintf(message, sizeof(message), "PUB %s %s %s %ld %s\n",
             uid, password, filename, (long)st.st_size, label);

    if (udp_communicate(fd, message, res)) {
        printf("Error: udp_communicate()\n");
        return 1;
    }

    /* RPB OK / RPB NOK / RPB NLG / RPB UNR / RPB WRP / RPB ERR */
    status = reply_status(message, "RPB");
    if (status == NULL) {
        printf("Unexpected reply from DS.\n");
        return 1;
    }

    if (!strcmp(status, "OK")) { printf("Resource published successfully.\n"); return 0; }
    if (!strcmp(status, "NOK")) printf("Unsuccessful publication.\n");
    else if (!strcmp(status, "NLG")) printf("User not logged in.\n");
    else if (!strcmp(status, "UNR")) printf("User not registered.\n");
    else if (!strcmp(status, "WRP")) printf("Wrong password.\n");
    else if (!strcmp(status, "ERR")) printf("Error.\n");
    else printf("Unexpected reply from DS.\n");

    return 1;
}

int versions(const char *filename, const char *ip, const char *port) {

    char message[BUF_SIZE+1];
    char reply[VERSIONS_BUF];
    const char *status;

    if (!check_filename(filename)) {
        printf("Invalid filename.\n");
        return 1;
    }

    // VRS filename
    snprintf(message, sizeof(message), "VRS %s\n", filename);

    if (tcp_request(ip, port, message, reply, sizeof(reply))) {
        printf("Error: could not get a reply from the DS (TCP).\n");
        return 1;
    }

    /* RVR NOK / RVR OK [UID Fsize label publication_time availability]* / RVR ERR */
    status = reply_status(reply, "RVR");
    if (status == NULL) {
        printf("Unexpected reply from DS.\n");
        return 1;
    }

    if (!strcmp(status, "NOK")) {
        printf("No peers publishing '%s'.\n", filename);
        return 0;
    }
    if (!strcmp(status, "ERR")) {
        printf("Error.\n");
        return 1;
    }
    if (strncmp(status, "OK ", 3) != 0) { // reply status is OK, but not in the right format
        printf("Unexpected reply from DS.\n");
        return 1;
    }

    char *file_info = strtok((char *)status + 3, " "); // file DS info, starting at publisher UID 

    // HEADER - %-8s alines the string to the right, minimum 8 characters
    printf("%-8s %-10s %-20s %-22s %s\n", "UID", "Size", "Label", "Published", "Status");

    while (file_info != NULL) {
        char *size  = strtok(NULL, " ");
        char *label = strtok(NULL, " ");
        char *avail = NULL, *t;
        char time[64] = "";

        // don't know how many tokens correspond to the publication_time, catch all
        while ((t = strtok(NULL, " ")) != NULL) {
            if (!strcmp(t, "AVL") || !strcmp(t, "NAV")) { avail = t; break; }
            if (strlen(time) + strlen(t) + 2 < sizeof(time)) { // check if the time tokens aren't too long
                if (time[0] != '\0') strcat(time, " "); // add time token to string time, ' ' as the separator
                strcat(time, t);
            }
        }

        if (size == NULL || label == NULL || avail == NULL) {
            printf("Malformed reply from DS.\n");
            return 1;
        }

        printf("%-8s %-10s %-20s %-22s %s\n", file_info, size, label, time,
               !strcmp(avail, "AVL") ? "Online" : "Offline");

        file_info = strtok(NULL, " "); // next file
    }

    return 0;
}