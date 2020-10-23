#include "userprog/syscall.h"
#include "lib/user/syscall.h"
#include "userprog/pagedir.c"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler(struct intr_frame *);

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

// 2.2: handle access to user memory
void check_pointer(void *vaddr)
{
  // invalid pointers: `null pointer, ptr to unmapped virtual memory, ptr to kernel vm`
  // are rejected by `terminating the offending process & freeing the resources`
  if (vaddr == NULL || lookup_page(active_pd(), vaddr, false) || is_kernel_vaddr(vaddr))
  {
    thread_exit();
  }
  // Q: 怎么free recources? 退出之后就默认free了吗？
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
  printf("system call!\n");
  check_pointer(f); // 确保拿到了一个valid pointer
  int *sp = (int *)f.esp;
  int sysCallNum = *sp;
  // `sysCallNum`is stored in stack pointer(f.esp)
  // return by modifying `f.eax`
  switch (sysCallNum)
  {
  case 0:
    halt();
  case 1:
    exit(*(sp + 1));
    break;
  case 2:
    f.eax = exec(*(sp + 1));
    break;
  case 3:
    f.eax = wait(*(sp + 1));
    break;
  case 4:
    f.eax = create(*(sp + 1), *(sp + 2));
    break;
  case 5:
    f.eax = remove(*(sp + 1));
    break;
  case 6:
    f.eax = open(*(sp + 1));
    break;
  case 7:
    f.eax = filesize(*(sp + 1));
    break;
  case 8:
    f.eax = read(*(sp+1), *(sp+2), *(sp+3));
    break;
  case 9:
    f.eax = write(*(sp+1), *(sp+2),*(sp+3));
    break;
  case 10:
    f.eax = seek(*(sp+1), *(sp+2));
    break;
  case 11:
    f.eax = tell(*(sp + 1));
    break;
  case 12:
    f.eax = close(*(sp + 1));
    break;
  }
  else : 
    thread_exit(); // terminate the thread
}

void halt(void) NO_RETURN;
void exit(int status) NO_RETURN;
pid_t exec(const char *file);
int wait(pid_t);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned length);
int write(int fd, const void *buffer, unsigned length);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);