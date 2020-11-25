#include "page.h"
#include "threads/thread.h"


/* Returns a hash value for page p. */
unsigned
spt_hash (const struct hash_elem *p_, void *aux UNUSED)
{
const struct sup_page_table_entry *p = hash_entry (p_, struct sup_page_table_entry, hash_elem);
return hash_bytes (&p->user_vaddr, sizeof(p->user_vaddr));
}


/* Returns true if page a precedes page b. */
bool
spt_hash_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
const struct sup_page_table_entry *a = hash_entry (a_, struct sup_page_table_entry, hash_elem);
const struct sup_page_table_entry *b = hash_entry (b_, struct sup_page_table_entry, hash_elem);
return a->user_vaddr < b->user_vaddr;
}


/* Returns the page containing the given virtual address,
   or a null pointer if no such page exists in the current thread's pages. */
struct sup_page_table_entry *
spt_hash_lookup (const void *address)
{
struct sup_page_table_entry p;
struct hash_elem *e;
p.user_vaddr = address;
e = hash_find (&thread_current ()->spt_hash_table, &p.hash_elem);
return e != NULL ? hash_entry (e, struct sup_page_table_entry, hash_elem) : NULL;
}


/* init a spt entry with known info */
struct sup_page_table_entry* 
spt_init_page (uint32_t vaddr, bool isDirty=0, bool isAccessed=0)
{
	struct sup_page_table_entry* page = malloc (sizeof(struct sup_page_table_entry));
	if(page == NULL){
		/* malloc fail */
		return NULL;
	}
	page->user_vaddr = vaddr;e;
	page->dirty = isDirty;
	page->accessed = isAccessed;
	return page;
}


/* create a new page and push into hashtable */
void* 
spt_create_page (uint32_t vaddr)
{
	struct sup_page_table_entry* newPage = spt_init_page(vaddr);
	if(newPage == NULL){
		return NULL;
	}
	hash_insert(&thread_current()->spt_hash_table, &newPage->hash_ele)
	return newPage;

}


/* free resource of the page at address `vaddr` 
   and remove it from hash table 
 */
void* 
spt_free_page (uint32_t vaddr)
{
	struct sup_page_table_entry* page =  spt_hash_lookup(vaddr);
	hash_delete(&thread_current()->spt_hash_table, page->hash_ele);
	free(page);
}
