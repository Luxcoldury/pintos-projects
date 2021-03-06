#include "page.h"
#include "threads/thread.h"
#include "frame.h"
#include "swap.h"				/* for clear */
#include "lib/kernel/hash.h"	/* for hash */
#include "threads/malloc.h"
#include "userprog/process.h"	/* install_page */
#include "filesys/file.h"     // syscall
#include "filesys/off_t.h"     // syscall


/* Returns a hash value for page p. */
unsigned
spt_hash (const struct hash_elem *p_, void *aux UNUSED)
{
	const struct sup_page_table_entry *p = hash_entry (p_, struct sup_page_table_entry, hash_ele);
	return hash_bytes (&p->user_vaddr, sizeof(p->user_vaddr));
}


/* Returns true if page a precedes page b. */
bool
spt_hash_less (const struct hash_elem* a_, const struct hash_elem* b_, void *aux UNUSED)
{
	const struct sup_page_table_entry *a = hash_entry (a_, struct sup_page_table_entry, hash_ele);
	const struct sup_page_table_entry *b = hash_entry (b_, struct sup_page_table_entry, hash_ele);
	return a->user_vaddr < b->user_vaddr;
}


/* Returns the page containing the given virtual address,
   or a null pointer 
   if no such page exists in the current thread's pages. */
struct sup_page_table_entry *
spt_hash_lookup (const void *address)
{
	struct sup_page_table_entry p;
	struct hash_elem* e;
	p.user_vaddr = (uint32_t*) address;
	
	e = hash_find (&thread_current ()->spt_hash_table, &p.hash_ele);
	return e != NULL ? hash_entry (e, struct sup_page_table_entry, hash_ele) : NULL;
}


/* init a spt entry with known info */
struct sup_page_table_entry* 
spt_init_page (uint32_t* vaddr)
{
	struct sup_page_table_entry* page = malloc (sizeof(struct sup_page_table_entry));
	if(page == NULL){
		/* malloc fail */
		return NULL;
	}
	page->user_vaddr = vaddr;
	lock_init (&page->spt_lock);
	return page;
}


/* create a new page, push into hashtable, and install it. */
struct sup_page_table_entry* 
spt_create_page (uint32_t* vaddr)
{
	struct sup_page_table_entry* newPage = spt_init_page(vaddr);
	/* if page allocation failed */
	if(newPage == NULL){
		return NULL;
	}

	// insert page into hash table
	if(hash_insert(&thread_current()->spt_hash_table, &newPage->hash_ele)!=NULL){
		free(newPage);
		return NULL;
	}

	/* if success, get a frame for it */
	struct frame_table_entry* frame = ft_get_frame(newPage);
	if(frame == NULL){
		free(newPage);
		return NULL;
	}

	/* install page and frame */
	newPage->frame = frame;
	newPage->status = FRAME;
    newPage->file=NULL;
	newPage->file_offset=0;
	newPage->file_bytes=0;
	newPage->writable=true;
	if(!install_page(vaddr, frame->frame, true)){
		spt_free_page(newPage);
		return NULL;
	}
	
	return newPage;
}


/* reallocate a frame and load the data on swap to the new frame.
   return false if failed. */
bool
spt_reallocate_frame_and_load(struct sup_page_table_entry* page)
{
	ASSERT(page->status != FRAME);
	/* get a frame for it */
	struct frame_table_entry* frame = ft_get_frame(page);
	if(frame == NULL){
		return false;
	}

	/* if success, install page and frame */
	if(!install_page(page->user_vaddr, frame->frame, page->writable)){
		spt_free_page(page);
		return false;
	}

	/* load data to frame */
    if(page->status == SWAP){
        swap_reclamation(frame, page);
    }
	else if(page->status == FILE){
      if(page->file == NULL)
        return false;

      file_seek (page->file, page->file_offset);
      off_t read_bytes = file_read (page->file, frame->frame, page->file_bytes);
      if(read_bytes != (off_t)page->file_bytes)
        return false;
      memset (frame->frame + read_bytes, 0, PGSIZE - page->file_bytes);
    }
	page->frame = frame;
	page->status = FRAME;

	/* success */
	return true;
}


struct sup_page_table_entry* 
spt_create_file_mmap_page (uint32_t* vaddr, struct file * file, off_t offset, uint32_t file_bytes, bool writable)
{
	struct sup_page_table_entry* newPage = spt_init_page(vaddr);
	/* if page allocation failed */
	if(newPage == NULL){
		return NULL;
	}

	// insert page into hash table
	if(hash_insert(&thread_current()->spt_hash_table, &newPage->hash_ele)!=NULL){
		free(newPage);
		return NULL;
	}

	/* if success, map a file for it */
	newPage->status = FILE;
	newPage->frame = NULL;
	newPage->file = file;
	newPage->file_offset = offset;
	newPage->file_bytes = file_bytes;
	newPage->writable = writable;

	return newPage;
}

void *
spt_free_file_mmap_page(uint32_t* vaddr)
{
	struct sup_page_table_entry* page = spt_hash_lookup(vaddr);
	if(page == NULL || page->file)
		return NULL;

	struct frame_table_entry* frame = page->frame;
    struct file* f = page->file;

	switch (page->status) {
        case FRAME:
            lock_acquire(&ft_lock);
            frame->do_not_swap = true;
            lock_release(&ft_lock);

            uint32_t* pagedir = thread_current()->pagedir;
            if(pagedir_is_dirty(pagedir, page->user_vaddr) || pagedir_is_dirty(pagedir, page->frame->frame)) {
                file_write_at (page->file, page->user_vaddr, page->file_bytes, page->file_offset);
            }
            break;

        case SWAP:
            swap_reclamation(page->frame,page);
            file_write_at (page->file, page->user_vaddr, page->file_bytes, page->file_offset);
            break;
	}

    ft_free_frame (page->frame);
	hash_delete(&thread_current()->spt_hash_table, &page->hash_ele);
	free(page);
    return true;
}



/* free resource of the page at address `vaddr` 
   and remove it from hash table 
 */
void
spt_free_page (struct sup_page_table_entry* page)
{
	/* for swap if on disk */
	if(page->status == SWAP){
		swap_free_pagesized_blocks(page->swap_id);
	}
	if(page->status == FRAME){
		ft_free_frame(page->frame);
	}
    if(page->status == FILE){
		spt_free_file_mmap_page(page->frame);
        return;
	}
	hash_delete(&thread_current()->spt_hash_table, &page->hash_ele);
	free(page);
}


/* destructor function  for `spt_destroy_hash` */
static void
spt_hash_destructor(struct hash_elem*e, void *aux UNUSED)
{
  /* clean the spte related resources  */
  struct sup_page_table_entry *page = hash_entry (e, struct sup_page_table_entry, hash_ele);

  /* free the spte */
  spt_free_page(page);
}


/* destroy the spt hash table of the given thread */
void spt_destroy_hash(struct thread* t)
{
	hash_destroy(&t->spt_hash_table, spt_hash_destructor);
}

