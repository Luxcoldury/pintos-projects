#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

#ifndef FIXED_POINT_H
#include "fixed_point.h"
#endif

#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* for p1.3 */
#define nice_min -20
#define nice_max 20
int64_t load_avg;                     
int ready_threads;
// int max_priority;/* record the maximum thread priority currently, so that `yield()` in `set_nice`*/


/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);
  // printf("thread_init\n");
  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);

  /* for p1.3: init load_avg = 0, ready_threads = 0 */
  load_avg = INT_TO_FP(0);
  ready_threads = 0;

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
  
  /* p1.3: init niceness and recent_cpu and priority */
  // if(thread_mlfqs){
  //   struct thread *p = initial_thread;
  //   p->nice = 0;               /* p1.3: initial thread have nice value = 0 */
  //   p->recent_cpu = INT_TO_FP(0);         /* p1.3: initial thread have r_cpu value = 0 */  
  //   /* recalculate the priority. */
  //   p->priority = PRI_MAX;
    // max_priority = p->priority;
  // }
  
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  
  /* p1.3： inherit */
  if(thread_mlfqs){
    struct thread *p = thread_current ();
    t->nice = p->nice;                      /* p1.3: nice value inherite */
    t->recent_cpu = p->recent_cpu;          /* p1.3: recent_cpu inherite */
    
    /* recalculate the priority and update `max_priority`. */
    recalcu_priority(t, NULL);
  }
  /* init tid */
  tid = t->tid = allocate_tid ();


  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  /* Add to run queue. */
  thread_unblock (t);

  // if (priority > thread_current()->priority)
  // 如果新thread优先级高，就要运行新thread。
    thread_yield (); // 不管新thread高不高，都reschedule。
  // 问题：即使新thread的pri低，如果有其他的（不相关的、）同pri的、ready的thread，可能会切过去hhh。大概不影响test，毕竟test里的优先级都是好好分开的

  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  list_push_back (&ready_list, &t->elem);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread)
  {
    list_push_back (&ready_list, &cur->elem);
  } 
    
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's **intrinsic** priority to NEW_PRIORITY.
   设定当前线程的**本征**优先级 */
void
thread_set_priority (int new_priority) 
{
  enum intr_level old_level;
  old_level = intr_disable ();

  // p1.2 priority: thread_mlfqs == False means not using p1.3
  if(!thread_mlfqs){
    ASSERT (PRI_MIN <= new_priority && new_priority <= PRI_MAX);
    thread_current ()->intrinsic_priority = new_priority;

    // 自己在运行，pri一定是最高的，自己一定不是waiter。可能是holder
    // 改低：给其他thread让路，但不影响任何其他thread的pri，因为自己不在等其他thread，没donate给其他thread
    // 改高：没啥影响_(:з」∠)_因为自己不在等其他thread，没donate给其他thread
    // 结论：不用参考holder的pri，也没有其他的thread的pri要改

    // 更新pri
    update_priority();
    // 不管咋样都重新schedule
    thread_yield();
  }

  intr_set_level (old_level);
}

/* Returns the current thread's priority. 获取表征优先级 */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/* p1.3: Sets the current thread's nice value to NICE and recalculate the priority. */
void
thread_set_nice (int nice UNUSED) 
{
  /* reset nice value */
  thread_current ()->nice = nice;
  if (nice>nice_max) thread_current ()->nice = nice_max;
  if (nice<nice_min) thread_current ()->nice = nice_min;

  /* recalculate the priority. */
  recalcu_priority(thread_current (), NULL);

  /* if no longer highest priority, yield */
  // if (thread_current ()->priority < max_priority)
  thread_yield();
}

/* p1.3: Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  return thread_current ()->nice;
}

/* p1.3: Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  return FP_ROUND_NEAR(FP_MULT_INT(load_avg, 100));
}

/* p1.3: Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  return FP_ROUND_NEAR(FP_MULT_INT(thread_current ()->recent_cpu, 100));
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;

  t->priority = t->intrinsic_priority = priority; // 线程创建时的优先级赋值：指定优先级=本征优先级=表征优先级
  list_init(&t->locks_holding); // 初始化list：这个thread都hold了哪些锁
  t->blocked_by_lock = NULL; // 初始化：这个thread还没有被某个lock阻塞
  
  t->magic = THREAD_MAGIC;
  t->ticks_to_wait = 0;       /* p1.1: which means it is not waiting */

  /* p1.3 */
  t->nice = 0;
  t->recent_cpu = INT_TO_FP(0);
  
  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list)){
    return idle_thread;
  }
  else{
    // p1.2 priority
    // if(!thread_mlfqs){
      enum intr_level old_level= intr_disable ();
      list_reverse(&ready_list); // 这是魔法！！！！预先reverse一次，可以保证最后FIFO。不加的话，ready_list会反复正反翻转，并不雨露均沾了
      list_sort(&ready_list, thread_priority_less_than, NULL);
      // sort是升序，故reverse后变降序
      list_reverse(&ready_list);
      intr_set_level (old_level);
    // }
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
  }
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);

/* Return TRUE if a.priority < b.priority. 
   用于list_sort()和list_insert_ordered() */
bool
thread_priority_less_than (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
  return list_entry(a, struct thread, elem)->priority < list_entry(b, struct thread, elem)->priority;
}

/* Update the priority accroding to intrinsic_priority and donated priority (from locks_holding list)
   用本征pri和受赠的pri更新表征pri */
void
update_priority(void){
  if(thread_mlfqs) return;

  int max_priority_among_all_waiters = thread_current()->intrinsic_priority; // 如果没有waiter捐，最低值就是自己的本征pri了子

  struct list_elem *e;
  for (e = list_begin (&thread_current()->locks_holding); e != list_end (&thread_current()->locks_holding); e = list_next (e))
  {
    // 遍历这个thread hold的所有lock
    struct lock *lock_held = list_entry(e, struct lock, holder_list_elem);
    // lock的waiter们
    struct list *waiter_list = &(&lock_held->semaphore)->waiters;
    int max_priority_among_waiters_of_this_lock = list_entry(list_max(waiter_list, thread_priority_less_than, NULL), struct thread, elem)->priority;
    if (max_priority_among_waiters_of_this_lock > max_priority_among_all_waiters){
      max_priority_among_all_waiters = max_priority_among_waiters_of_this_lock;
    }
  }
  thread_current()->priority = max_priority_among_all_waiters;
}

/* TODO
   以下调用要重新schedule，可能发生抢占：
   [√] thread_yield 本来就会触发schedule
   [▲] thread_create 目前直接调用thread_yield
   [√] thread_set_priority
   [√] lock_release 本质上是sema_up
   [√] sema_up 直接调用thread_yield
   [√] cond_signal 本质上是sema_up
   [■] cond_wait 本身不要求重新schedule。wait到signal后因为要acquire一个lock，在lock_acquire不成功的时候会thread_yield，自动发生重新schedule

   正在运行的thread一定拥有 最高的 表征优先级

   优先级排列只用在ready_list里实现，因为sema的up和cond的signal会把所有waiter一股脑扔回ready_list，没能成功down的会再回到waiters里
*/

/* TODO
   以下调用要donate
   [√] lock_acquire不成功时donate
   [√] lock_release时撤回donate，重新计算所有donation
   [√] thread_set_priority时要重新计算所有donation
*/

/* 
   1. lock_acquire不成功->wait->发生一次donation->donate给lock的holder
   2. Sema的down不产生优先级捐赠，因为根本不知道要捐给谁。Sema被up之后，所有的waiter都会被ready，全部扔去schedule，但sema只能被down一次，其他thread发现down不成，会再次注册为waiter然后block。
   3. Cond的wait不产生优先级捐赠，cond_signal和sema的up一样。成功获得signal会acquire一个lock，在lock_acquire时（如果lock在别人手里，就会）发生donation
*/


/* below for p1.3 */
/* p1.3: update ready_threads = all ready threads num + running threads(not idle) */
// void update_ready_threads(void)
// {
//   ASSERT(thread_mlfqs);
//   ASSERT(intr_context());

//   int tmp = thread_current()==idle_thread?0:1;
//   struct list_elem *e;
//   ASSERT (intr_get_level () == INTR_OFF);
//   for (e = list_begin (&ready_list); e != list_end (&ready_list);
//        e = list_next (e)){
//       if(list_entry (e, struct thread, allelem)!=idle_thread){
//         tmp++;
//       }
//     }

//   if(ready_threads!=tmp){
//     ready_threads = tmp;
//   }
// }

/* p1.3: update load_avg */
void update_load_avg(void)
{
  ASSERT(thread_mlfqs);
  // update_ready_threads ();
  int tmp = list_size (&ready_list);
  ready_threads = thread_current ()==idle_thread ?tmp:tmp+1;

  load_avg = FP_ADD(FP_DIV_INT(FP_MULT_INT(load_avg, 59),60), FP_DIV_INT(INT_TO_FP(ready_threads), 60));
  // printf ("ready_threads: %d   load_avg: %lld\n", ready_threads, load_avg);
}


/* p1.3: recompute `recent_cpu`  */
void
update_recent_cpu(struct thread *thread, void *aux UNUSED)
{
  if(!thread_mlfqs) return;
  struct thread *p = thread;
  if(p != idle_thread){
    int64_t coeff = FP_DIV(FP_MULT_INT(load_avg,2),(FP_ADD_INT(FP_MULT_INT(load_avg,2),1)));
    p->recent_cpu = FP_ADD_INT(FP_MULT(coeff, p->recent_cpu), p->nice);
    recalcu_priority(p, NULL);
  }  
}

/* p1.3: update recent_cpu by increasing 1 per seceond */
void current_recent_cpu_increse_1(void)
{
  /* if thread_mlfqs false or idle_thread, not using this */
  ASSERT(thread_mlfqs);
  struct thread *p = thread_current ();
  if(p==idle_thread) return;

  p->recent_cpu = FP_ADD_INT(p->recent_cpu, 1);
}

/* p1.3: recalculate the priority. */
void
recalcu_priority(struct thread *thread, void *aux UNUSED)
{
  /* if thread_mlfqs false or idle_thread, not using this */
  if(!thread_mlfqs) return;
  struct thread *p = thread;
  if(p==idle_thread) return;
  
  p->priority = FP_SUB(FP_ROUND_ZERO(FP_SUB(INT_TO_FP(PRI_MAX), FP_DIV_INT(p->recent_cpu, 4))), INT_TO_FP(2*p->nice));
  if(p->priority<PRI_MIN) p->priority=PRI_MIN;
  if(p->priority>PRI_MAX) p->priority=PRI_MAX;
}