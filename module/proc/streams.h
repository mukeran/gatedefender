/**
 * proc/streams.h - Header for proc/streams.c
 * @author mukeran
 */
#ifndef _MODULE_PROC_STREAMS_H
#define _MODULE_PROC_STREAMS_H

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>

ssize_t streams_read(struct file *filp, char *buffer, size_t length, loff_t *offset);
int streams_open(struct inode *inode, struct file *file);
int streams_release(struct inode *inode, struct file *file);

#endif
