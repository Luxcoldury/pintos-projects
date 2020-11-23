#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>

//a list of frame_table_entry as the page table
struct list frame_table;
struct frame_table_entry {
	uint32_t* frame;
	struct thread* owner;
	struct sup_page_entry* aux;
// Maybe store information for memory mapped files here too?
};

void ft_init;
void get_fte();
palloc_get_page

#endif/* vm/ frame.h */