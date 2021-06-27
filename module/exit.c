/**
 * exit.c - Linux kernel module cleanup function (not considered)
 * @author mukeran
 */
#include <linux/kernel.h>
#include <linux/module.h>

void cleanup_module(void) {
  printk(KERN_INFO "GateDefender unloaded!\n");
}
