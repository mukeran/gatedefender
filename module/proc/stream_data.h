#ifndef _MODULE_PROC_STREAM_DATA_H
#define _MODULE_PROC_STREAM_DATA_H

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>

ssize_t stream_data_read(struct file *filp, char *buffer, size_t length, loff_t *offset);
int stream_data_open(struct inode *inode, struct file *file);
int stream_data_release(struct inode *inode, struct file *file);

#endif
