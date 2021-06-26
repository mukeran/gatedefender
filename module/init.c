#include <linux/kernel.h>
#include <linux/module.h>

#include "capture.h"
#include "config.h"
#include "proc.h"
#include "stream.h"

MODULE_LICENSE("GPL");

int test;
char *str = "123456";

int init_module(void) {
  printk(KERN_INFO "Hello world!\n");
  init_global_config();
  init_streams();
  init_proc();
  register_capture();
  return 0;
}
