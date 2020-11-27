#ifndef VM_PAGE_H
#define VM_PAGE_H

/*  refer to `pte.h`, `hash.h`
	use a hashtable of `sup_page_table_entry` as supplementary page table
	each thread has its own sup_page_table:
	`thread.sup_page_table` 
 */
#include <stdbool.h>			/* for bool */
#include <stddef.h>				/* for size_t */
#include "lib/debug.h"			/* for unused var */
#include "lib/kernel/hash.h"


/* spt entry with information about page */
struct sup_page_table_entry {
	/* for hash */
	struct hash_elem hash_ele;		/* hash element */
	uint32_t* user_vaddr;			/* virtual address, as a key in hash table */
	struct lock spt_lock;			/* lock to provide page operation e.g. hash */

	/* for LRU evict in frame */
	uint64_t access_time;
// You can use the provided PTE functions instead. I’ve posted links to
// the documentation below, dirty and accessed暂时没有用到
	bool dirty=false;					/* if modified */
	bool accessed=false;				/* if accessed */ 

	/* information for swap */
	struct frame_table_entry* frame;	/* frame */
	size_t swap_id;						/* swap index in bitmap table */
	enum page_type status;				/* where the data are in */

};

enum page_type{
	FRAME,
	SWAP,
	FILE
};

// for spt operations
struct sup_page_table_entry*  spt_init_page (uint32_t vaddr, bool isDirty, bool isAccessed);
void* spt_create_page (uint32_t vaddr);
void* spt_free_page (uint32_t vaddr);


// for hash uses (included for init_thread )
unsigned spt_hash (const struct hash_elem *p_, void *aux UNUSED);
bool spt_hash_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);


#endif  /* vm/ page.h */