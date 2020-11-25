#ifndef VM_PAGE_H
#define VM_PAGE_H

/*  refer to `pte.h`, `hash.h`
	use a hashtable of `sup_page_table_entry` as supplementary page table
	each thread has its own sup_page_table:
	`thread.sup_page_table` 
 */
#include <stdbool.h>			/* for bool */
#include "lib/debug.h"			/* for unused var */
#include "lib/kernel/hash.h"


/* spt entry with information about page */
struct sup_page_table_entry {
	struct hash_elem hash_ele;		/* hash element */
	uint32_t* user_vaddr;			/* virtual address, as a key in hash table */
	struct lock spt_lock;					/* lock to provide page operation e.g. hash */

// You can use the provided PTE functions instead. I’ve posted links to
// the documentation below
	bool dirty;						/* if modified */
	bool accessed;					/* if accessed */ 
};


// for spt operations
void spt_init_page (struct hash_elem e, uint32_t vaddr, bool isDirty, bool isAccessed);
void* spt_create_page (uint32_t vaddr);
void* spt_free_page (uint32_t vaddr);


// for hash uses (included for init_thread )
unsigned spt_hash (const struct hash_elem *p_, void *aux UNUSED);
bool spt_hash_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);


#endif  /* vm/ page.h */