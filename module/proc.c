/**
 * proc.c - Create config/stream/streams in /proc | 创建 proc 文件系统中查看/修改配置的文件 config，查看流列表的文件 streams，以及显示流内容的 stream 目录及其文件
 * @author mukeran
 */
#include "proc.h"

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include "stream.h"
#include "proc/config.h"
#include "proc/streams.h"
#include "proc/stream_data.h"

#define UPDATE_INTERVAL 1000

struct proc_dir_entry *proc_base_dir;
struct proc_dir_entry *proc_config_file;
struct proc_dir_entry *proc_streams_file;
struct proc_dir_entry *proc_stream_dir;
struct task_struct *proc_stream_data_task;

static struct file_operations config_ops = {
    .read = config_read,
    .write = config_write,
    .open = config_open,
    .release = config_release,
};

static struct file_operations streams_ops = {
    .read = streams_read,
    .open = streams_open,
    .release = streams_release,
};

static struct file_operations stream_data_ops = {
  .read = stream_data_read,
  .open = stream_data_open,
  .release = stream_data_release,
};

/**
 * Create stream data proc /proc/stream/<:id>
 * 创建显示流内容的 proc 文件 /proc/stream/<:id>
 */
static int create_stream_data_proc(int id) {
  char buffer[20];
  struct proc_dir_entry *proc_stream_data_file;
  sprintf(buffer, "%d", id);
  proc_stream_data_file = proc_create(buffer, 0644, proc_stream_dir, &stream_data_ops);
  if (proc_stream_data_file == NULL)
    return -EPROCSTREAMDATA;
  return 0;
}

/**
 * Kernel thread loop that updates /proc/stream/<:id> periodly
 * 用来动态创建 /proc/stream 目录中流文件的循环，在另一线程启动（原因：proc_create 中用到了 kmalloc，但是没有配置 GPF_ATOMIC。如果在捕获到网络流量包的上下文调用，由于 dru 的限制，会导致申请内存失败，并报错。所以需要将 proc_create 的过程放在另外一个非网络流量包上下文执行）
 */
static int proc_stream_data_update_loop(void *data) {
  int i, last = 0, count;
  while (true) {
    msleep(UPDATE_INTERVAL);
    count = stream_count;
    if (count > last) {
      for (i = last; i < count; ++i)
        create_stream_data_proc(streams[i]->id);
      last = count;
    }
  }
  return 0;
}

/**
 * Initialize gatedefender proc filesystem
 * 初始化 gatedefender proc 文件系统
 */
int init_proc(void) {
  /* Create /proc/gatedefender */
  proc_base_dir = proc_mkdir(PROC_DIR_NAME, NULL);
  if (proc_base_dir == NULL) {
    printk(KERN_ERR "Failed to create /proc/gatedefender");
    return -EPROCDIR;
  }
  /* Make config file /proc/gatedefender/config */
  proc_config_file = proc_create("config", 0666, proc_base_dir, &config_ops);
  if (proc_config_file == NULL) {
    printk(KERN_ERR "Failed to create /proc/gatedefender/config");
    return -EPROCCONFIG;
  }
  /* Make streams file /proc/gatedefender/streams */
  proc_streams_file = proc_create("streams", 0644, proc_base_dir, &streams_ops);
  if (proc_streams_file == NULL) {
    printk(KERN_ERR "Failed to create /proc/gatedefender/streams");
    return -EPROCSTREAMS;
  }
  /* Create /proc/gatedefender/stream */
  proc_stream_dir = proc_mkdir("stream", proc_base_dir);
  if (proc_stream_dir == NULL) {
    printk(KERN_ERR "Failed to create /proc/gatedefender/stream");
    return -EPROCSTREAMDATA;
  }
  proc_stream_data_task = kthread_create(&proc_stream_data_update_loop, NULL, "proc_stream_data_update_loop");
  wake_up_process(proc_stream_data_task);
  return 0;
}
