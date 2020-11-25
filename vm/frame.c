#include <list.h>
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

/* falloc a new frame and append as a ft list entry */
void *falloc_get_frame (enum palloc_flags flag, struct sup_page_entry* aux)
{
	uint32_t* ft_vaddr = palloc_get_page(PAL_USER| PAL_ZERO);
	/* if has free frame */
	if (ft_vaddr != NULL){
		struct frame_table_entry* fte = malloc(sizeof(struct frame_table_entry));
		if (fte == NULL){
			exit(-1);
		}

		fte->frame = ft_vaddr;
		fte->owner = thread_current();
		fte->aux = aux;
		struct list_elem* e;
		fte->ele = e;
		list_insert(fte->ele, frame_table);
		return ft_vaddr;
	}
	else{
		evict(flag, aux);
	}
	
}

/* evict a frame */
void
evict(enum palloc_flags flag, struct sup_page_entry* aux)
{
	/* evict to swap/ mmap */
	if(aux->dirty){
		/* evict to disk/ mmap */
	}
	/* else evict a frame from ft */
	struct list_elem *e;
    for (e = list_begin (&frame_table); e != list_end (&frame_table);
           e = list_next (e))
        {
          struct frame_table_entry *fte = list_entry (e, struct frame_table_entry, elem);
        //   ...do something with f...
        }
}

/* free a frame and delete from ft list */
void 
falloc_free_page (struct frame_table_entry* fte);
{
	list_remove(fte->ele);
	palloc_free_page(fte->frame);
	free(fte);
}

