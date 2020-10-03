/* Tests that cond_signal() wakes up the highest-priority thread
   waiting in cond_wait(). */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"

static thread_func priority_condvar_thread;
static struct lock lock;
static struct condition condition;

void
test_priority_condvar (void) 
{
  int i;
  
  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);

  lock_init (&lock);
  cond_init (&condition);

  thread_set_priority (PRI_MIN);
  for (i = 0; i < 10; i++) 
    {
      int priority = PRI_DEFAULT - (i + 7) % 10 - 1;
      char name[16];
      snprintf (name, sizeof name, "priority %d", priority);
      thread_create (name, priority, priority_condvar_thread, NULL);
    }

  for (i = 0; i < 10; i++) 
    {
      lock_acquire (&lock);
      msg ("Signaling...");
      cond_signal (&condition, &lock);
      lock_release (&lock);
    }
}

static void
priority_condvar_thread (void *aux UNUSED) 
{
  msg ("Thread %s starting.", thread_name ());
  lock_acquire (&lock);
  cond_wait (&condition, &lock);
  msg ("Thread %s woke up.", thread_name ());
  lock_release (&lock);
}

/* 
  第35行时，所有子thread都被block在第50行，等待signal
  之后的运行顺序是:
  
  38、39、40 主线程acquire锁，发signal导致重schedule
  主pri=1，子线程抢占，pri最高的子线程抢到signal，其他继续等
  50处cond获得signal，想acquire锁但lock在主线程手上，向主线程donate之后阻塞等锁，回到主线程，主pri=子pri
  主线程41 release锁，失去donation，主pri=1，release导致重schedule，刚才的子线程得以抢占
  50成功acquire锁，51、52release锁，子线程结束
  其他子线程仍在等signal，主pri=1，进入下一循环

  结论：
  cond_wait不产生donation，因为是（release锁->block->获得signal->acquire锁），block时并不是锁的holder
  发signal要重schedule

*/