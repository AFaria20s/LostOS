#include "lib/kstring.h"

size_t k_strlen(const char *str) {
  // Return length of a null-terminated string
  size_t len = 0;

  while (str[len])
    len++;

  return len;
}

int k_strcmp(const char *a, const char *b) {
  // Compare two strings and return byte difference
  while (*a && *a == *b) {
    a++;
    b++;
  }

  return (unsigned char)*a - (unsigned char)*b;
}

static int k_isspace(char c) {
  // Check space or tab
  return c == ' ' || c == '\t';
}

int k_split(char *line, char **argv, int max_args) {
  // Split the line into arguments in-place and return argc
  int argc = 0;

  while (*line && argc < max_args) {
    while (k_isspace(*line)) {
      *line = '\0';
      line++;
    }

    if (!*line)
      break;

    argv[argc++] = line;

    while (*line && !k_isspace(*line))
      line++;
  }

  while (*line) {
    if (k_isspace(*line))
      *line = '\0';
    line++;
  }

  return argc;
}

void k_itoa(int value, char *buffer, int base) {
  // Convert integer to string in given base (2..16)
  char digits[] = "0123456789abcdef";
  char tmp[33];
  int i = 0;
  int negative = 0;
  unsigned int n;

  if (base < 2 || base > 16) {
    buffer[0] = '\0';
    return;
  }

  if (value < 0 && base == 10) {
    negative = 1;
    n = (unsigned int)(-value);
  } else {
    n = (unsigned int)value;
  }

  do {
    tmp[i++] = digits[n % (unsigned int)base];
    n /= (unsigned int)base;
  } while (n);

  if (negative)
    tmp[i++] = '-';

  while (i > 0)
    *buffer++ = tmp[--i];

  *buffer = '\0';
}

void k_strcp(char *dest, const char *src) {
  int i = 0;
  while (src[i]!='\0') {
    dest[i] = src[i];
    i++;
  }
  dest[i]='\0';
}

// Index 'src' at the end of 'dest' (assuming valid '\0')
void k_strapp(char *dest, const char *src) {
  int i = k_strlen(dest);
  int j = 0;

  while (src[j] != '\0') {
    dest[i] = src[j];
    i++;
    j++;
  }
  dest[i] = '\0';
}

// Concatenate 2 strings
void k_strcat(char *dest, const char *src1, const char *src2, const char *sep) {
  k_strcp(dest, src1);

  if (sep != NULL) k_strapp(dest, sep);

  k_strapp(dest, src2);
}

int k_strncmp(const char *a, const char *b, size_t n) {
  while (n > 0 && *a && (*a == *b)) {
    a++;
    b++;
    n--;
  }

  if (n == 0)
    return 0;

  return (unsigned char)*a - (unsigned char)*b;
}