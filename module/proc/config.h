/**
 * proc/config.h - Header for proc/config.c
 * @author mukeran
 */
#ifndef _MODULE_PROC_CONFIG_H
#define _MODULE_PROC_CONFIG_H

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>

ssize_t config_read(struct file *filp, char *buffer, size_t length, loff_t *offset);
ssize_t config_write(struct file *filp, const char *buffer, size_t length, loff_t *offset);
int config_open(struct inode *inode, struct file *file);
int config_release(struct inode *inode, struct file *file);

#endif
