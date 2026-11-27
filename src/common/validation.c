/* validation.c
 * Local validation of protocol fields.
 * Used to reject invalid input before anything is sent to the DS.
 *
 * All functions return 1 if the value is valid and 0 otherwise.
 */

#include <ctype.h>  // isdigit, isalnum
#include <string.h> // strlen
#include <stdlib.h>

#include "common/config.h"      // UID_SIZE, PWD_SIZE, FILENAME_MAX_LEN, LABEL_MAX_LEN
#include "common/validation.h"

int check_port(const char *s) {
    char *end;
    long v = strtol(s, &end, 10);
    return !(*s == '\0' || *end != '\0' || v < PORT_MIN || v > PORT_MAX);
}

int check_uid(const char *uid) {

    // check the length first, it also protects the loop below from reading
    // past the end of a shorter string
    if (strlen(uid) != UID_SIZE)
        return 0;

    // the (unsigned char) cast is required by isdigit/isalnum: passing a
    // negative char is undefined behavior
    for (int i = 0; i < UID_SIZE; i++)
        if (!isdigit((unsigned char)uid[i]))
            return 0;

    return 1;
}

int check_pwd(const char *password) {
    
    if (strlen(password) != PWD_SIZE) return 0;

    // isalnum is enough here (letters and digits only)
    for (int i = 0; i < PWD_SIZE; i++)
        if (!isalnum((unsigned char)password[i])) return 0;

    return 1;
}

int check_filename(const char *filename) {
    
    size_t len = strlen(filename);

    // minimum 5, at least 1 character of "base name" + '.' + 3 chars for the extension
    if (len < 5 || len > FILENAME_MAX_LEN) return 0;

    // the extension has exactly 3 characters, so the dot is 4th from the end
    if (filename[len-4] != '.') return 0;

    // base name, everything before the dot
    for (size_t i = 0; i < len-4; i++)
        if (!isalnum((unsigned char)filename[i])
            && filename[i] != '-' && filename[i] != '_')
            return 0;

    // extension, the last 3 characters, alphanumeric only
    for (size_t i = len-3; i < len; i++)
        if (!isalnum((unsigned char)filename[i])) return 0;

    return 1;
}

int check_label(const char *label) {
    
    size_t len = strlen(label);

    // the label has 1 to LABEL_MAX_LEN (20) characters
    if (len < 1 || len > LABEL_MAX_LEN) return 0;

    // same allowed characters as the filename "base name": letters, digits, '-' and '_'
    for (size_t i = 0; i < len; i++)
        if (!isalnum((unsigned char)label[i])
            && label[i] != '-' && label[i] != '_')
            return 0;

    return 1;
}

int check_fsize(const char *s, long *out) {
    // only digits, at most 8 (strtol would also accept spaces and signs)
    if (*s == '\0' || strlen(s) > 8) return 0;
    for (const char *p = s; *p; p++)
        if (!isdigit((unsigned char)*p)) return 0;

    long v = strtol(s, NULL, 10);
    if (v > FSIZE_MAX) return 0;

    *out = v;
    return 1;
}