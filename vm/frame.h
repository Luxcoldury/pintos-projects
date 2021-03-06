#ifndef VM_FRAME_H
#define VM_FRAME_H
/* refer to palloc.h, frame table is a list 
   whose entries are connected with spt_hash_table entries */
#include <list.h>

/* a list of frame_table_entry as the page table */
struct list frame_table;
struct lock ft_lock;		/* Q: 什么时候用到ft_lock呢？ */

struct frame_table_entry {
	struct list_elem ele;		/* for list */
	uint32_t* frame;  				/* ptr to page that currently occupies it */
	struct thread* owner;			/* the thread who owns it */
	struct sup_page_table_entry* page;	/* ptr to supplemental page entry */
	bool do_not_swap;
// Maybe store information for memory mapped files here too?
};


void ft_init(void);
struct frame_table_entry* ft_get_frame (struct sup_page_table_entry* page);
void ft_free_frame (struct frame_table_entry* fte);

#endif/* vm/ frame.h */