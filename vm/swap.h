#ifndef VM_SWAP_H
#define VM_SWAP_H

enum block_type{
	FILE,
	
}
struct block
{
	struct list_elem list_elem; /* Element in all_blocks. */
	char name[16]; 				/* Block device name. */
	enum block_type type; 		/* Type of block device. */
	block_sector_t size; 		/* Size in sectors. */
	const struct block_operations *ops; /* Driver operations. */
	void *aux;					 /* Extra data owned by driver. */
	unsigned long long read_cnt; /* Number of sectors read. */
	unsigned long long write_cnt; /* Number of sectors written. */
};

#endif/* vm/ swap.h */