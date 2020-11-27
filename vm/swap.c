#include "swap.h"
#include "page.h"
#include "userprog/pagedir.h"


//Make the swap block global
static size_t blocks_per_page =  PGSIZE / BLOCK_SECTOR_SIZE;
static struct block* swap_blocks;			/* the swap device */
static block_sector_t swap_num;				/* num of available swaps(how many pages)in swap device */
static struct bitmap* swap_free_map;		/* true if free for swap, otherwise false */


/* Get the block device when we initialize our swap code */
void 
swap_init();
{
	/* init the block device*/
	global_swap_block = block_get_role(BLOCK_SWAP);
		/* if failed, ERROR */
	block_sector_t num_blocks = block_size(global_swap_block);
	swap_num = num_blocks / blocks_per_page;

	/* init bitmap */
	swap_free_map = bitmap_create( block_num/ BLOCKS_PER_PAGE );
	bitmap_set_all(swap_free_map, true);
}



/* bitmap operation: 
   find the first free block and 
   return its index in the bitmap. */
static size_t
first_free_block_index(){
	return bitmap_scan_and_flip(swap_free_map, 0, 1, true);
}


/* bitmap operation: 
   as the name indecates */
static void
swap_free_pagesized_blocks(swap_index)
{
	/* too large index */
	ASSERT (swap_index >= swap_num);

	/* the block is empty(available) */
	ASSERT (bitmap_test(swap_free_map, swap_index) == true);
	
	bitmap_flip(swap_free_map, swap_index);
}


/* true for write(swap out), false for read 
   the frame is the frame I want to read into / write from.
*/
static size_t
read_write_block(uint8_t* frame, bool write, size_t index){
{
	/* write to disk */
	if ( write ){
		/* the index is the starting index of the block that is free */
		index = first_free_block_index();

		/* page write */
		for(int i = 0; i < 8; ++i)
		{
		/* each read/write will rea/write 512 bytes, therefore we need to read/
		write 8 times, each at 512 increments of the frame */
		block_write(swap_blocks, blocks_per_page * index + i, frame + (i * BLOCK_SECTOR_SIZE));
		} 
		return index;
	}
	else{
		/* page write */
		for(int i = 0; i < 8; ++i)
		{
		/* each read/write will rea/write 512 bytes, therefore we need to read/
		write 8 times, each at 512 increments of the frame */
		block_write(swap_blocks, blocks_per_page * index + i, frame + (i * BLOCK_SECTOR_SIZE));
		} 
		return index;
	}
}

/* swap out: from frame */
void swap_eviction(struct frame_table_entry* fte)
{
	/* set page, frame unlinked */
	struct supple_page_entry* page = fte->page;
	page->frame = NULL;

	/* then free the frame （not the spte） */
	pagedir_clear_page(thread_current()->pagedir, fte->frame);
	ft_free_frame(fte);

	/* find the block and write */
	size_t swap_index = read_write_block(fte->frame, true, 1);/* 1 is not used here */
	
	/* track evicted pages */
	page->status = SWAP;
	page->swap_id = swap_index;
	return;
}


/* swap in: back into frame fte */
void swap_reclamation(struct frame_table_entry* fte, struct sup_page_table_entry* spte)
{
	/* re-link a new frame(rather than new spte) */
	spte->frame = fte;
	frame->page = spte;

	/* read from disk to frame */
	size_t swap_index = read_write_block(fte->frame, false, spte->swap_id);

	/* free the blocks */
	page->status = FRAME;
	swap_free_pagesized_blocks(swap_index);
}
