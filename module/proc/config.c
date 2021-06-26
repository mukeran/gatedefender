#include "config.h"

#include "../config.h"

#include <linux/uaccess.h>

ssize_t config_read(struct file *filp, char *buffer, size_t length, loff_t *offset) {
  char *config_data;
  int config_length = serialize_global_config(&config_data), size_to_read = 0;
  if (*offset >= config_length)
    goto config_read_return;
  size_to_read = _min(config_length - *offset, length);
  if (copy_to_user(buffer, config_data + *offset, size_to_read) != 0) {
    size_to_read = -EFAULT;
    goto config_read_return;
  }
config_read_return:
  kfree(config_data);
  return size_to_read;
}

ssize_t config_write(struct file *filp, const char *buffer, size_t length, loff_t *offset) {
  int size_to_write = length;
  if (unserialize_global_config(buffer, length, 0)) {
    size_to_write = -ECONFIG;
    goto config_write_return;
  }
config_write_return:
  return size_to_write;
}

int config_open(struct inode *inode, struct file *file) {
  try_module_get(THIS_MODULE);
  return 0;
}

int config_release(struct inode *inode, struct file *file) {
  module_put(THIS_MODULE);
  return 0;
}
