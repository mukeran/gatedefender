/**
 * utils.h - Header for utils.c
 * @author mukeran
 */
#ifndef _MODULE_UTILS_H
#define _MODULE_UTILS_H

#define _min(a, b) (a < b ? a : b)

#define HASH_LIST_SIZE 10007

char* strdup(const char* str);
char* memdup(const char* str, int size);
char* trim(char* str);
typedef struct hash_list {
  struct hash_list** hash;
  int count;
} hash_list_t;

hash_list_t* alloc_hash_list(void);
hash_list_t* check_hash(struct hash_list* hl, int hash);
hash_list_t* insert_hash(struct hash_list* hl, int hash);

#endif
