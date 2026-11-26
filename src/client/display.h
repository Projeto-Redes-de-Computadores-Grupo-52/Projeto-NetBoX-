/**
 * @file display.h
 * @brief Human-friendly output of the DS replies (list and versions).
 *
 * This module only formats and prints. It knows nothing about sockets or
 * about how the replies were obtained: commands.c extracts the payload of
 * a reply (the part after the status) and hands it over to be displayed.
 *
 * Both functions use strtok() internally, so they MODIFY the string they
 * receive (delimiters are overwritten with '\0').
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <stddef.h> // size_t

/** One peer entry of a "RVR OK" reply. */
typedef struct {
    char uid[8];            // UID_SIZE + 1 (+ slack)
    char size[16];          // Fsize as text (max 8 digits)
    char label[24];         // LABEL_MAX_LEN + 1 (+ slack)
    char pub_time[20];      // "YYYYMMDD-HHMMSS" (+ slack)
    int available;          // 1 = AVL, 0 = NAV
} Version;

/**
 * @brief Prints the resource names of a "RLS OK" reply, one per line,
 *        under the heading "Available resources:".
 *
 * @param names  Space-separated filenames (the text after "RLS OK ").
 *               Modified by this function.
 */
void print_resources(char *names);

/**
 * @brief Parses the entries of a "RVR OK" reply into an array.
 *
 * @param entries  Space-separated entries (the text after "RVR OK ").
 *                 Modified by this function (strtok).
 * @param out      [out] Array that receives the entries.
 * @param max      Capacity of @p out.
 *
 * @return Number of entries parsed (>= 0), or -1 if the reply is malformed
 *         (incomplete entry, field too long, availability not AVL/NAV,
 *         or more than @p max entries).
 */
int parse_versions(char *entries, Version *out, size_t max);

/**
 * @brief Prints the entries as a table.
 *
 * @param v  Entries to print.
 * @param n  Number of entries.
 */
void print_versions(const Version *v, size_t n);

#endif /* DISPLAY_H */