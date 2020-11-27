#ifndef VM_SWAP_H
#define VM_SWAP_H
/* swap slot in block.h */
#include "devices/block.h"
#include "lib/kernel/bitmap.h"
#include "frame.h"

//Make the swap block global
static struct block* global_swap_block;
//Get the block device when we initialize our swap code
void swap_init()
{
global_swap_block = block_get_role(BLOCK_SWAP);
}


/* swap operations */
void swap_eviction();
void swap_reclamation();


#endif/* vm/ swap.h */