#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "userprog/syscall.h"
#include <stdio.h>


struct frame_table *f_table;

/* Frame table initialization */
void f_table_init (void)
{
    f_table = malloc (sizeof (struct frame_table));
    ASSERT (f_table != NULL);
    f_table->frames = (struct frame *)calloc (sizeof (struct frame), ft_max);
    ASSERT (f_table->frames != NULL);
    lock_init (&f_table->ft_lock);
    f_table->clock_hand = 0;
    f_table->num_free = ft_max;
}

/* Allocation of a frame within physical memory, should be allocated
   within the User pool */
void *
f_table_alloc (enum palloc_flags flag)
{
    ASSERT(flag == PAL_USER || flag == (PAL_ZERO | PAL_USER));
    if (flag == PAL_USER || flag == (PAL_ZERO | PAL_USER))
    {
        int index = f_table_get_index ();
        void *page = NULL;
        if (index == FRAME_ERROR)
        { /* Evict a frame for page */
            f_table_evict ();
            index = f_table->clock_hand - 1;
        }
        page = palloc_get_page (flag);
        ASSERT (page != NULL);

        lock_acquire( &f_table->ft_lock);
        struct frame *empty_frame = f_table->frames + index;
        empty_frame->page = page;
        empty_frame->is_occupied = true;
        empty_frame->second_chance = false;
        empty_frame->t = thread_current ();
        f_table->num_free--;
        lock_release (&f_table->ft_lock);

        return page;
    }
    else
    {
        return NULL;
    }
}

struct frame *
get_frame (void *page)
{
    struct frame *the_frame = NULL;
    lock_acquire (&f_table->ft_lock);
    bool found = false;
    int i = 0;
    while (i < ft_max && !found)
    {
        struct frame *cur_frame = f_table->frames + i;
        if (cur_frame != NULL && cur_frame->page == page)
        {
            //page found
            found = true;
            the_frame = cur_frame;
        }
        i++;
    }
    lock_release (&f_table->ft_lock);
    return the_frame;
}

/* Linear searches frame table array, finds unoccupied frame,
   and returns its index */
int f_table_get_index (void)
{
    if (f_table->num_free == 0)
        return FRAME_ERROR;

    struct frame *cur_frame = NULL;
    lock_acquire (&f_table->ft_lock);
    int i;
    for (i = 0; i < ft_max; i++)
    {
        cur_frame = f_table->frames + i;
        if (!cur_frame->is_occupied)
        {
            lock_release (&f_table->ft_lock);
            return i;
        }
    }
    ASSERT(1 == 0);
}

//FIXME: evicting from the table is easy.
//but how do you Remove references to the frame from any
//page table that refers to it.?
bool f_table_evict (void)
{
    // Second chance eviction algorithm

    lock_acquire (&f_table->ft_lock);
    bool found = false;

    while (!found)
    {
        /* Keep clock index in bounds */
        f_table->clock_hand = f_table->clock_hand % ft_max;

        struct frame *temp_frame = (f_table->frames + f_table->clock_hand);
        void *temp_page = temp_frame->page;
        struct sp_entry *temp_spte = temp_frame->spte;

        uint32_t *temp_pd = thread_current ()->pagedir;
        bool accessed = pagedir_is_accessed (temp_pd, temp_page);
        bool dirty = pagedir_is_dirty (temp_pd, temp_page);

        /* Not referenced and not written to 
           Evict Page! */
        if (!accessed) /* (0,0) */
        {
            if (dirty){
                /*page is dirty must write to disk, find out where*/
               if (temp_spte->page_loc == IN_FILE)
               {
                   /*write to filesys*/
                   lock_acquire (&file_sys_lock);
                   //TODO: double check accuracy of this
                   file_write(temp_spte->file, temp_page, temp_spte->offset);
               }
               else 
               {
                   /*in swap*/
                   //TODO:double check this too & block cur thread
                   temp_spte->swap_index = swap_out (temp_page);
                   temp_spte->page_loc = IN_SWAP;
                   
               }
            }
            /* move out from frame */
            pagedir_clear_page (temp_pd, temp_frame->page);
            palloc_free_page (temp_frame->page);
            found = true;
        }
        else
        {
            pagedir_set_accessed (temp_pd, temp_page, 0);
        }
        f_table->clock_hand++;
    }
    lock_release (&f_table->ft_lock);
    return found;
}

/* Miles Driving */
void f_table_free (void *page)
{
    struct frame *temp_frame = get_frame (page);
    temp_frame->is_occupied = false;
    temp_frame->spte = NULL;
    temp_frame->t = NULL;
    temp_frame->page = NULL;
}
/* End Driving */
