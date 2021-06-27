/**
 * utils.c - Some useful common functions and data structures
 * @author mukeran
 */
#include "utils.h"

#include <linux/slab.h>

/**
 * Implements of strdup
 */
char* strdup(const char *str) {
  int length = strlen(str);
  char *new_str = (char*) kzalloc(sizeof(char) * (length + 1), GFP_KERNEL);
  strcpy(new_str, str);
  new_str[length] = '\0';
  return new_str;
}

/**
 * Implements of memdup
 * 这里 kzalloc 的 flag 为 GFP_ATOMIC，适用于 netfilter hook 上下文中的内存申请
 */
char* memdup(const char *str, int size) {
  char *new_str = (char*) kzalloc(sizeof(char) * size, GFP_ATOMIC);
  memcpy(new_str, str, size);
  return new_str;
}

/**
 * Remove blank letters from two ends of str
 */
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

/**
 * Create a new hash_list instance
 * 创建一个新的哈希表实例
 */
hash_list_t* alloc_hash_list(void) {
  struct hash_list *hl = (struct hash_list *) kzalloc(sizeof(struct hash_list), GFP_ATOMIC);
  hl->hash = (struct hash_list **) kzalloc(sizeof(struct hash_list *) * HASH_LIST_SIZE, GFP_ATOMIC);
  hl->count = 0;
  return hl;
}

/**
 * Check if hash_list has hash number
 * 检查哈希表中是否存在该哈希值
 */
hash_list_t* check_hash(struct hash_list* hl, int hash) {
  if (hash >= HASH_LIST_SIZE)
    return NULL;
  if (hl->hash[hash] != NULL)
    return hl->hash[hash];
  return NULL;
}

/**
 * Insert hash number to hash_list
 * 向哈希表中插入值
 */
hash_list_t* insert_hash(struct hash_list* hl, int hash) {
  if (hash >= HASH_LIST_SIZE)
    return NULL;
  if (hl->hash[hash] != NULL)
    return hl->hash[hash];
  hl->hash[hash] = alloc_hash_list();
  ++hl->count;
  return hl->hash[hash];
}
