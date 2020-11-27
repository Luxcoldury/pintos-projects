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
#include "lib/kernel/hash.h"	/* for hash */
#include "threads/synch.h"		/* for lock */

/* where the data of a page in */
enum page_type{
	FRAME,
	SWAP,
	FILE
};

/* spt entry with information about page */
struct sup_page_table_entry {
	/* for hash */
	struct hash_elem hash_ele;		/* hash element */
	uint32_t* user_vaddr;			/* virtual address, as a key in hash table */
	struct lock spt_lock;			/* lock to provide page operation e.g. hash */

	/* information for swap */
	enum page_type status;				/* where the data are in */
	struct frame_table_entry* frame;	/* frame */
	size_t swap_id;						/* swap index in bitmap table */

	// for FILE mmap
	sturct file *file;
	size_t file_offset;
	size_t file_bytes;
	bool writable;

};


// for spt operations
struct sup_page_table_entry*  spt_init_page (uint32_t vaddr, bool isDirty);
void* spt_create_page (uint32_t vaddr);
void* spt_free_page (struct sup_page_table_entry* page);

struct sup_page_table_entry *spt_hash_lookup (const void *address);

struct sup_page_table_entry *spt_create_file_mmap_page (uint32_t vaddr, struct file * file, size_t offset, uint32_t file_bytes, bool writable);

// for hash uses (included for init_thread )
unsigned spt_hash (const struct hash_elem *p_, void *aux UNUSED);
bool spt_hash_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
void spt_destroy_hash(struct thread* t);

#endif  /* vm/ page.h */