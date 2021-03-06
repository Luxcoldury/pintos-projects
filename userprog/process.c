#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

/* p2.1: 
   Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy=NULL;
  tid_t tid;

  // strtok_r会修改原指针的内容
  // 只对file_name做strtok_r
  // fn_copy保留不变
  char *func=NULL, *save_ptr=NULL;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  func = palloc_get_page (0);
  if (func == NULL){
    palloc_free_page(fn_copy);
    return TID_ERROR;
  }

  strlcpy (func, file_name, PGSIZE);
  func = strtok_r (func, " ", &save_ptr);

  struct process_control_block *pcb = palloc_get_page(0);
  if (pcb==NULL){
    palloc_free_page(fn_copy);
    palloc_free_page(func);
    return TID_ERROR;
  }

  pcb->pid = PID_INITIALIZING;
  pcb->parent_thread = thread_current();
  pcb->file_name = fn_copy;
  pcb->waiting = false;
  pcb->exited = false;
  pcb->parent_dead = false;
  pcb->return_status = -1;

  sema_init(&pcb->sema_thread_created, 0);
  sema_init(&pcb->sema_being_waited_by_father, 0);

  /* Create a new thread to execute FILE_NAME(p2.1: now changed to `FUNC`). */
  tid = thread_create (func, PRI_DEFAULT, start_process, pcb); // fn_cpoy即"exe arg arg..."原样传给start_process
  if (tid == TID_ERROR){
    palloc_free_page(func);
    palloc_free_page(fn_copy); 
    palloc_free_page(pcb);
    return TID_ERROR;
  }

  sema_down(&pcb->sema_thread_created);
  palloc_free_page(func);

  // 把子进程加到列表里
  if(pcb->pid>=0){
    list_push_back(&thread_current()->child_thread_pcb_list,&pcb->elem);
  }

  // if (fn_copy) palloc_free_page(fn_copy);
  return pcb->pid;
}

/* A thread function that loads a user process and starts it
   running.
   对应到`thread_creats()`里面就是kf的部分， kf->function(kf->aux)
   这里的 file_name_ 即`process_create()`里面传入的`fn_copy` "exe arg arg..."
*/
static void
start_process (void *pcb_)
{
  struct process_control_block *pcb = pcb_;
  char* exe_name = pcb->file_name;
  char* argvs;
  exe_name = strtok_r (exe_name, " ", &argvs);
  // 这一步分离完之后，exe_name是"exe"，argvs是"arg arg arg..."

  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  // printf("\n\n----exename:%s----\n\n",exe_name);
  // printf("\n\n----argvs:%s----\n\n",argvs);

  success = load (exe_name, &if_.eip, &if_.esp); // 传给load的只有exe本身

  /* If load failed, quit. */
  if (!success) 
    goto finish;
  // p2.1: if syccess, parsing arguments to stack
  int argc = 0;
  // arg_vp: array of argument variable ponters (char*)
  char *sp = (char*) if_.esp, *arg_vps[256];

  // use args[256] to reverse arguments
  char *args[256];  
  args[argc++] = exe_name;
  for (char *argv = strtok_r (NULL, " ", &argvs); argv != NULL;
        argv = strtok_r (NULL, " ", &argvs)){
          args[argc++] = argv;
        }

  // push exe_name, begin from PHY_BASE-1
  
  // push argvs
  // sp-=1;
  for (int i = argc-1; i >=0; i--){
    sp -= strlen(args[i])+3;
    arg_vps[i] = sp;
    strlcpy(sp, args[i], strlen(args[i])+1);
    // printf("\n sp: %s, &sp: %p // arg[%d]\n", sp, sp, i);
  }
    
  // uint8_t 0
  // word align
  while(((int)sp) % 4)
    sp--;
  // *(uint8_t*)sp = (uint8_t)0;
  // printf("\n &sp: %p // word align\n", sp);

  // char*
  // last arg_ptr = 0
  sp-=sizeof (char*);
  *(char**)sp = (char*)0;
  // printf("\n sp: %s, &sp: %p // char* last arg_ptr\n", sp, sp);

  // char* 
  // push arg_vps: argument pointers
  for(int i=argc-1; i>=0; i--){
    sp -= sizeof(char*);
    *(char**)sp = arg_vps[i];
    // printf("\n sp: %p, at : %p // arg[%d]= %s\n", *(char**)sp, sp, i, *(char**)sp);
  }

  // char** 
  // 4 bytes upward
  sp-=sizeof(char**);
  *((char***)sp) = (char**)(sp + sizeof(char**));
  // printf("\n sp: %p, &sp: %p // char** &arg[0]\n", *(char**)sp, sp);
  
  // int 
  // argc
  sp-=sizeof(int);
  *(int*)sp=argc;
  // printf("\n sp: %d, &sp: %p // argc\n",*sp, sp);
  
  // void* 
  // rd
  sp-=sizeof(int);
  *(void**)sp=0;
  if_.esp = sp;
  // hex_dump((uintptr_t)if_.esp, if_.esp, 128, true);

  finish:
  /*但是不管成不成功都要释放之前分配的1page内存所以传入fn_copy */
  palloc_free_page (exe_name);
  pcb->pid = success ? (pid_t)(thread_current()->tid) : PID_ERROR;
  thread_current()->pcb = pcb;
  sema_up(&pcb->sema_thread_created);

  if (!success)
    exit (-1);

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

// void printlist(struct list* l)
// {
//   for (struct list_elem* e = list_begin(l); e != list_end(l); e = list_next(e))
//   {
//     struct thread* t = list_entry(e, struct thread, child_thread_elem);
//     printf("%d %s %d \n",t->tid,t->name,t->status);
//   }
// }

/* p2.1: 如果是儿子，爸爸等儿子死了再return -1
   Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid UNUSED) 
{
  // p2.6: infinite loop, or wait forever
  // printf("process %d wait %d \n", thread_tid(), child_tid);
  struct list* child_list = &thread_current()->child_thread_pcb_list;
  struct list_elem* temp_elem;
  struct process_control_block* temp_pcb;
  int found=0;

//  printlist(child_list);

  if (list_empty(child_list)) return -1;

  for(temp_elem=list_begin(child_list);temp_elem!=list_end(child_list);temp_elem=list_next(temp_elem)){
    temp_pcb = list_entry(temp_elem,struct process_control_block,elem);
    if(temp_pcb->pid==child_tid){
      found = 1;
      break;
    }
  }

  if (!found || temp_pcb->waiting)
    return -1;

  temp_pcb->waiting=true;

  if(!temp_pcb->exited){
    sema_down(&temp_pcb->sema_being_waited_by_father);
  }

  int exit_status = temp_pcb->return_status;
  list_remove(temp_elem);
  palloc_free_page(temp_pcb);
  // printf("process wait done\n");
  return exit_status;

}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  while (!list_empty(&cur->file_descriptor_list)) {
    struct list_elem *e = list_pop_front (&cur->file_descriptor_list);
    struct file_descriptor *f = list_entry (e, struct file_descriptor, elem);
    file_close(f->file);
    palloc_free_page(f);
  }
  
  if(cur->owner_file){
    file_allow_write(cur->owner_file);
    file_close (cur->owner_file);
  }

  while (!list_empty(&cur->child_thread_pcb_list)) {
    struct list_elem *e = list_pop_front (&cur->child_thread_pcb_list);
    struct process_control_block *pcb;
    pcb = list_entry(e, struct process_control_block, elem);
    if (pcb->exited == true) {
      palloc_free_page (pcb);
    } else {
      pcb->parent_dead = true;//爸爸先死了_(:з」∠)_
      pcb->parent_thread = NULL;
    }
  }

  cur->pcb->exited = true;
  bool parent_dead = cur->pcb->parent_dead;
  sema_up(&cur->pcb->sema_being_waited_by_father);

  if (parent_dead)
    palloc_free_page(&cur->pcb);

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL)
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;
  /* deny writes to exacutables */
  file_deny_write(file);
  thread_current()->owner_file = file;

 done:
  /* We arrive here whether the load is successful or not. */
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}


/* As for the `suggested order` in the Official Document:
  p2.1: argument parsing
  p2.2: user memo access
  p2.3: System call infrastructure
  p2.4: The `exit` system call
  p2.5: The `write` system call for writing to fd 1, the system console
  p2.6: change `process_wait()` to an infinite loop
 */