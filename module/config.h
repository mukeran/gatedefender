/**
 * config.h - Header for config.c
 * @author mukeran
 */
#ifndef _MODULE_CONFIG_H
#define _MODULE_CONFIG_H

#include <linux/module.h>

#include "utils.h"

#define BLOCK_UDP "block_udp"
#define BLOCK_DNS "block_dns"
#define BLOCK_TCP "block_tcp"
#define BLOCK_ICMP "block_icmp"

#define BLOCK_DNS_TUNNELING_STREAM "block_dns_tunneling_stream"
#define BLOCK_ICMP_TUNNELING_STREAM "block_icmp_tunneling_stream"

#define ECONFIG 20001

typedef struct config {
  int count, capacity;
  char **keys;
  char **values;
} config_t;

extern struct config *global_config;

void init_global_config(void);
int serialize_global_config(char **data);
int unserialize_global_config(const char *data, int length, u8 overwrite);
const char *get_global_config(const char *key);
void set_global_config(const char *key, const char *value);

#endif
