#include "swap.h"
#include "page.h"
#include "userprog/pagedir.h"

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
	
	
	/* track evicted pages */
	page->swap_id = s
}


/* swap in: back into frame */
void swap_reclamation();