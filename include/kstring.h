#ifndef KSTRING_H
#define KSTRING_H

#include <stddef.h>

// Simple helpers for '\0'-terminated strings.
size_t k_strlen(const char *str);
int k_strcmp(const char *a, const char *b);

// Splits a line into words inside the same buffer.
int k_split(char *line, char **argv, int max_args);

// Converts an integer to text.
void k_itoa(int value, char *buffer, int base);

#endif
