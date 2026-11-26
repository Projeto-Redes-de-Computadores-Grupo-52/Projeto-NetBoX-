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

int print_versions(char *entries) {
    // %-8s, left-aligned, minimum width 8, the same widths are used in the
    // header and in the rows so the columns line up
    printf("%-8s %-10s %-20s %-22s %s\n", "UID", "Size", "Label", "Published", "Status");

    // each entry has exactly 5 space-separated fields
    for (char *uid      = strtok(entries, " "); uid != NULL; uid = strtok(NULL, " ")) {
         char *size     = strtok(NULL, " ");
         char *label    = strtok(NULL, " ");
         char *raw_time = strtok(NULL, " ");
         char *avail    = strtok(NULL, " ");
         char when[32]; // "DD-MM-YYYY HH:MM:SS" has 19 chars

         // a truncated reply leaves some field NULL, stop before using it
        if (!size || !label || !raw_time || !avail) {
            printf("Malformed reply from DS.\n");
            return 1;
        }

        format_time(raw_time, when, sizeof(when));
        printf("%-8s %-10s %-20s %-22s %s\n", uid, size, label, when,
               !strcmp(avail, "AVL") ? "Online" : "Offline");
            // AVL = available, NAV = not available
    }
    return 0;
}