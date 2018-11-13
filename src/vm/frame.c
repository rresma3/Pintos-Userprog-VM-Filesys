#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "vm/frame.h"
#include "userprog/pagedir.h"
#include <stdio.h>

struct frame_table *f_table;

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

void *f_table_alloc (enum palloc_flags flag)
{
    ASSERT (flag == PAL_USER || flag == (PAL_ZERO | PAL_USER));
    if (flag == PAL_USER || flag == (PAL_ZERO | PAL_USER))
    {
        int index = f_table_get_index ();
        void *page = NULL;
        if (index != -1)
        {
            page = palloc_get_page (flag);
            ASSERT (page != NULL);

            lock_acquire (&f_table->ft_lock);
            struct frame *empty_frame = f_table->frames + index;
            empty_frame->page = page;
            empty_frame->is_occupied = true;
            empty_frame->is_evictable = true;
            empty_frame->t = thread_current ();
            f_table->num_free--;
            lock_release (&f_table->ft_lock);
        }
        /* Evict a frame for page */
        else
        {
            f_table_evict ();
        }

        return page;
    }
    else
    {
        return NULL;
    }
}

struct frame *get_frame(void *page)
{
    struct frame *the_frame = NULL;
    lock_acquire(&f_table->ft_lock);
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
    }
    lock_release(&f_table->ft_lock);
    return the_frame;
}

/* Linear searches frame table array, finds unoccupied frame,
   and returns its index */
int f_table_get_index (void)
{
    if (f_table->num_free == 0)
    {
        return -1;
    }
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
        
        uint32_t *temp_pd = thread_current ()->pagedir;
        bool accessed = pagedir_is_accessed (temp_pd, temp_page);
        bool dirty = pagedir_is_dirty (temp_pd, temp_page);

        /* Not referenced and not written to 
           Replace Page! */
        if (!accessed && !dirty) /* (0,0) */
        {
            
        }
        /* Not referenced but written to 
           Write to disk, but may not be needed again */
        else if (!accessed && dirty) /* (0,1) */
        {

        }
        /* Recently referenced and not written to 
           May be needed again soon, but doesn't need to be written to disk */
        else if (accessed && !dirty) /* (1,0) */
        {
            pagedir_set_accessed (temp_pd, temp_page, 0);
        }
        /* Recently referrenced and recently written to 
           Don't evict */
        else /* (1,1) */
        {
            pagedir_set_accessed (temp_pd, temp_page, 0);
        }

        f_table->clock_hand++;
    }
    return false;
}

void f_table_free (void *page)
{
    page++;
    return;
}
