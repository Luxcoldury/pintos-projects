#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "devices/pit.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
#ifndef THREADS/THREAD_C
#include "threads/thread.c"
#endif  
/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* Number of timer ticks since OS booted. */
static int64_t ticks;

/* Number of loops per timer tick.
   Initialized by timer_calibrate(). */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);
static void real_time_delay (int64_t num, int32_t denom);

/* Sets up the timer to interrupt TIMER_FREQ times per second,
   and registers the corresponding interrupt. */
void
timer_init (void) 
{
  pit_configure_channel (0, 2, TIMER_FREQ);
  intr_register_ext (0x20, timer_interrupt, "8254 Timer");
}

/* Calibrates loops_per_tick, used to implement brief delays. */
void
timer_calibrate (void) 
{
  unsigned high_bit, test_bit;

  ASSERT (intr_get_level () == INTR_ON);
  printf ("Calibrating timer...  ");

  /* Approximate loops_per_tick as the largest power-of-two
     still less than one timer tick. */
  loops_per_tick = 1u << 10;
  while (!too_many_loops (loops_per_tick << 1)) 
    {
      loops_per_tick <<= 1;
      ASSERT (loops_per_tick != 0);
    }

  /* Refine the next 8 bits of loops_per_tick. */
  high_bit = loops_per_tick;
  for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
    if (!too_many_loops (loops_per_tick | test_bit))
      loops_per_tick |= test_bit;

  printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/* Returns the number of timer ticks since the OS booted. */
int64_t
timer_ticks (void) 
{
  enum intr_level old_level = intr_disable ();
  int64_t t = ticks;
  intr_set_level (old_level);
  return t;
}

/* Returns the number of timer ticks elapsed since THEN, which
   should be a value once returned by timer_ticks(). */
int64_t
timer_elapsed (int64_t then) 
{
  return timer_ticks () - then;
}

/* Sleeps for approximately TICKS timer ticks.  Interrupts must
   be turned on. */
void
timer_sleep (int64_t ticks) 
{
  // assert arises if Interrupts not turned on.
  ASSERT (intr_get_level () == INTR_ON);
  if (ticks<=0){
    return;
  }
  // make sure atom operation
  enum intr_level old_level = intr_disable ();
  struct thread *p = thread_current();
  p->ticks_to_wait = ticks;
  thread_block();  

  intr_set_level (old_level);
}

/* Sleeps for approximately MS milliseconds.  Interrupts must be
   turned on. */
void
timer_msleep (int64_t ms) 
{
  real_time_sleep (ms, 1000);
}

/* Sleeps for approximately US microseconds.  Interrupts must be
   turned on. */
void
timer_usleep (int64_t us) 
{
  real_time_sleep (us, 1000 * 1000);
}

/* Sleeps for approximately NS nanoseconds.  Interrupts must be
   turned on. */
void
timer_nsleep (int64_t ns) 
{
  real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* Busy-waits for approximately MS milliseconds.  Interrupts need
   not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_msleep()
   instead if interrupts are enabled. */
void
timer_mdelay (int64_t ms) 
{
  real_time_delay (ms, 1000);
}

/* Sleeps for approximately US microseconds.  Interrupts need not
   be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_usleep()
   instead if interrupts are enabled. */
void
timer_udelay (int64_t us) 
{
  real_time_delay (us, 1000 * 1000);
}

/* Sleeps execution for approximately NS nanoseconds.  Interrupts
   need not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_nsleep()
   instead if interrupts are enabled.*/
void
timer_ndelay (int64_t ns) 
{
  real_time_delay (ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics. */
void
timer_print_stats (void) 
{
  printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}


/* decrease `ticks_to_wait` for the waiting threads */
static void
decre_ticks(struct thread *thread, void* aux UNUSED)
{
  struct thread *p = thread;
  if (p->ticks_to_wait==0)       /* not blocked */
    return;
  /* blocked */
  enum intr_level old_level = intr_disable ();
  p->ticks_to_wait -= 1;

  // if `ticks_to_wait` eqauls 0, then unlock
  if (p->ticks_to_wait==0){
    thread_unblock(p);
  }
  intr_set_level (old_level);
}

/* recompute `recent_cpu` for the all threads */
static void
update_recent_cpu(struct thread *thread, void *aux UNUSED)
{
  struct thread *p = thread;
  fixed_t coeff = FP_DIV(FP_MULT_MIX(load_avg,2),(FP_ADD_MIX(FP_MULT_MIX(load_avg,2),1)));
  p->recent_cpu = FP_ADD_MIX(coeff*p->recent_cpu, p->nice);
}

/* recalculate the priority. */
static void
recalcu_priority(struct thread *thread, void *aux UNUSED)
{
  /* if thread_mlfqs false, not using this */
  if(!thread_mlfqs) return;
  struct thread *p = thread;
  p->priority = PRI_MAX - FP_CONST(FP_DIV_MIX(p->recent_cpu, 4)) - 2*(p->nice);
}

/* Timer interrupt handler. which happens each `timer_ticks` */
static void
timer_interrupt (struct intr_frame *args UNUSED)
{
  ticks++;

  /* p1.1: each time ticks++, all positive `ticks_to_wait`-- */
  thread_foreach(decre_ticks, NULL);  

  /* p1.3: update recent_cpu, load_avg, ready_threads frequently */
  if(thread_mlfqs){
    // no interrupts
    enum intr_level old_level = intr_disable ();
    /* update ready_threads */
    ready_threads = list_size(&ready_list)+1;
    /* running thread `recent_cpu` += 1 per tick */
    thread_current()->recent_cpu=FP_ADD_MIX(thread_current()->recent_cpu, 1);
    /* per second */
    if(timer_ticks() % TIMER_FREQ==0){
      /* recompute load_avg and recent_cpu per second */
      load_avg = FP_CONST(FP_DIV_MIX(FP_MULT_MIX(load_avg, 59),60))+ready_threads/60;
      thread_foreach(update_recent_cpu, NULL); 
    }
    // recalculate priority once every fourth clock tick, for every thread
    if(timer_ticks() % 4==0){
      thread_foreach(recalcu_priority, NULL); 
    }

    intr_set_level (old_level);
  }
  thread_tick ();
}


/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
static bool
too_many_loops (unsigned loops) 
{
  /* Wait for a timer tick. */
  int64_t start = ticks;
  while (ticks == start)
    barrier ();

  /* Run LOOPS loops. */
  start = ticks;
  busy_wait (loops);

  /* If the tick count changed, we iterated too long. */
  barrier ();
  return start != ticks;
}

/* Iterates through a simple loop LOOPS times, for implementing
   brief delays.

   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict. */
static void NO_INLINE
busy_wait (int64_t loops) 
{
  while (loops-- > 0)
    barrier ();
}

/* Sleep for approximately NUM/DENOM seconds. */
static void
real_time_sleep (int64_t num, int32_t denom) 
{
  /* Convert NUM/DENOM seconds into timer ticks, rounding down.
          
        (NUM / DENOM) s          
     ---------------------- = NUM * TIMER_FREQ / DENOM ticks. 
     1 s / TIMER_FREQ ticks
  */
  int64_t ticks = num * TIMER_FREQ / denom;

  ASSERT (intr_get_level () == INTR_ON);
  if (ticks > 0)
    {
      /* We're waiting for at least one full timer tick.  Use
         timer_sleep() because it will yield the CPU to other
         processes. */                
      timer_sleep (ticks); 
    }
  else 
    {
      /* Otherwise, use a busy-wait loop for more accurate
         sub-tick timing. */
      real_time_delay (num, denom); 
    }
}

/* Busy-wait for approximately NUM/DENOM seconds. */
static void
real_time_delay (int64_t num, int32_t denom)
{
  /* Scale the numerator and denominator down by 1000 to avoid
     the possibility of overflow. */
  ASSERT (denom % 1000 == 0);
  busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000)); 
}
