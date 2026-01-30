#ifndef TEST_SECURITY_H
#define TEST_SECURITY_H

int sanitize_path(const char *input, char *output, size_t outlen);
char *scanner(const char *filename, int *status_code);

#endif