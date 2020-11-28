#include "userprog/syscall.h"
#include "lib/user/syscall.h" // for sysCall functions in handler
#include "userprog/pagedir.h" // for check_pointer()
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "devices/shutdown.h"   // for shutdown_power_off
#include "filesys/file.h"       // syscall
#include "filesys/filesys.h"    // syscall
#include "lib/kernel/list.h"    // for fd list 
#include "userprog/process.h"
#include "devices/input.h"
#include "threads/vaddr.h"      // for is_kernel_vaddr() to check ptr

/* for syscall in P2, P3 */
#define SYS_CALL_NUM_MIN 0
#ifdef VM
  #include "vm/swap.h"
  #include "vm/frame.h"
  #include "vm/page.h"
  #define SYS_CALL_NUM_MAX 14
#elif USERPROG
  #define SYS_CALL_NUM_MAX 12
#endif


static void syscall_handler(struct intr_frame *);

struct lock filesys_lock;

/* helper functions at the bottom. */
struct file_descriptor*
find_file_descriptor_by_fd(int fd);
struct mmap_descriptor*
find_mmap_descriptor_by_md(mapid_t md);

void syscall_init(void)
{
  lock_init (&filesys_lock);
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
  // printf("system call!\n");

  // get sysCall number and validate it
  int *sp = (int *)f->esp;
  if(!check_pointer(sp)){
    exit(-1);
  }
  int sysCallNum = *sp;
  ASSERT(sysCallNum >= SYS_CALL_NUM_MIN && sysCallNum <= SYS_CALL_NUM_MAX);

  #ifdef VM
  thread_current()->kernel_esp_temp = (uint8_t*)sp;
  #endif

  // `sysCallNum`is stored in stack pointer(f.esp)
  // sysCall function returns by modifying `f.eax`
  switch (sysCallNum)
  {
  case SYS_HALT:
    halt();
    break;

  case SYS_EXIT:
    if(!check_pointer(sp+1) || !is_user_vaddr (sp+1)){
      exit(-1);
    }
    exit(*(sp + 1));
    break;

  case SYS_EXEC:
    if(!check_pointer(sp+1) || !is_user_vaddr (sp+1)){
      exit(-1);
    }
    f->eax = exec((char*)*(sp + 1));
    break;

  case SYS_WAIT:
    if(!check_pointer(sp+1) || !is_user_vaddr (sp+1)){
      exit(-1);
    }
    f->eax = wait(*(sp + 1));
    break;

  case SYS_CREATE:
    if(!check_pointers(sp+1, 2) || !is_user_vaddr (sp+1)){
      exit(-1);
    }
    f->eax = create((char*)*(sp + 1), *(sp + 2));
    break;

  case SYS_REMOVE:
    if(!check_pointer(sp+1) || !is_user_vaddr (sp+1)){
      exit(-1);
    }
    f->eax = remove((char*)*(sp + 1));
    break;

  case SYS_OPEN:
    if(!check_pointer(sp+1) || !is_user_vaddr (sp+1)){
      exit(-1);
    }
    f->eax = open((char*)*(sp + 1));
    break;

  case SYS_FILESIZE:
    if(!check_pointer(sp+1) || !is_user_vaddr (sp+1)){
      exit(-1);
    }
    f->eax = filesize(*(sp + 1));
    break;

  case SYS_READ:
    // printf("check read ptr!\n");
    if (!check_pointers(sp+1, 3)|| !is_user_vaddr (sp+1)){
      exit(-1);
    }
    f->eax = read(*(sp + 1), (void*)*(sp + 2), *(sp + 3));
    break;

  case SYS_WRITE:
    if(!check_pointers(sp+1, 3) || !is_user_vaddr (sp+1)){
      exit(-1);
    }
    f->eax = write(*(sp + 1), (void*)*(sp + 2), *(sp + 3));
    break;

  case SYS_SEEK:
    if(!check_pointers(sp+1, 2) || !is_user_vaddr (sp+1)){
      exit(-1);
    }
    seek(*(sp + 1), *(sp + 2));
    break;

  case SYS_TELL:
    if(!check_pointer(sp+1) || !is_user_vaddr (sp+1)){
      exit(-1);
    }
    f->eax = tell(*(sp + 1));
    break;

  case SYS_CLOSE:
    if(!check_pointer(sp+1) || !is_user_vaddr (sp+1)){
      exit(-1);
    }
    close(*(sp + 1));
    break;

  case SYS_MMAP:
    if(!check_pointers(sp+1, 2) || !is_user_vaddr (sp+1) || !is_user_vaddr (sp+2)){
      exit(-1);
    }
    f->eax = mmap(*(sp + 1), (void*)*(sp + 2));
    break;

  case SYS_MUNMAP:
    if(!check_pointer(sp+1) || !is_user_vaddr (sp+1)){
      exit(-1);
    }
    exit(-1);
    // munmap(*(sp + 1));
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
  thread_current ()->pcb->return_status = status;
  
  /* Do not print when a kernel thread that is not a
  user process terminates, or when the halt system call is invoked. */
  // if(!thread_current ()->halted && thread_tid() != 1){
  printf("%s: exit(%d)\n", thread_current()->name, status);
  // }

  thread_exit();
}

/* Runs the executable whose name is given in cmd_line, 
   passing any given arguments, and returns the new process’s program ID (pid). 
   Must return pid -1 if the program cannot load or run for any reason. 
   Thus, the parent process cannot return from the exec 
   until it knows whether the child process successfully loaded its executable. 
   You must use appropriate synchronization to ensure this. */
pid_t exec(const char *cmd_line UNUSED)
{
  lock_acquire (&filesys_lock);
  pid_t pid = process_execute(cmd_line);
  lock_release(&filesys_lock);
  return pid;
  // return -1;
}

/* Waits for a child process pid and retrieves the child’s exit status. */
int wait(pid_t pid UNUSED)
{
  return process_wait(pid);
  // return -1;
}

/* Creates a new file called file initially initial_size bytes in size. 
   Returns true if successful, false otherwise. 
   Creating a new file does not open it: 
   opening the new file is a separate operation which would require a `open` system call. */
bool create(const char *file, unsigned initial_size)
{
  if (!check_pointer(file)){
    exit(-1);
  }
  lock_acquire (&filesys_lock);
  bool create = filesys_create(file, initial_size);
  lock_release(&filesys_lock);

  return create;
}

/* Deletes the file called file.
   Returns true if successful, false otherwise. 
   A file may be removed regardless of whether it is open or closed, 
   and removing an open file does not close it. */
bool remove(const char *file)
{
  if (file==NULL){
    exit(-1);
  }
  lock_acquire (&filesys_lock);
  bool remove = filesys_remove(file);
  lock_release(&filesys_lock);

  return remove;
}

/* Opens the file called file. 
   Returns a nonnegative integer handle called a "file descriptor"(fd),
   or -1 if the file could not be opened. */
int open(const char *file)
{
  struct file_descriptor* f = palloc_get_page(0);
  if(f == NULL){/* malloc fail */
    return -1;
  }

  // printf ("check opening file ptr!\n");
  if (!check_pointer(file)){
    palloc_free_page(f);
    exit(-1);
  }
  lock_acquire (&filesys_lock);
  struct file * opened_file = filesys_open(file);
  lock_release(&filesys_lock);
  
  if(opened_file==NULL){
    palloc_free_page(f);
    return -1;
  } 
  // if(is_file_executable(opened_file)){
  //   file_deny_write(opened_file);
  // }
  // else successfully open
  // add file_descriptor
  f->fd = thread_current()->fileNum_plus2++;
  f->file = opened_file;
  list_push_front(&thread_current()->file_descriptor_list, &f->elem);

  return f->fd;
}

/* Returns the size, in bytes, of the file open as fd. */
int filesize(int fd)
{
  lock_acquire (&filesys_lock);
  struct file_descriptor* f = find_file_descriptor_by_fd(fd);
  if(f==NULL){
    lock_release(&filesys_lock);
    return -1;
  }

  int f_size = file_length(f->file);
  lock_release(&filesys_lock);

  return f_size;
}

/* Reads size bytes from the file open as fd into buffer. 
   Returns the number of bytes actually read (0 at end of file), 
   or -1 if the file could not be read (due to a condition other than end of file). 
   Fd 0 reads from the keyboard using input_getc(). */
int read(int fd, void *buffer, unsigned length)
{
  if (!(check_pointer(buffer) && check_pointer((char*)buffer+length-1) && is_user_vaddr (buffer) && is_user_vaddr ((char*)buffer+length-1))){
    exit(-1);
  }

  lock_acquire (&filesys_lock);

  int read_bytes = 0 ;
  // 输入流
  if (fd == STDIN_FILENO){
    uint8_t *buf = buffer;
    for(unsigned int i = 0 ; i < length ; i++){
      buf[i] = input_getc();
      read_bytes++;
    }
    lock_release(&filesys_lock);
    return read_bytes;
  }
    
  // 输出流
  if (fd == STDOUT_FILENO){ 
    lock_release(&filesys_lock);
    return -1;
  }
  // 自己打开的 file, found by `fd` and thread_current()->file_descriptor_list
  struct file_descriptor* f = find_file_descriptor_by_fd (fd);
  if (f == NULL || f->file == NULL){
    lock_release(&filesys_lock);
    return -1;
  }
  
  read_bytes = file_read (f->file, buffer, length);
  lock_release(&filesys_lock);
  return read_bytes;
}

/* Writes size bytes from buffer to the open file fd. 
   Returns the number of bytes actually written, 
   which may be less than size if some bytes could not be written. */
int write(int fd, const void *buffer, unsigned length)
{
  // printf("check write ptr!\n");
  if (!(check_pointer(buffer) && check_pointer((char*)buffer+length-1)&& is_user_vaddr (buffer) && is_user_vaddr ((char*)buffer+length-1))){
    exit(-1);
  }

  // 输入流
  if (fd == STDIN_FILENO)
    return -1;
  lock_acquire (&filesys_lock);
  // 输出流
  if (fd == STDOUT_FILENO)
    { 
      putbuf ((char *)buffer, (size_t)length);
      lock_release (&filesys_lock);
      return length;
    }
  // 自己打开的 file, found by `fd` and thread_current()->file_descriptor_list
  struct file_descriptor* f = find_file_descriptor_by_fd (fd);
  if (f == NULL || f->file == NULL){
    lock_release(&filesys_lock);
    return -1;
  }
  
  int ret = file_write (f->file, buffer, length);
  lock_release(&filesys_lock);

  return ret;
}

/* Changes the next byte to be read or written in open file fd to position, 
   expressed in bytes from the beginning of the file. 
   (Thus, a position of 0 is the file’s start.) */
void seek(int fd, unsigned position)
{
  lock_acquire (&filesys_lock);

  struct file_descriptor* f = find_file_descriptor_by_fd(fd);
  
  if (!(f == NULL || f->file == NULL)){
    file_seek(f->file, position);
  }
  lock_release(&filesys_lock);
  return;
  
}

/* Returns the position of the next byte to be read or written in open file fd,
   expressed in bytes from the beginning of the file. */
unsigned tell(int fd)
{
  lock_acquire (&filesys_lock);
  struct file_descriptor* f = find_file_descriptor_by_fd(fd);

  if (f == NULL || f->file == NULL){
    lock_release(&filesys_lock);
    return -1;
  }
  unsigned ret = file_tell(f->file);
  lock_release(&filesys_lock);
  return ret;
}

/* Closes file descriptor fd. 
   Exiting or terminating a process implicitly closes all its open file descriptors,
   as if by calling this function for each one. */
void close(int fd)
{
  lock_acquire (&filesys_lock);
  struct file_descriptor* f = find_file_descriptor_by_fd(fd);
  if (f == NULL || f->file == NULL){
    lock_release(&filesys_lock);
    return;
  }

  file_close(f->file);
  list_remove(&f->elem);
  palloc_free_page(f);
  lock_release(&filesys_lock);
  return;
}


mapid_t mmap(int fd, void *addr){
  // stdin,stdout,指针无效,指针非page
  if (fd < 2 || addr == NULL || pg_ofs(addr) != 0) return MAP_FAILED;
  struct thread *cur = thread_current();

  lock_acquire (&filesys_lock);

  struct file_descriptor* file_d = find_file_descriptor_by_fd(fd);
  struct file *f = NULL;
  size_t f_size;

  if(file_d && file_d->file) {
    f = file_reopen (file_d->file);
    f_size = file_length(f);
  }

  if(f == NULL || f_size == 0) goto mmap_not_success;

  for (size_t i = 0; i < f_size; i += PGSIZE) {
    // 原位有其他page
    if (spt_hash_lookup(addr+i)!=NULL) goto mmap_not_success;
  }

  //创建file page
  for (size_t i = 0; i < f_size; i += PGSIZE) {
    size_t file_bytes = (i + PGSIZE < f_size ? PGSIZE : f_size - i);
    spt_create_file_mmap_page(addr, f, i, file_bytes, true);
  }

  //指定md
  mapid_t md;
  if (!list_empty(&cur->mmap_descriptor_list)) {
    md = list_entry(list_back(&cur->mmap_descriptor_list), struct mmap_descriptor, elem)->md + 1;
  }else{
    md = 1;
  }

  struct mmap_descriptor *m = (struct mmap_descriptor*) malloc(sizeof(struct mmap_descriptor));
  m->md = md;
  m->file = f;
  m->addr = addr;
  m->size = f_size;
  list_push_back (&cur->mmap_descriptor_list, &m->elem);

  lock_release (&filesys_lock);
  return md;

  mmap_not_success:

  lock_release (&filesys_lock);
  return MAP_FAILED;
}
/********************** helper functions ***************************************/

/* as the name, return f with a valid file*
   otherwise return NULL */
struct file_descriptor*
find_file_descriptor_by_fd(int fd){
  /* validate fd in [2, thread_current()->fileNum_plus2] */
  if(fd<2 || fd>thread_current()->fileNum_plus2) 
    return NULL;

  struct list* l = &thread_current()->file_descriptor_list;
  if(list_empty(l)) 
    return NULL;

  // printf ("\nsize: %d\n",list_size(&l));
  for (struct list_elem *e = list_begin (l); e != list_end (l); e = list_next (e))
  {
    struct file_descriptor *f = list_entry (e, struct file_descriptor, elem);
      if(f->fd == fd && check_pointer(f->file)){
        return f;
      }    
  }

  return NULL;
}

struct mmap_descriptor*
find_mmap_descriptor_by_md(mapid_t md){
  struct list* l = &thread_current()->mmap_descriptor_list;
  if(list_empty(l)) 
    return NULL;

  // printf ("\nsize: %d\n",list_size(&l));
  for (struct list_elem *e = list_begin (l); e != list_end (l); e = list_next (e))
  {
    struct mmap_descriptor *m = list_entry (e, struct mmap_descriptor, elem);
      if(m->md == md){
        return m;
      }    
  }

  return NULL;
}