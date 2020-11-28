#include <list.h>
#include "swap.h"
#include "frame.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* init frame table */
void
ft_init(void)
{
	list_init(&frame_table);
	lock_init(&ft_lock);
}


/* evict a frame and one is available, NOT FINISHED!!! */
static struct frame_table_entry*
ft_evict_frame(void)
{
	// /* evict to swap/ mmap */
	// if(page->dirty){
	// 	/* evict to disk/ mmap */
	// 	return NULL;
	// }
	/* else evict a frame from ft */
	/* uint64_t earlist_time = 99999;
    for (struct list_elem *e = list_begin (&frame_table); e != list_end (&frame_table);
           e = list_next (e))
        {
          struct frame_table_entry *fte = list_entry (e, struct frame_table_entry, elem);

        } */

	/* temporily choose the first one */
	struct list_elem *e = list_begin (&frame_table);
    struct frame_table_entry *evict_frame = list_entry (e, struct frame_table_entry, ele);

	swap_eviction(evict_frame);
	return evict_frame;
}


/* falloc a new frame from a page and append as a ft list entry 
   return NULL if failed. */
struct frame_table_entry *
ft_get_frame (struct sup_page_table_entry* page)
{
	lock_acquire(&ft_lock);

	uint32_t* ft_vaddr = palloc_get_page(PAL_USER);
	struct frame_table_entry* fte;
	/* if has free frame */
	if (ft_vaddr != NULL){
		fte = malloc(sizeof(struct frame_table_entry));
		if (fte == NULL){
			return NULL;
		}
	}
	else{
		/* if no free frame, evict one for the given page */
		fte = ft_evict_frame();
		if(fte == NULL){
			return NULL;
		}
	}
	/* construct the struct */
	fte->frame = ft_vaddr;
	fte->owner = thread_current();
	fte->page = page;
	list_push_back(&frame_table, &fte->ele);

	lock_release(&ft_lock);
	return fte;
}


/* free a frame and delete from ft list */
void 
ft_free_frame (struct frame_table_entry* fte)
{
	lock_acquire(&ft_lock);

	list_remove(&fte->ele);
	palloc_free_page(fte->frame);
	free(fte);

	lock_release(&ft_lock);
}

