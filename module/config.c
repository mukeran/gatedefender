#include "config.h"

#include <linux/slab.h>
#include <linux/string.h>
#include <linux/proc_fs.h>

#define CONFIG_CAPACITY_LEVEL 10
#define BUFFER_CAPACITY_LEVEL 1024

struct config *global_config = NULL;

static void grow_config_capacity(struct config *conf) {
  conf->capacity += CONFIG_CAPACITY_LEVEL;
  conf->keys = (char**)krealloc(conf->keys, sizeof(char*) * conf->capacity, GFP_KERNEL | __GFP_ZERO);
  conf->values = (char**)krealloc(conf->values, sizeof(char*) * conf->capacity, GFP_KERNEL | __GFP_ZERO);
}

const char *get_config(struct config *conf, const char *key) {
  int i;
  for (i = 0; i < conf->count; ++i) {
    if (strcmp(conf->keys[i], key) == 0)
      return conf->values[i];
  }
  return "";
}

void set_config(struct config *conf, const char *key, const char *value) {
  int i, id;
  for (i = 0; i < conf->count; ++i) {
    if (strcmp(conf->keys[i], key) == 0) {
      kfree(conf->values[i]);
      conf->values[i] = strdup(value);
      return;
    }
  }
  if (conf->count >= conf->capacity)
    grow_config_capacity(conf);
  id = conf->count;
  conf->keys[id] = strdup(key);
  conf->values[id] = strdup(value);
  ++conf->count;
}

struct config* init_config(void) {
  struct config *conf;
  conf = (struct config*)kzalloc(sizeof(struct config), GFP_KERNEL);
  conf->capacity = CONFIG_CAPACITY_LEVEL;
  conf->count = 0;
  conf->keys = (char**)kzalloc(sizeof(char*) * conf->capacity, GFP_KERNEL);
  conf->values = (char**)kzalloc(sizeof(char*) * conf->capacity, GFP_KERNEL);
  return conf;
}

static void free_config(struct config *conf) {
  int i;
  if (conf == NULL)
    return;
  for (i = 0; i < conf->count; ++i) {
    if (conf->keys[i])
      kfree(conf->keys[i]);
    if (conf->values[i])
      kfree(conf->values[i]);
  }
  kfree(conf->keys);
  kfree(conf->values);
  kfree(conf);
}

void init_global_config(void) {
  if (global_config != NULL)
    return;
  global_config = init_config();
  /* Default global configs */
  set_global_config(BLOCK_UDP, "0");
  set_global_config(BLOCK_DNS, "0");
  set_global_config(BLOCK_TCP, "0");
  set_global_config(BLOCK_ICMP, "0");
  set_global_config(BLOCK_RAW, "0");
}

void free_global_config(void) {
  free_config(global_config);
}

int serialize_global_config(char **data) {
  int i, length = 0, capacity = BUFFER_CAPACITY_LEVEL, append_length;
  *data = (char *)kzalloc(sizeof(char) * capacity, GFP_KERNEL);
  for (i = 0; i < global_config->count; ++i) {
    append_length = strlen(global_config->keys[i]) + strlen(global_config->values[i]) + 4;
    if (length + append_length >= capacity) {
      capacity += BUFFER_CAPACITY_LEVEL;
      *data = (char *)krealloc(*data, sizeof(char) * capacity, GFP_KERNEL | __GFP_ZERO);
    }
    sprintf(*data + length, "%s = %s\n", global_config->keys[i], global_config->values[i]);
    length += append_length;
  }
  return length;
}

int unserialize_global_config(const char *data, int length, u8 overwrite) {
  int id, line_length;
  char *sep = strdup(data), *pos, *token, *key, *value;
  struct config *temp_config = NULL;
  if (overwrite)
    temp_config = init_config();
  for (token = strsep(&sep, "\n"); token != NULL; token = strsep(&sep, "\n")) {
    token = trim(strdup(token));
    line_length = strlen(token);
    if (line_length == 0 || token[0] == '#')
      continue;
    pos = strchr(token, '=');
    if (pos == NULL) {
      printk(KERN_ALERT "Invalid config line!\n");
      return -ECONFIG;
    }
    key = (char*)kzalloc(sizeof(char) * (pos - token + 1), GFP_KERNEL);
    memcpy(key, token, sizeof(char) * (pos - token));
    value = (char*)kzalloc(sizeof(char) * (line_length - (pos - token)), GFP_KERNEL);
    memcpy(value, token + (pos - token) + 1, sizeof(char) * (line_length - (pos - token) - 1));
    if (overwrite) {
      id = temp_config->count;
      temp_config->keys[id] = strdup(trim(key));
      temp_config->values[id] = strdup(trim(value));
      ++temp_config->count;
      if (temp_config->count >= temp_config->capacity)
        grow_config_capacity(temp_config);
    } else
      set_global_config(trim(key), trim(value));
    kfree(key);
    kfree(value);
    kfree(token);
  }
  if (overwrite) {
    free_config(global_config);
    global_config = temp_config;
  }
  return 0;
}

const char *get_global_config(const char *key) {
  return get_config(global_config, key);
}

void set_global_config(const char *key, const char *value) {
  return set_config(global_config, key, value);
}

u8 is_blocked_stream(int id) {
  char buffer[30];
  sprintf(buffer, "block_stream_%d", id);
  return strcmp(get_global_config(buffer), "1") == 0;
}
