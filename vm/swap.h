#ifndef VM_SWAP_H
#define VM_SWAP_H
/* swap slots/ blocks in global_swap_block */
#include "devices/block.h"
#include "lib/kernel/bitmap.h"
#include "frame.h"
#include "threads/vaddr.h"


/* swap operations */
void swap_init();
void swap_eviction();
void swap_reclamation();


#endif/* vm/ swap.h */