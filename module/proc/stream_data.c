#include "streams.h"

#include <linux/uaccess.h>

#include "../stream.h"
#include "../utils.h"

ssize_t stream_data_read(struct file *filp, char *buffer, size_t length, loff_t *offset) {
  int id, stream_data_length, size_to_read = 0;
  char *stream_data;
  sscanf(filp->f_path.dentry->d_iname, "%d", &id);
  stream_data_length = get_stream_data(id, &stream_data);
  if (*offset >= stream_data_length)
    goto stream_data_read_return;
  size_to_read = _min(stream_data_length - *offset, length);
  if (copy_to_user(buffer, stream_data + *offset, size_to_read) != 0) {
    size_to_read = -EFAULT;
    goto stream_data_read_return;
  }
stream_data_read_return:
  kfree(stream_data);
  return size_to_read;
  return 0;
}

int stream_data_open(struct inode *inode, struct file *file) {
  try_module_get(THIS_MODULE);
  return 0;
}

int stream_data_release(struct inode *inode, struct file *file) {
  module_put(THIS_MODULE);
  return 0;
}
