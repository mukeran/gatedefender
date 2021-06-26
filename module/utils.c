#include "utils.h"

#include <linux/slab.h>

char* strdup(const char *str) {
  int length = strlen(str);
  char *new_str = (char*) kzalloc(sizeof(char) * (length + 1), GFP_KERNEL);
  strcpy(new_str, str);
  new_str[length] = '\0';
  return new_str;
}

char* memdup(const char *str, int size) {
  char *new_str = (char*) kzalloc(sizeof(char) * size, GFP_ATOMIC);
  memcpy(new_str, str, size);
  return new_str;
}

char* trim(char *str) {
  int length;
  while (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n')
    ++str;
  length = strlen(str);
  while (length > 0 && (str[length - 1] == ' ' || str[length - 1] == '\t' || str[length - 1] == '\r' || str[length - 1] == '\n'))
    --length;
  str[length] = '\0';
  return str;
}
