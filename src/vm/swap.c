#include <bitmap.h>
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>

#include "vm/swap.h"

static size_t SECTORS_PER_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;
static size_t pages_per_swap ();

/* Swap initialization */
void 
swap_table_init (void)
{
    swap_dev = block_get_role (BLOCK_SWAP);
    if (swap_dev != NULL)
    {
        lock_init (&swap_lock);
        swap_table = bitmap_create (pages_per_swap ());
        if (swap_table != NULL)
        {
            /* if swap table's bitmap creation was successful, 
               initialize all the slots to 'available' */
            bitmap_set_all (swap_table, true);
        }
        else
        {
            /* panic the kernel if bitmap could not be created */
            PANIC ("ERROR: swap bitmap could not be created");
        }
    }
    else
    {
        /* panic the kernel if no swap device found */
        PANIC ("ERROR: no swap device found");
    }
}

/* Find an available swap slot and place the given page into the slot */
size_t 
swap_out (void *uaddr)
{
    /* find a free swap slot and mark it as used concurrently */
    lock_acquire (&swap_lock);
    size_t swap_index = bitmap_scan_and_flip (swap_table, 0, 1, true);
    lock_release (&swap_lock);

    if (swap_index == BITMAP_ERROR)
        return SWAP_ERROR;
    
    size_t count = 0;
    while (count < SECTORS_PER_PAGE)
    {
        block_write (swap_dev, (swap_index * SECTORS_PER_PAGE) + count, 
                     uaddr + (count * BLOCK_SECTOR_SIZE));
        count++;
    }
    return swap_index;
}

/* Swap a page out of a swap slot and place in memory designated by uaddr */
void 
swap_in (size_t swap_index, void *uaddr)
{
    /* swap a page size's worth of data into memory */
    size_t count = 0;
    while (count < SECTORS_PER_PAGE)
    {
        block_read (swap_dev, (swap_index * SECTORS_PER_PAGE) + count,
                    uaddr + (count * BLOCK_SECTOR_SIZE));
        count++;
    }

    lock_acquire (&swap_lock);
    bitmap_flip (swap_table, swap_index);
    lock_release (&swap_lock);
}

/* update metadata and free the given swap slot*/
void 
free_swap_slot (size_t swap_index)
{
    /* clear the corresponding swap slot bit in bitmap */
    bitmap_flip (swap_table, swap_index);  
}

/* Fixed value of pages that could be contained in a swap block */
static size_t 
pages_per_swap ()
{
    return (block_size (swap_dev) / SECTORS_PER_PAGE);
}
