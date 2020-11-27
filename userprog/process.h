#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
bool install_page (void *upage, void *kpage, bool writable);

typedef tid_t pid_t;

#define PID_ERROR         ((pid_t) -1)
#define PID_INITIALIZING  ((pid_t) -2)

/* PCB : see initialization at process_execute(). */
struct process_control_block {

  pid_t pid;
  const char* file_name;  // 制行的命令，包含参数
  int32_t return_status;  // 返回值

  struct list_elem elem;  // 爸爸的pcb_list的list_elem
  struct thread* parent_thread;
  struct semaphore sema_thread_created; // start_process完成之后，告诉process_execute可以清理一些变量的内存
  struct semaphore sema_being_waited_by_father; // 爸爸在等你

  bool waiting;
  bool exited;
  bool parent_dead; // 爸爸先走了，自己的后事要自己料理了
};


// the nunnegative int handle file descriptor
struct file_descriptor{
  struct list_elem elem;  // for list operation

  int fd;                 // nonnegative int fd (0, 1 reserved)
  struct file *file;      // the opened file
};

struct mmap_descriptor{
  struct list_elem elem;  // for list operation

  mapid_t md;             // md number (start from 1)
  struct file *file;      // the opened file
  void *addr;             // mmap first page
  size_t size;            // file size
};


#endif /* userprog/process.h */
