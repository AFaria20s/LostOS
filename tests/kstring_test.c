#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "lib/kstring.h"

static int failures;

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);      \
      failures++;                                                               \
    }                                                                           \
  } while (0)

static void test_strlen(void) {
  CHECK(k_strlen("") == 0);
  CHECK(k_strlen("LostOS") == 6);
  CHECK(k_strlen("a string with spaces") == 20);
}

static void test_string_comparison(void) {
  CHECK(k_strcmp("", "") == 0);
  CHECK(k_strcmp("same", "same") == 0);
  CHECK(k_strcmp("abc", "abd") < 0);
  CHECK(k_strcmp("abd", "abc") > 0);
  CHECK(k_strcmp("abc", "abcd") < 0);
  CHECK(k_strcmp("abcd", "abc") > 0);

  CHECK(k_strncmp("command", "command-line", 7) == 0);
  CHECK(k_strncmp("command", "commute", 4) == 0);
  CHECK(k_strncmp("command", "commute", 5) < 0);
  CHECK(k_strncmp("abc", "xyz", 0) == 0);
  CHECK(k_strncmp("abc", "abcd", 4) < 0);
}

static void test_split(void) {
  char line[] = "  lost   file.txt\t/home  ";
  char *argv[4];
  int argc = k_split(line, argv, 4);

  CHECK(argc == 3);
  CHECK(strcmp(argv[0], "lost") == 0);
  CHECK(strcmp(argv[1], "file.txt") == 0);
  CHECK(strcmp(argv[2], "/home") == 0);

  {
    char limited[] = "one two three";
    char *limited_argv[2];

    argc = k_split(limited, limited_argv, 2);
    CHECK(argc == 2);
    CHECK(strcmp(limited_argv[0], "one") == 0);
    CHECK(strcmp(limited_argv[1], "two") == 0);
    CHECK(limited[7] == '\0');
  }

  {
    char empty[] = " \t ";
    char *empty_argv[1];

    CHECK(k_split(empty, empty_argv, 1) == 0);
  }
}

static void test_itoa(void) {
  char buffer[40];

  k_itoa(0, buffer, 10);
  CHECK(strcmp(buffer, "0") == 0);
  k_itoa(12345, buffer, 10);
  CHECK(strcmp(buffer, "12345") == 0);
  k_itoa(-12345, buffer, 10);
  CHECK(strcmp(buffer, "-12345") == 0);
  k_itoa(INT_MIN, buffer, 10);
  CHECK(strcmp(buffer, "-2147483648") == 0);
  k_itoa(255, buffer, 16);
  CHECK(strcmp(buffer, "ff") == 0);
  k_itoa(5, buffer, 2);
  CHECK(strcmp(buffer, "101") == 0);
  k_itoa(-15, buffer, 16);
  CHECK(strcmp(buffer, "fffffff1") == 0);
  k_itoa(42, buffer, 1);
  CHECK(strcmp(buffer, "") == 0);
  k_itoa(42, buffer, 17);
  CHECK(strcmp(buffer, "") == 0);
}

static void test_copy_and_append(void) {
  char buffer[8] = "old";

  CHECK(k_strcp(buffer, sizeof(buffer), "hello") == 1);
  CHECK(strcmp(buffer, "hello") == 0);
  CHECK(k_strcp(buffer, sizeof(buffer), "1234567") == 1);
  CHECK(strcmp(buffer, "1234567") == 0);
  CHECK(k_strcp(buffer, sizeof(buffer), "12345678") == 0);
  CHECK(strcmp(buffer, "1234567") == 0);
  CHECK(k_strcp(buffer, 0, "x") == 0);

  CHECK(k_strapp(buffer, sizeof(buffer), "") == 1);
  CHECK(strcmp(buffer, "1234567") == 0);
  CHECK(k_strapp(buffer, sizeof(buffer), "x") == 0);
  CHECK(strcmp(buffer, "1234567") == 0);

  CHECK(k_strcp(buffer, sizeof(buffer), "") == 1);
  CHECK(k_strapp(buffer, sizeof(buffer), "lost") == 1);
  CHECK(strcmp(buffer, "lost") == 0);
  CHECK(k_strapp(buffer, sizeof(buffer), "os") == 1);
  CHECK(strcmp(buffer, "lostos") == 0);
}

static void test_concat(void) {
  char buffer[9] = "unchanged";

  CHECK(k_strcat(buffer, sizeof(buffer), "Lost", "OS", " ") == 1);
  CHECK(strcmp(buffer, "Lost OS") == 0);
  CHECK(k_strcat(buffer, sizeof(buffer), "Lost", "OS", NULL) == 1);
  CHECK(strcmp(buffer, "LostOS") == 0);
  CHECK(k_strcat(buffer, sizeof(buffer), "1234567", "8", NULL) == 1);
  CHECK(strcmp(buffer, "12345678") == 0);
  CHECK(k_strcat(buffer, sizeof(buffer), "12345678", "9", NULL) == 0);
  CHECK(strcmp(buffer, "12345678") == 0);
  CHECK(k_strcat(buffer, 0, "a", "b", NULL) == 0);
}

int main(void) {
  test_strlen();
  test_string_comparison();
  test_split();
  test_itoa();
  test_copy_and_append();
  test_concat();

  if (failures != 0) {
    fprintf(stderr, "%d test assertion(s) failed\n", failures);
    return 1;
  }

  puts("kstring tests passed");
  return 0;
}
