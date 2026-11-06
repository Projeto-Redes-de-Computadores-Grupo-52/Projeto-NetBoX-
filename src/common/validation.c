#include <ctype.h>
#include <string.h>
#include "common/config.h"
#include "common/validation.h"

int check_uid(const char *uid) {
    if (strlen(uid) != UID_SIZE)
        return 0;

    for (int i = 0; i < UID_SIZE; i++)
        if (!isdigit((unsigned char)uid[i]))
            return 0;

    return 1;
}

int check_pwd(const char *password) {
    if (strlen(password) != PWD_SIZE) return 0;

    for (int i = 0; i < PWD_SIZE; i++)
        if (!isalnum((unsigned char)password[i])) return 0;

    return 1;
}

int check_filename(const char *filename) {
    size_t len = strlen(filename);

    if (len < 5 || len > FILENAME_MAX_LEN) return 0;
    if (filename[len-4] != '.') return 0;

    for (size_t i = 0; i < len-4; i++)
        if (!isalnum((unsigned char)filename[i])
            && filename[i] != '-' && filename[i] != '_')
            return 0;

    for (size_t i = len-3; i < len; i++)
        if (!isalnum((unsigned char)filename[i])) return 0;

    return 1;
}