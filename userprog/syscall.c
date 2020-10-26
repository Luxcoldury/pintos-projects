#include "userprog/syscall.h"
#include "lib/user/syscall.h"     // for sysCall functions in handler
#include "userprog/pagedir.h"     // for check_pointer()
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler(struct intr_frame *);

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}


static void
syscall_handler(struct intr_frame *f UNUSED)
{
  printf("system call!\n");
  check_pointer(f); // 确保拿到了一个valid pointer
  int *sp = (int *)f->esp;
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
    f->eax = exec(*(sp + 1));
    break;
  case 3:
    f->eax = wait(*(sp + 1));
    break;
  case 4:
    f->eax = create(*(sp + 1), *(sp + 2));
    break;
  case 5:
    f->eax = remove(*(sp + 1));
    break;
  case 6:
    f->eax = open(*(sp + 1));
    break;
  case 7:
    f->eax = filesize(*(sp + 1));
    break;
  case 8:
    f->eax = read(*(sp+1), *(sp+2), *(sp+3));
    break;
  case 9:
    f->eax = write(*(sp+1), *(sp+2),*(sp+3));
    break;
  case 10:
    seek(*(sp+1), *(sp+2));
    break;
  case 11:
    f->eax = tell(*(sp + 1));
    break;
  case 12:
    close(*(sp + 1));
    break;
  }
  
  thread_exit(); // terminate the thread
}

// the only finished function below
void halt(void){
  shutdown_power_off();
}

void exit(int status){
  // UNFINISHED!!!
  // "Do not print these messages when a kernel thread that is not a
  // user process terminates, or when the halt system call is invoked. The message is optional
  // when a process fails to load."
  char* name = thread_current ()->name;
  printf ("%s:exit(%d)\n", name, status);
  thread_exit ();
}

pid_t exec(const char *file){
  return 0;
}

int wait(pid_t t UNUSED){
  return 0;
}

bool create(const char *file, unsigned initial_size){
  return false;
}

bool remove(const char *file){
  return false;
}

int open(const char *file){
  return 0;
}

int filesize(int fd){
  return 0;
}

int read(int fd, void *buffer, unsigned length){
  return 0;
}

int write(int fd, const void *buffer, unsigned length){
  return 0;
}

void seek(int fd, unsigned position){

}

unsigned tell(int fd){
  return 0;
}

void close(int fd){

}