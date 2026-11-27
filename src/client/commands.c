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
#include <stdlib.h>     // size_t
#include <unistd.h> 
#include <arpa/inet.h>  //INET_ADDRSTRLEN    

#include "client/commands.h"
#include "common/config.h"              // BUF_SIZE, VERSIONS_BUF, FSIZE_MAX
#include "common/validation.h"          // check_uid, check_pwd, check_filename, check_label
#include "client/network/udp_client.h"  // udp_communicate            
#include "client/network/tcp_client.h"  // tcp_request
#include "client/display.h"             // print_resources, print_versions
#include "common/io.h"

// returns a pointer to the reply status after the reply code
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

// asks the DS for the versions of 'filename' and parses them
// returns the number of entries (> 0), 0 if nobody publishes the file, or -1
// on error (already reported to the user), "no peers" is not
// reported here, so each caller says what fits its context
static int fetch_versions(const char *filename, const char *ip, const char *port,
                          Version *list, size_t max) {

    char request[BUF_SIZE+1];
    char reply[VERSIONS_BUF];
    const char *status;
    int n;

    // VRS filename
    snprintf(request, sizeof(request), "VRS %s\n", filename);

    if (tcp_request(ip, port, request, reply, sizeof(reply))) {
        printf("Error: could not get a reply from the DS (TCP).\n");
        return -1;
    }

    // RVR NOK / RVR OK [UID Fsize label publication_time availability]* / RVR ERR
    status = reply_status(reply, "RVR");
    if (status == NULL) {
        printf("Unexpected reply from DS.\n");
        return -1;
    }

    if (!strcmp(status, "NOK")) return 0;
    if (!strcmp(status, "ERR")) {
        printf("Error.\n");
        return -1;
    }
    if (strncmp(status, "OK ", 3) != 0) {
        printf("Unexpected reply from DS.\n");
        return -1;
    }

    // parse first, print after: a malformed reply prints nothing at all
    n = parse_versions((char *)status + 3, list, max);
    if (n <= 0) {
        printf("Malformed reply from DS.\n");
        return -1;
    }
    return n;
}

// downloads 'filename' from ip:port into the current directory
// writes to "<filename>.tmp" and renames at the end, so a failed or partial
// transfer never leaves a file under the real name
// Returns 0 on success, 1 on any error (already reported to the user)
static int download(const char *filename, const char *ip, const char *port) {

    char request[BUF_SIZE+1];
    char token[32];                 // "RGT", the status and Fsize
    char tmp[FILENAME_MAX_LEN + 8]; // temporary file's name
    char buffer[BUF_SIZE];
    long fsize;
    long left;                      // leftovers
    int fd, d;
    FILE *f = NULL;

    snprintf(request, sizeof(request), "GET %s\n", filename);
    snprintf(tmp, sizeof(tmp), "%s.tmp", filename);

    if ((fd = tcp_connect(ip, port)) == -1) {
        printf("Could not connect to the peer.\n");
        return 1;
    }
    if (io_write_all(fd, request, strlen(request)) == -1) {
        printf("Could not send the request to the peer.\n");
        goto fail;
    }

    // header: "RGT <status> ..." read token by token
    if (io_read_token(fd, token, sizeof(token), " ") == -1 || strcmp(token, "RGT") != 0) {
        printf("Unexpected reply from the peer.\n");
        goto fail;
    }

    // "OK " ends with a space, "NOK\n" / "ERR\n" end with '\n'
    if ((d = io_read_token(fd, token, sizeof(token), " \n")) == -1) {
        printf("Unexpected reply from the peer.\n");
        goto fail;
    }
    if (!strcmp(token, "NOK") && d == '\n') {
        printf("The peer does not have '%s'.\n", filename);
        goto fail;
    }
    if (strcmp(token, "OK") || d != ' ') {
        printf("Unexpected reply from the peer.\n");
        goto fail;
    }

    // Fsize
    if (io_read_token(fd, token, sizeof(token), " ") == -1 || !check_fsize(token, &fsize)) {
        printf("Invalid file size sent by the peer.\n");
        goto fail;
    }

    // file contents, exactly fsize bytes, in blocks (BUF_SIZE)
    if ((f = fopen(tmp, "wb")) == NULL) {
        printf("Could not create '%s'.\n", tmp);
        goto fail;
    }
    for (left = fsize; left > 0; ) {

        // if the file is smaller than the buffer we only need to read and write once
        size_t chunk = left > (long)sizeof(buffer) ? sizeof(buffer) : (size_t)left;

        if (io_read_exact(fd, buffer, chunk) == -1) {
            printf("Transfer interrupted: the peer sent fewer bytes than announced.\n");
            goto fail;
        }
        if (fwrite(buffer, 1, chunk, f) != chunk) { // element size 1, chunk elements
            printf("Could not write to disk.\n");
            goto fail;
        }
        
        left -= (long)chunk;
    }

    // the protocol ends the message with '\n', but the file is already
    // complete after fsize bytes, so the final '\n' is not required, we do
    // not read it, and the connection is closed by us, as the protocol says

    // fclose can fail on the final flush, only then is the file really saved
    if (fclose(f) != 0) {
        f = NULL;
        printf("Could not write to disk.\n");
        goto fail;
    }
    
    f = NULL;
    close(fd);

    if (rename(tmp, filename) == -1) {
        printf("Could not rename '%s' to '%s'.\n", tmp, filename);
        remove(tmp);
        return 1;
    }

    printf("Download complete: '%s' saved in the current directory (%ld bytes).\n",
           filename, fsize);
    
    return 0;

    // single cleanup point, never leaves a socket or a partial file behind
    // could've used functions instead of goto, but in this case it made the code simpler

fail:   
    if (f != NULL) fclose(f);
    remove(tmp); // harmless if it was never created
    close(fd);
    return 1;
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

    Version list[MAX_VERSIONS];
    int n;

    if (!check_filename(filename)) {
        printf("Invalid filename.\n");
        return 1;
    }

    n = fetch_versions(filename, ip, port, list, MAX_VERSIONS);
    if (n < 0) return 1;
    if (n == 0) {
        printf("No peers publishing '%s'.\n", filename);
        return 0;       // "no peers" is a normal answer, not an error
    }

    print_versions(list, (size_t)n);
    return 0;
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

    /* RWH OK IP peerTCPport / RWH NOK / RWH ERR */
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

int get(const char *filename, const char *ip, const char *port, int fd, struct addrinfo *res) {
    
    char answer[BUF_SIZE+1], uid[BUF_SIZE+1], extra[BUF_SIZE+1]; // user's UID choice
    char peer_ip[INET_ADDRSTRLEN], peer_port[6];
    Version list[MAX_VERSIONS];
    int n, chosen = -1, online = 0;

    if (!check_filename(filename)) {
        printf("Invalid filename.\n");
        return 1;
    }

    // 1. versions of the file
    n = fetch_versions(filename, ip, port, list, MAX_VERSIONS);
    if (n < 0) return 1;
    if (n == 0) {
        printf("No peers publishing '%s'.\n", filename);
        return 1; // for get, "nobody has it" is a failure
    }

    // NOTE: there could be a change of availability to one of the users

    // 2. choose the version
    for (int i = 0; i < n; i++)
        if (list[i].available) { online++; chosen = i; } // chosen is here in case there's only one version

    if (online == 0) {
        print_versions(list, (size_t)n);
        printf("No peer is online right now.\n");
        return 1;
    }

    if (n > 1) { // several versions, show them and ask which UID
        print_versions(list, (size_t)n);
        
        // warn the user here
        printf("Note: availability may change while you choose. If the download "
               "fails, run 'versions %s' again to see the current state.\n", filename);
        
        printf("UID to download from: ");
        fflush(stdout);
        
        // blocking read on purpose - one command at a time
        if (fgets(answer, sizeof(answer), stdin) == NULL ||
            sscanf(answer, "%s %s", uid, extra) != 1) {
            printf("Cancelled.\n");
            return 1;
        }
        
        chosen = -1;
        for (int i = 0; i < n; i++)
            if (!strcmp(list[i].uid, uid)) chosen = i;

        if (chosen == -1) { printf("That UID does not publish '%s'.\n", filename); return 1; }
        if (!list[chosen].available) { printf("User %s is offline.\n", uid); return 1; }
    }
    
    // n == 1 and online == 1, chosen already points to the only entry

    // 3. confirm the peer is still active and get its address
    if (where(filename, list[chosen].uid, fd, res,
              peer_ip, sizeof(peer_ip), peer_port, sizeof(peer_port))) return 1;
    if (!check_port(peer_port)) {
        printf("The DS sent an invalid peer port.\n");
        return 1;
    }

    // 4. download
    return download(filename, peer_ip, peer_port);
}