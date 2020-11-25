#ifndef VM_PAGE_H
#define VM_PAGE_H

/* refer to `pte.h`, `hash.h` */
#include "lib/kernel/hash.c"

//a list or hashtable of sup_page_table_entry as your supplementary page table
//remember, each thread should have its own sup_page_table, so create a new
// list or hashtable member in thread.h
struct sup_page_table_entry {
	uint32_t* user_vaddr;
/*
Consider storing the time at which this page was accessed if you want to
implement LRU!
Use the timer_ticks () function to get this value!*/
	uint64_t access_time;
// You can use the provided PTE functions instead. I’ve posted links to
// the documentation below
	bool dirty;
	bool accessed;
}

#endif/* vm/ page.h */