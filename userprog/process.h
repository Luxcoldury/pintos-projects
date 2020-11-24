#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

typedef tid_t pid_t;

#define PID_ERROR         ((pid_t) -1)
#define PID_INITIALIZING  ((pid_t) -2)

/* PCB : see initialization at process_execute(). */
struct process_control_block {

  pid_t pid;                /* The pid of process */

  const char* file_name;      /* The command line of this process being executed */

  struct list_elem elem;    /* element for thread.child_list */
  struct thread* parent_thread;    /* the parent process. */

  bool waiting;             /* indicates whether parent process is waiting on this. */
  bool exited;              /* indicates whether the process is done (exited). */
  bool parent_dead;              /* indicates whether the parent process has terminated before. */
  int32_t return_status;         /* the exit code passed from exit(), when exited = true */

  /* Synchronization */
  struct semaphore sema_thread_created;   /* the semaphore used between start_process() and process_execute() */
  struct semaphore sema_being_waited_by_father;             /* the semaphore used for wait() : parent blocks until child exits */

};


// the nunnegative int handle file descriptor
struct file_descriptor{
  struct list_elem elem;  // for list operation

  int fd;                 // nonnegative int fd (0, 1 reserved)
  struct file *file;      // the opened file
};


#endif /* userprog/process.h */
