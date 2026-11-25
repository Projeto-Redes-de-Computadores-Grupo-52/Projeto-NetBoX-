#include <stdio.h>
#include <string.h>

#include "client/display.h"

// Converts "20261123-174721" into "23-11-2026 17:47:21".
// If the input does not match the format, it is copied unchanged.
static void format_time(const char *raw, char *out, size_t out_size) {
    int y, mo, d, h, mi, s;

    if (sscanf(raw, "%4d%2d%2d-%2d%2d%2d", &y, &mo, &d, &h, &mi, &s) == 6)
        snprintf(out, out_size, "%02d-%02d-%04d %02d:%02d:%02d", d, mo, y, h, mi, s);
    else
        snprintf(out, out_size, "%s", raw);
}

void print_resources(char *names) {
    printf("Available resources:\n");
    for (char *tok = strtok(names, " "); tok != NULL; tok = strtok(NULL, " "))
        printf("  %s\n", tok);
}

int print_versions(char *entries) {
    // %-8s: left-aligned, minimum width 8
    printf("%-8s %-10s %-20s %-22s %s\n", "UID", "Size", "Label", "Published", "Status");

    for (char *uid = strtok(entries, " "); uid != NULL; uid = strtok(NULL, " ")) {
        char *size     = strtok(NULL, " ");
        char *label    = strtok(NULL, " ");
        char *raw_time = strtok(NULL, " ");
        char *avail    = strtok(NULL, " ");
        char when[32];

        if (!size || !label || !raw_time || !avail) {
            printf("Malformed reply from DS.\n");
            return 1;
        }

        format_time(raw_time, when, sizeof(when));
        printf("%-8s %-10s %-20s %-22s %s\n", uid, size, label, when,
               !strcmp(avail, "AVL") ? "Online" : "Offline");
    }
    return 0;
}