#include "page.h"
#include "threads/thread.h"
#include "frame.h"
#include "swap.h"				/* for clear */
#include "lib/kernel/hash.h"	/* for hash */
#include "threads/malloc.h"
#include "userprog/process.h"	/* install_page */


/* Returns a hash value for page p. */
unsigned
spt_hash (const struct hash_elem *p_, void *aux UNUSED)
{
const struct sup_page_table_entry *p = hash_entry (p_, struct sup_page_table_entry, hash_ele);
return hash_bytes (&p->user_vaddr, sizeof(p->user_vaddr));
}


/* Returns true if page a precedes page b. */
bool
spt_hash_less (const struct hash_elem*a_, const struct hash_elem*b_, void *aux UNUSED)
{
const struct sup_page_table_entry *a = hash_entry (a_, struct sup_page_table_entry, hash_ele);
const struct sup_page_table_entry *b = hash_entry (b_, struct sup_page_table_entry, hash_ele);
return a->user_vaddr < b->user_vaddr;
}


/* Returns the page containing the given virtual address,
   or a null pointer if no such page exists in the current thread's pages. */
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
	if(!install_page(vaddr, frame->frame, true)){
		spt_free_page(newPage);
		return NULL;
	}
	
	return newPage;
}


struct sup_page_table_entry* 
spt_create_file_mmap_page (uint32_t* vaddr, struct file * file, size_t offset, uint32_t file_bytes, bool writable)
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
spt_free_file_mmap_page(uint32_t vaddr)
{
	struct sup_page_table_entry* page =  spt_hash_lookup(vaddr);
	if(page == NULL || page->file)
		return NULL;

	if(page)
	
  // Pin the associated frame if loaded
  // otherwise, a page fault could occur while swapping in (reading the swap disk)
  if (spte->status == ON_FRAME) {
    ASSERT (spte->kpage != NULL);
    vm_frame_pin (spte->kpage);
  }


  // see also, vm_load_page()
  switch (spte->status)
  {
  case ON_FRAME:
    ASSERT (spte->kpage != NULL);

    // Dirty frame handling (write into file)
    // Check if the upage or mapped frame is dirty. If so, write to file.
    bool is_dirty = spte->dirty;
    is_dirty = is_dirty || pagedir_is_dirty(pagedir, spte->upage);
    is_dirty = is_dirty || pagedir_is_dirty(pagedir, spte->kpage);
    if(is_dirty) {
      file_write_at (f, spte->upage, bytes, offset);
    }

    // clear the page mapping, and release the frame
    vm_frame_free (spte->kpage);
    pagedir_clear_page (pagedir, spte->upage);
    break;

  case ON_SWAP:
    {
      bool is_dirty = spte->dirty;
      is_dirty = is_dirty || pagedir_is_dirty(pagedir, spte->upage);
      if (is_dirty) {
        // load from swap, and write back to file
        void *tmp_page = palloc_get_page(0); // in the kernel
        vm_swap_in (spte->swap_index, tmp_page);
        file_write_at (f, tmp_page, PGSIZE, offset);
        palloc_free_page(tmp_page);
      }
      else {
        // just throw away the swap.
        vm_swap_free (spte->swap_index);
      }
    }
    break;

  case FROM_FILESYS:
    // do nothing.
    break;

  default:
    // Impossible, such as ALL_ZERO
    PANIC ("unreachable state");
  }

  // the supplemental page table entry is also removed.
  // so that the unmapped memory is unreachable. Later access will fault.
  hash_delete(& supt->page_map, &spte->elem);
  return true;
}



/* free resource of the page at address `vaddr` 
   and remove it from hash table 
 */
void
spt_free_page (struct sup_page_table_entry* page)
{
	/* for mmap */
	// free a mmap file page
	if(page->file){
		// unmap();
		return ;
	}

	/* for swap if on disk */
	if(page->status == SWAP){
		swap_free_pagesized_blocks(page->swap_id);
	}
	if(page->status == FRAME){
		ft_free_frame(page->frame);
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

