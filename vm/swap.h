#ifndef VM_SWAP_H
#define VM_SWAP_H
/* swap slots/ blocks in global_swap_block */
#include "devices/block.h"
#include "lib/kernel/bitmap.h"
#include "frame.h"
#include "threads/vaddr.h"


/* swap operations */
void swap_init();
void swap_eviction(struct frame_table_entry* fte);
void swap_reclamation(struct frame_table_entry* fte, struct sup_page_table_entry* spte);
void swap_free_pagesized_blocks(size_t swap_index);

#endif/* vm/ swap.h */