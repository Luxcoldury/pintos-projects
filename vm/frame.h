#ifndef VM_FRAME_H
#define VM_FRAME_H
#include <list.h>


/* a list of frame_table_entry as the page table */
struct list frame_table;
struct lock ft_lock;		/* Q: 什么时候用到ft_lock呢？ */

struct frame_table_entry {
	uint32_t* frame;  			/* ptr to page that currently occupies it */
	struct thread* owner;		/* the thread who owns it */
	struct sup_page_entry* page;	/* ptr to supplemental page entry */
	struct list_element *ele;
// Maybe store information for memory mapped files here too?
};


/* refer to palloc.h */

void ft_init();
void *falloc_get_frame (enum palloc_flags);
void falloc_free_page (struct frame_table_entry* fte);

#endif/* vm/ frame.h */