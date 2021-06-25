#include <linux/kernel.h>
#include <linux/module.h>

MODULE_LICENSE("GPL");

int test;
char *str = "123456";

int init_module(void) {
  printk(KERN_INFO "Hello world!\n");
  return 0;
}
