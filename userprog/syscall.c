#include "userprog/syscall.h"
#include "userprog/pagedir.c"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  thread_exit (); // terminate the thread
}

// 2.2: handle access to user memory
void check_pointer(void *buffer){
  // invalid pointers: `null pointer, ptr to unmapped virtual memory, ptr to kernel vm`
  // are rejected by `terminating the offending process & freeing the resources`
  if(buffer == NULL || lookup_page(active_pd(), buffer, false) || is_kernel_vaddr(buffer)){
    thread_exit ();
  }
  // 怎么free recources? 退出之后就默认free了吗？
}

void halt (void) NO_RETURN;
void exit (int status) NO_RETURN;
pid_t exec (const char *file);
int wait (pid_t);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned length);
int write (int fd, const void *buffer, unsigned length);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);