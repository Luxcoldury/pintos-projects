#include "userprog/syscall.h"
#include "lib/user/syscall.h" // for sysCall functions in handler
#include "userprog/pagedir.h" // for check_pointer()
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "lib/kernel/list.h"  // for fd list 

#define SYS_CALL_NUM_MIN 0
#define SYS_CALL_NUM_MAX 12

static void syscall_handler(struct intr_frame *);

// the nunnegative int handle file descriptor
struct file_descriptor{
  struct list_elem elem;  // for list operation

  int fd;                 // nonnegative int fd (0, 1 reserved)
  struct file *file;      // the opened file
}

static struct file_descriptor*
find_file_descriptor_by_fd(fd);

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
  printf("system call!\n");

  // 确保拿到了一个valid pointer
  check_pointer(f);

  // get sysCall number and validate it
  int *sp = (int *)f->esp;
  int sysCallNum = *sp;
  ASSERT(sysCallNum >= SYS_CALL_NUM_MIN && sysCallNum <= SYS_CALL_NUM_MAX);
  
  // `sysCallNum`is stored in stack pointer(f.esp)
  // sysCall function returns by modifying `f.eax`
  switch (sysCallNum)
  {
  case SYS_HALT:
    halt();
    break;
  case SYS_EXIT:
    exit(*(sp + 1));
    break;
  case SYS_EXEC:
    f->eax = exec(*(sp + 1));
    break;
  case SYS_WAIT:
    f->eax = wait(*(sp + 1));
    break;
  case SYS_CREATE:
    f->eax = create(*(sp + 1), *(sp + 2));
    break;
  case SYS_REMOVE:
    f->eax = remove(*(sp + 1));
    break;
  case SYS_OPEN:
    f->eax = open(*(sp + 1));
    break;
  case SYS_FILESIZE:
    f->eax = filesize(*(sp + 1));
    break;
  case SYS_READ:
    f->eax = read(*(sp + 1), *(sp + 2), *(sp + 3));
    break;
  case SYS_WRITE:
    f->eax = write(*(sp + 1), *(sp + 2), *(sp + 3));
    break;
  case SYS_SEEK:
    seek(*(sp + 1), *(sp + 2));
    break;
  case SYS_TELL:
    f->eax = tell(*(sp + 1));
    break;
  case SYS_CLOSE:
    close(*(sp + 1));
    break;
  }

  // thread_exit(); // terminate the thread
}

/* Terminates Pintos by calling shutdown_power_off() (declared in threads/init.h). 
   This should be seldom used, because you lose some information about possible deadlock situations, etc. */
void halt(void)
{
  shutdown_power_off();
}

/* Terminates the current user program, returning status to the kernel. If the process’s parent
waits for it (see below), this is the status that will be returned. Conventionally, a status of 0
indicates success and nonzero values indicate errors. */
void exit(int status)
{
  // UNFINISHED!!!
  // "Do not print these messages when a kernel thread that is not a
  // user process terminates, or when the halt system call is invoked. The message is optional
  // when a process fails to load."
  thread_current ()->exit_status = status;
  char *name = thread_current()->name;
  printf("%s:exit(%d)\n", name, status);
  thread_exit();
}

/* Runs the executable whose name is given in cmd_line, passing any given arguments, and
returns the new process’s program ID (pid). Must return pid -1, which otherwise should
not be a valid pid, if the program cannot load or run for any reason. Thus, the parent
process cannot return from the exec until it knows whether the child process successfully
loaded its executable. You must use appropriate synchronization to ensure this. */
pid_t exec(const char *cmd_line UNUSED)
{
  return 0;
}

/* Waits for a child process pid and retrieves the child’s exit status. */
int wait(pid_t pid UNUSED)
{
  return -1;
}

/* Creates a new file called file initially initial_size bytes in size. 
   Returns true if successful, false otherwise. 
   Creating a new file does not open it: 
   opening the new file is a separate operation which would require a `open` system call. */
bool create(const char *file, unsigned initial_size)
{
  check_pointer(file);
  bool create = filesys_create(file, initial_size);

  return create;
}

/* Deletes the file called file.
   Returns true if successful, false otherwise. 
   A file may be removed regardless of whether it is open or closed, 
   and removing an open file does not close it. */
bool remove(const char *file)
{
  check_pointer(file);
  bool remove = filesys_remove(name);

  return remove;
}

/* Opens the file called file. 
   Returns a nonnegative integer handle called a "file descriptor"(fd),
   or -1 if the file could not be opened. */
int open(const char *file)
{
  check_pointer(file);
  struct file * opened_file = filesys_open(file);

  if(opened_file==NULL){
    return -1;
  } 
  // else successfully open
  struct file_descriptor* f = malloc(sizeof(struct file_descriptor));
  if(f == NULL){/* malloc fail */
    file_close(opened_file);
    return -1;
  }
  // add file_descriptor
  f->fd = thread_create()->fileNum_plus2++;
  f->file = opened_file;
  list_push_back(&thread_current()->file_descriptor_list, &f->elem);

  return f->fd;
}

/* Returns the size, in bytes, of the file open as fd. */
int filesize(int fd)
{
  struct file_descriptor* f = find_file_descriptor_by_fd(fd);
  if(f==NULL)
    return -1;
  return 0;
}

/* Reads size bytes from the file open as fd into buffer. 
   Returns the number of bytes actually read (0 at end of file), 
   or -1 if the file could not be read (due to a condition other than end of file). 
   Fd 0 reads from the keyboard using input_getc(). */
int read(int fd, void *buffer, unsigned length)
{
  check_pointer(buffer);
  int read_bytes = 0 ;

  // 输入流
  if (fd == STDIN_FILENO)
    input_getc((char *)buffer, (size_t)length);
    return length;
  // 输出流
  if (fd == STDOUT_FILENO)
    { 
      return -1;
    }
  // 自己打开的 file, found by `fd` and thread_current()->file_descriptor_list
  struct file_descriptor* f = find_file_by_fd (fd);
  if (f == NULL)
    return -1;
  
  read_bytes = file_read (f->file, buffer, length);
  
  return read_bytes;

/* Writes size bytes from buffer to the open file fd. 
   Returns the number of bytes actually written, 
   which may be less than size if some bytes could not be written. */
int write(int fd, const void *buffer, unsigned length)
{
  check_pointer(buffer);
  int written_bytes = 0 ;

  // 输入流
  if (fd == STDIN_FILENO)
    return -1;
  // 输出流
  if (fd == STDOUT_FILENO)
    { 
      putbuf ((char *)buffer, (size_t)length);
      return length;
    }
  // 自己打开的 file, found by `fd` and thread_current()->file_descriptor_list
  struct file_descriptor* f = find_file_by_fd (fd);
  if (f == NULL)
    return -1;
  
  written_bytes = file_write (f->file, buffer, length);
  
  return written_bytes;
}

/* Changes the next byte to be read or written in open file fd to position, 
   expressed in bytes from the beginning of the file. 
   (Thus, a position of 0 is the file’s start.) */
void seek(int fd, unsigned position)
{
}

/* Returns the position of the next byte to be read or written in open file fd, expressed in bytes
from the beginning of the file. */
unsigned tell(int fd)
{
  return 0;
}

/* Closes file descriptor fd. Exiting or terminating a process implicitly closes all its open file
descriptors, as if by calling this function for each one. */
void close(int fd)
{
}

/******************************* bellow helper functions ***************************************/

/* as the name, return f with a valid file*
   otherwise return NULL */
static 
struct file_descriptor*
find_file_descriptor_by_fd(fd){
  struct list *l = thread_current()->file_descriptor_list;
  struct list_elem *e;

  for (e = list_begin (&l); e != list_end (&l); e = list_next (e)){
      struct file_descriptor *f = list_entry (e, struct file_descriptor, elem);
      if(f->fd == fd){
        check_pointer(f->file)
        return f;
      }    
  }

  return NULL;
}