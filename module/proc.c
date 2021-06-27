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

static int create_stream_data_proc(int id) {
  char buffer[20];
  struct proc_dir_entry *proc_stream_data_file;
  sprintf(buffer, "%d", id);
  proc_stream_data_file = proc_create(buffer, 0644, proc_stream_dir, &stream_data_ops);
  if (proc_stream_data_file == NULL)
    return -EPROCSTREAMDATA;
  return 0;
}

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
