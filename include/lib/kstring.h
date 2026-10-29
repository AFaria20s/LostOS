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

// Concatenates 2 strings
int k_strcat(char *dest, size_t capacity, const char *src1, const char *src2, const char *sep);

// Index 'src' at the end of 'dest' (assuming valid '\0')
int k_strapp(char *dest, size_t capacity, const char *src);

int k_strcp(char *dest, size_t capacity, const char *src);

// Compare first n characters from string
int k_strncmp(const char *a, const char *b, size_t n);

#endif