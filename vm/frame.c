#include <list.h>
#include "swap.h"
#include "frame.h"
#include "threads/palloc.h"
#include "threads/threads.h"

/* init frame table */
void
ft_init()
{
	list_init(&frame_table);
	lock_init(&ft_lock);
}

/* falloc a new frame from a page and append as a ft list entry */
void *ft_get_frame (struct sup_page_entry* page)
{
	uint32_t* ft_vaddr = palloc_get_page(PAL_USER);
	/* if has free frame */
	if (ft_vaddr != NULL){
		struct frame_table_entry* fte = malloc(sizeof(struct frame_table_entry));
		if (fte == NULL){
			return NULL;
		}
	}
	else{
		/* if no free frame, evict one for the given page */
		if(struct frame_table_entry* fte = ft_evict_frame(page) == NULL){
			return NULL;
		}
	}
	/* construct the struct */
	fte->frame = ft_vaddr;
	fte->owner = thread_current();
	fte->page = page;
	struct list_elem* e;
	fte->ele = e;
	list_insert(fte->ele, frame_table);
	return fte;
}


/* evict a frame and one is available, NOT FINISHED!!! */
void
ft_evict_frame(struct sup_page_entry* page)
{
	/* evict to swap/ mmap */
	if(page->dirty){
		/* evict to disk/ mmap */
		return NULL;
	}
	/* else evict a frame from ft */
	uint64_t earlist_time = 99999;
    for (struct list_elem *e = list_begin (&frame_table); e != list_end (&frame_table);
           e = list_next (e))
        {
          struct frame_table_entry *fte = list_entry (e, struct frame_table_entry, elem);
		  int access_time = fte->page->access_time;
		  bool accessed = fte->page->accessed;

		  /* if any page is not accessed, swap out its frame */
		  if(! accessed ){
			swap_eviction(fte);
			return fte;
		  }

		  /* else use LRU */
		  if(access_time < earlist_time){
			  earlist_time = access_time;
			  struct frame_table_entry* evict_frame = fte;
		  }
        }
	swap_eviction(evict_frame);
	return evict_frame;
}


/* free a frame and delete from ft list */
void 
ft_free_frame (struct frame_table_entry* fte);
{
	list_remove(fte->ele);
	palloc_free_page(fte->frame);
	free(fte);
}

