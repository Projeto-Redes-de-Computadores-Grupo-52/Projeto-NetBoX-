/* commands.c
 * User commands: build the protocol message, exchange it with the DS,
 * interpret the status of the reply and tell the user what happened.
 *
 * All commands follow the same pattern:
 *   1. validate the arguments locally (nothing is sent if they are invalid)
 *   2. build the request
 *   3. send it and receive the reply (UDP, or TCP for "versions")
 *   4. check the reply code and map each status to a message for the user
 */

#include <stdio.h>
#include <string.h>     // strcmp, strncmp, strlen
#include <netdb.h>      // struct addrinfo
#include <sys/stat.h>   // stat, S_ISREG

#include "client/commands.h"
#include "common/config.h"              // BUF_SIZE, VERSIONS_BUF, FSIZE_MAX
#include "common/validation.h"          // check_uid, check_pwd, check_filename, check_label
#include "client/network/udp_client.h"  // udp_communicate            
#include "client/network/tcp_client.h"  // tcp_request
#include "client/display.h"             // print_resources, print_versions

// this function returns a pointer to the reply status after the reply code
static const char *reply_status(const char *reply, const char *code) {
    size_t n = strlen(code);
    // code doesn't match or doesn't have a space after, return NULL
    if (strncmp(reply, code, n) != 0 || reply[n] != ' ') return NULL;
    return reply+n+1;
}

// sends a request and reads the reply, prints the error, returns 0 on success
static int ds_exchange(int fd, const char *request, char *reply, size_t size,
                       struct addrinfo *res) {
    
    int r = udp_communicate(fd, request, reply, size, res);

    if (r == 2) printf("No reply from DS (timeout).\n");
    else if (r != 0) printf("Error: communication with the DS failed.\n");
    return r;
}

int login(const char *uid, const char *password, const char *peerport, int fd, struct addrinfo *res) {

    char request[BUF_SIZE+1];
    char reply[BUF_SIZE+1];
    const char *status;

    // local validation, the DS would answer ERR, but this saves a round trip
    if (!check_uid(uid)) {
        printf("Invalid UID.\n");
        return 1;
    }
    if (!check_pwd(password)) {
        printf("Invalid password.\n");
        return 1;
    }

    // LIN UID password peerTCPport
    snprintf(request, sizeof(request), "LIN %s %s %s\n", uid, password, peerport);

    if (ds_exchange(fd, request, reply, sizeof(reply), res)) return 1;

    /* RLI OK / RLI NOK / RLI REG / RLI ERR */
    status = reply_status(reply, "RLI");
    if (status == NULL) {
        printf("Unexpected reply from DS.\n");
        return 1;
    }

    // success cases return 0 right away, main only stores the session on 0
    if (!strcmp(status, "OK")) { printf("Successful login.\n"); return 0; }
    if (!strcmp(status, "REG")) { printf("New user registered.\n"); return 0; }
    
    if (!strcmp(status, "NOK")) printf("Wrong password.\n");
    else if (!strcmp(status, "ERR")) printf("Error.\n");
    else printf("Unexpected reply from DS.\n");
    return 1;
}

int unregister(const char *uid, const char *password, int fd, struct addrinfo *res) {

    char request[BUF_SIZE+1];
    char reply[BUF_SIZE+1];
    const char *status;

    // UNR UID password
    snprintf(request, sizeof(request), "UNR %s %s\n", uid, password);

    if (ds_exchange(fd, request, reply, sizeof(reply), res)) return 1;

    /* RUR OK / RUR NOK / RUR UNR / RUR WRP / RUR ERR */
    status = reply_status(reply, "RUR");
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

    char request[BUF_SIZE+1];
    char reply[BUF_SIZE+1];
    const char *status;

    // LOU UID password
    snprintf(request, sizeof(request), "LOU %s %s\n", uid, password);

    if (ds_exchange(fd, request, reply, sizeof(reply), res)) return 1;
    
    /* RLO OK / RLO NLG / RLO UNR / RLO WRP / RLO ERR */
    status = reply_status(reply, "RLO");
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

    char request[BUF_SIZE+1];
    char reply[BUF_SIZE+1];
    const char *status;

    snprintf(request, sizeof(request), "LST\n");

    if (ds_exchange(fd, request, reply, sizeof(reply), res)) return 1;

    /* RLS NOK / RLS OK filename filename ... / RLS ERR */
    status = reply_status(reply, "RLS");
    if (status == NULL) {
        printf("Unexpected reply from DS.\n");
        return 1;
    }

    // "no resources" is a normal answer, not an error
    if (!strcmp(status, "NOK")) {
        printf("No published resources.\n");
        return 0;
    }
    // "OK " with the space, the protocol says OK always comes with a list
    if (!strncmp(status, "OK ", 3)) {
        // the cast drops const because strtok (inside print_resources) writes
        // into the buffer, it is safe since "reply" is our own local array
        print_resources((char *)status+3);
        return 0;
    }

    if (!strcmp(status, "ERR")) printf("Error.\n");
    else printf("Unexpected reply from DS.\n");
    return 1;
}

int remove_resource(const char *uid, const char *password, const char *filename, int fd, struct addrinfo *res) {

    char request[BUF_SIZE+1];
    char reply[BUF_SIZE+1];
    const char *status;

    // the filename is not checked against the local directory here,
    // removing only concerns what was published, not what is on disk
    if (!check_filename(filename)) {
        printf("Invalid filename.\n");
        return 1;
    }

    snprintf(request, sizeof(request), "REM %s %s %s\n", uid, password, filename);

    if (ds_exchange(fd, request, reply, sizeof(reply), res)) return 1;

    /* RRM OK / RRM NOK / RRM NLG / RRM UNR / RRM WRP / RRM ERR */
    status = reply_status(reply, "RRM");
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

    char request[BUF_SIZE+1];
    char reply[BUF_SIZE+1];
    const char *status;
    struct stat st; // file attributes (need the size)

    if (!check_filename(filename)) {
        printf("Invalid filename.\n");
        return 1;
    }
    if (!check_label(label)) {
        printf("Invalid label (1-20 characters: letters, digits, - and _).\n");
        return 1;
    }

    // checking that the file exists locally before sending
    // anything, stat also gives us Fsize for the request
    if (stat(filename, &st) == -1 || !S_ISREG(st.st_mode)) {
        printf("File '%s' not found in the local directory. (May not be a regular file)\n", filename);
        return 1;
    }
    if (st.st_size > FSIZE_MAX) {
        printf("File too large (maximum %d bytes).\n", FSIZE_MAX);
        return 1;
    }

    // PUB UID password filename Fsize label
    snprintf(request, sizeof(request), "PUB %s %s %s %ld %s\n", 
             uid, password, filename, (long)st.st_size, label); // cast to long to match %ld

    if (ds_exchange(fd, request, reply, sizeof(reply), res)) return 1;

    /* RPB OK / RPB NOK / RPB NLG / RPB UNR / RPB WRP / RPB ERR */
    status = reply_status(reply, "RPB");
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

    char request[BUF_SIZE+1];
    char reply[VERSIONS_BUF]; // larger than the UDP buffer, the list may have many peers
    const char *status;

    if (!check_filename(filename)) {
        printf("Invalid filename.\n");
        return 1;
    }

    // VRS filename
    snprintf(request, sizeof(request), "VRS %s\n", filename);

    // unlike UDP, TCP failures all come back as a single error code
    if (tcp_request(ip, port, request, reply, sizeof(reply))) {
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

    // print_versions returns 1 if an entry is malformed, so it can be
    // returned directly as this function's result
    return print_versions((char *)status + 3);
}

int where(const char *filename, const char *uid, int fd, struct addrinfo *res,
          char *ip, size_t ip_size, char *port, size_t port_size) {

    char request[BUF_SIZE+1];
    char reply[BUF_SIZE+1];
    const char *status;

    // temporary copies, only written to the caller's buffers once the whole reply has been validated
    char peer_ip[BUF_SIZE+1], peer_port[BUF_SIZE+1], extra[BUF_SIZE+1];

    if (!check_filename(filename) || !check_uid(uid)) {
        printf("Invalid filename or UID.\n");
        return 1;
    }

    // WHR filename UID
    snprintf(request, sizeof(request), "WHR %s %s\n", filename, uid);

    if (ds_exchange(fd, request, reply, sizeof(reply), res)) return 1;

    // RWH OK IP peerTCPport / RWH NOK / RWH ERR
    status = reply_status(reply, "RWH");
    if (status == NULL) {
        printf("Unexpected reply from DS.\n");
        return 1;
    }

    if (!strncmp(status, "OK ", 3)) {
        // two fields after "OK ", IP and port
        if (sscanf(status + 3, "%s %s %s", peer_ip, peer_port, extra) != 2) {
            printf("Unexpected reply from DS.\n");
            return 1;
        }
        snprintf(ip, ip_size, "%s", peer_ip);
        snprintf(port, port_size, "%s", peer_port);
        return 0;
    }

    if (!strcmp(status, "NOK")) printf("Peer not available.\n");
    else if (!strcmp(status, "ERR")) printf("Error.\n");
    else printf("Unexpected reply from DS.\n");

    return 1;
}