#include "../include/kstring.h"

size_t k_strlen(const char *str) {
  size_t len = 0;

  while (str[len])
    len++;

  return len;
}

int k_strcmp(const char *a, const char *b) {
  while (*a && *a == *b) {
    a++;
    b++;
  }

  return (unsigned char)*a - (unsigned char)*b;
}

static int k_isspace(char c) {
  return c == ' ' || c == '\t';
}

int k_split(char *line, char **argv, int max_args) {
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
