#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stdbool.h>
#include <stdint.h>
#include "threads/synch.h"

#define SWAP_ERROR -1

/* Block device for the swap */
struct block *swap_dev;

/* Bitmap of swap slots to depict availability */
struct bitmap *swap_table;
struct lock swap_lock;

/* Swap initialization */
void swap_table_init (void);

/* Swap garbage collection */
void swap_table_destroy (void);

/* Swap a frame into a swap slot */
size_t swap_out (void *uaddr);

/* Swap a frame out of a swap slot to mem page */
void swap_in (size_t swap_index, void *uaddr);

/* Vacate a given swap slot */
void free_swap_slot (size_t swap_index);

#endif 
