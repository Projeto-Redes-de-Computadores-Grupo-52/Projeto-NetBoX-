/**
 * @file validation.h
 * @brief Local validation of protocol fields.
 *
 * Used to reject invalid input before anything is sent to the DS.
 *
 * Return convention (all functions): 1 if the value is valid, 0 if it is
 * not. Note this is the opposite of the commands, where 0 means success:
 * here the functions answer a yes/no question, so they can be used
 * directly in conditions.
 *
 * None of the functions modifies its argument.
 */

#ifndef VALIDATION_H
#define VALIDATION_H

/**
 * @brief Checks a user ID: exactly UID_SIZE (6) decimal digits.
 *
 * @param uid  String to check.
 *
 * @return 1 if valid, 0 otherwise.
 */
int check_uid(const char *uid);

/**
 * @brief Checks a password: exactly PWD_SIZE (8) ASCII letters or digits.
 *
 * @param password  String to check.
 *
 * @return 1 if valid, 0 otherwise.
 */
int check_pwd(const char *password);

/**
 * @brief Checks a filename in the form "name.ext".
 *
 * Rules: at most FILENAME_MAX_LEN (24) characters in total, including the
 * dot and the extension. The base name has at least 1 character: letters,
 * digits, '-' or '_'. It is followed by a dot and an extension of exactly
 * 3 alphanumeric characters.
 *
 * @param filename  String to check.
 *
 * @return 1 if valid, 0 otherwise.
 */
int check_filename(const char *filename);

/**
 * @brief Checks a label: 1 to LABEL_MAX_LEN (20) characters, letters,
 *        digits, '-' or '_'.
 *
 * @param label  String to check.
 *
 * @return 1 if valid, 0 otherwise.
 */
int check_label(const char *label);

/**
 * @brief Checks a port: a decimal integer between PORT_MIN (1) and
 *        PORT_MAX (65535), with no extra characters.
 *
 * Used for the command line arguments and for the peer port sent by the DS.
 *
 * @param s  String to check.
 *
 * @return 1 if valid, 0 otherwise.
 */
int check_port(const char *s);

/**
 * @brief Checks a file size received from a peer: only digits, at most
 *        8 of them, and a value from 0 to FSIZE_MAX (10,000,000).
 *
 * Signs, spaces and other characters are rejected.
 *
 * @param s    String to check.
 * @param out  [out] The numeric value. Only written when the result is 1.
 *
 * @return 1 if valid, 0 otherwise.
 */
int check_fsize(const char *s, long *out);

#endif /* VALIDATION_H */