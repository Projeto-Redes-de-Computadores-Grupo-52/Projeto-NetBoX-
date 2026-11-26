/* display.c
 * Human-friendly output of the DS replies (list and versions).
 * Only formats and prints: it knows nothing about sockets. commands.c
 * extracts the payload of a reply and hands it over to be displayed.
 */

#include <stdio.h>
#include <string.h> // strtok, strcmp

#include "client/display.h"

// converts "AAAAMMDD-HHMMSS" into "DD-MM-AAAA HH:MM:SS"
// if the input does not match the format, it is copied unchanged
static void format_time(const char *raw, char *out, size_t out_size) {
    int y, mo, d, h, mi, s;

    // %4d%2d%2d, fixed-width fields, because there are no separators
    // between year, month and day in the DS format
    if (sscanf(raw, "%4d%2d%2d-%2d%2d%2d", &y, &mo, &d, &h, &mi, &s) == 6)
        snprintf(out, out_size, "%02d-%02d-%04d %02d:%02d:%02d", d, mo, y, h, mi, s);
    else
        snprintf(out, out_size, "%s", raw);
}

void print_resources(char *names) {
    printf("Available resources:\n");
    // strtok splits on spaces, the first call gets the string, the following
    // ones get NULL to continue where the previous one stopped
    for (char *tok = strtok(names, " "); tok != NULL; tok = strtok(NULL, " "))
        printf("  %s\n", tok);
}

int parse_versions(char *entries, Version *out, size_t max) {
    
    size_t n = 0;

    for (char *uid = strtok(entries, " "); uid != NULL; uid = strtok(NULL, " ")) {
        char *size     = strtok(NULL, " ");
        char *label    = strtok(NULL, " ");
        char *pub_time = strtok(NULL, " ");
        char *avail    = strtok(NULL, " ");

        // truncated entry, or more entries than we can hold
        if (!size || !label || !pub_time || !avail || n >= max) return -1;

        // the length checks make the strcpy calls below safe
        if (strlen(uid) >= sizeof(out[n].uid) || strlen(size) >= sizeof(out[n].size) ||
            strlen(label) >= sizeof(out[n].label) || strlen(pub_time) >= sizeof(out[n].pub_time))
            return -1;

        // anything other than AVL/NAV does not follow the protocol
        if (strcmp(avail, "AVL") != 0 && strcmp(avail, "NAV") != 0) return -1;

        strcpy(out[n].uid, uid);
        strcpy(out[n].size, size);
        strcpy(out[n].label, label);
        strcpy(out[n].pub_time, pub_time);
        out[n].available = !strcmp(avail, "AVL");
        n++;
    }
    return (int)n;
}

void print_versions(const Version *v, size_t n) {
    // %-8s: left-aligned, minimum width 8
    printf("%-8s %-10s %-20s %-22s %s\n", "UID", "Size", "Label", "Published", "Status");

    for (size_t i = 0; i < n; i++) {
        char when[32];
        format_time(v[i].pub_time, when, sizeof(when));
        printf("%-8s %-10s %-20s %-22s %s\n", v[i].uid, v[i].size, v[i].label, when,
               v[i].available ? "Online" : "Offline");
    }
}