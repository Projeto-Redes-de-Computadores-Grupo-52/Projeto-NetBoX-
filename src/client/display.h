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

/**
 * @brief Prints the resource names of a "RLS OK" reply, one per line,
 *        under the heading "Available resources:".
 *
 * @param names  Space-separated filenames (the text after "RLS OK ").
 *               Modified by this function.
 */
void print_resources(char *names);

/**
 * @brief Prints the entries of a "RVR OK" reply as a table.
 *
 * Each entry has 5 fields: UID, size, label, publication time and
 * availability. The time is converted from "YYYYMMDD-HHMMSS" to
 * "DD-MM-YYYY HH:MM:SS" (shown unchanged if it does not match), and
 * availability is shown as "Online" (AVL) or "Offline" (NAV).
 *
 * @param entries  Space-separated entries (the text after "RVR OK ").
 *                 Modified by this function.
 *
 * @return 0 on success, 1 if an entry is incomplete (malformed reply).
 *         In that case the header and the entries before it were
 *         already printed.
 */
int print_versions(char *entries);

#endif /* DISPLAY_H */