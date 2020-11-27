#include "page.h"
#include "threads/thread.h"
#include "frame.h"
#include "lib/kernel/hash.h"	/* for hash */


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
	struct hash_elem*e;
	p.user_vaddr = address;
	e = hash_find (&thread_current ()->spt_hash_table, &p.hash_ele);
	return e != NULL ? hash_entry (e, struct sup_page_table_entry, hash_ele) : NULL;
}


/* init a spt entry with known info */
struct sup_page_table_entry* 
spt_init_page (uint32_t vaddr)
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


/* create a new page and push into hashtable */
struct sup_page_table_entry* 
spt_create_page (uint32_t vaddr)
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
	newPage->frame = frame;
	install_page(vaddr, frame->frame, true);
	
	return newPage;
}


struct sup_page_table_entry* 
spt_create_file_mmap_page (uint32_t vaddr, struct file * file, size_t offset, uint32_t file_bytes, bool writable)
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

/* free resource of the page at address `vaddr` 
   and remove it from hash table 
 */
void* 
spt_free_page (struct sup_page_table_entry* page)
{
	/* for mmap */
	// free a mmap file page
	if(page->file){
		// unmap();
		return;
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

