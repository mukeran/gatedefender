#ifndef _MODULE_PROC_H
#define _MODULE_PROC_H

#include <linux/proc_fs.h>

#define PROC_DIR_NAME "gatedefender"

#define EPROCDIR 10001
#define EPROCCONFIG 10002
#define EPROCSTREAMS 10003
#define EPROCSTREAMDATA 10004

extern struct proc_dir_entry *proc_base_dir;
extern struct proc_dir_entry *proc_config_file;

int init_proc(void);

#endif
