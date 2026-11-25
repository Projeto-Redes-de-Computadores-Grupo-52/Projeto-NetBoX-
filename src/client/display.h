/**
 * @file display.h
 * @brief Human-friendly output of the DS replies (list and versions).
 */
#ifndef DISPLAY_H
#define DISPLAY_H

/**
 * @brief Prints the resource names of a "RLS OK" reply, one per line.
 * @param names  Space-separated filenames. Modified (strtok).
 */
void print_resources(char *names);

/**
 * @brief Prints the entries of a "RVR OK" reply as a table.
 * @param entries  Space-separated groups "UID Fsize label time AVL|NAV". Modified (strtok).
 * @return 0 on success, 1 if the reply is malformed.
 */
int print_versions(char *entries);

#endif