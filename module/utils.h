#ifndef _MODULE_UTILS_H
#define _MODULE_UTILS_H

#define _min(a, b) (a < b ? a : b)

char* strdup(const char *str);
char* memdup(const char *str, int size);
char* trim(char *str);

#endif
