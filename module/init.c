/**
 * init.c - Linux kernel module entrypoint | LKM 入口（初始化各个功能，启动流量捕获）
 * @author mukeran
 */
#include <linux/kernel.h>
#include <linux/module.h>

#include "capture.h"
#include "config.h"
#include "proc.h"
#include "stream.h"

MODULE_LICENSE("GPL");

int init_module(void) {
  init_global_config();
  init_streams();
  init_proc();
  register_capture();
  printk(KERN_INFO "GateDefender loaded!\n");
  return 0;
}
