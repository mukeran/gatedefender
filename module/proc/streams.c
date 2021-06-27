/**
 * proc/streams.c - /proc/gatedefender/streams file operations
 * @author mukeran
 */
#include "streams.h"

#include <linux/uaccess.h>

#include "../stream.h"
#include "../utils.h"

ssize_t streams_read(struct file *filp, char *buffer, size_t length, loff_t *offset) {
  char *streams_info;
  int streams_length = display_streams(&streams_info), size_to_read = 0;
  if (*offset >= streams_length)
    goto stream_read_return;
  size_to_read = _min(streams_length - *offset, length);
  if (copy_to_user(buffer, streams_info + *offset, size_to_read) != 0) {
    size_to_read = -EFAULT;
    goto stream_read_return;
  }
stream_read_return:
  kfree(streams_info);
  return size_to_read;
  return 0;
}

int streams_open(struct inode *inode, struct file *file) {
  try_module_get(THIS_MODULE);
  return 0;
}

int streams_release(struct inode *inode, struct file *file) {
  module_put(THIS_MODULE);
  return 0;
}
