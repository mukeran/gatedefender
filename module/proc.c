#include "proc.h"

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "proc/config.h"
#include "proc/streams.h"

struct proc_dir_entry *proc_base_dir;
struct proc_dir_entry *proc_config_file;
struct proc_dir_entry *proc_streams_file;
struct proc_dir_entry *proc_stream_dir;

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

static void create_stream_proc(void) {

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
  create_stream_proc();
  return 0;
}
