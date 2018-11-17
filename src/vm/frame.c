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
    f_table->frames = (struct frame *) calloc (sizeof (struct frame), ft_max);
    ASSERT (f_table->frames != NULL);
    lock_init (&f_table->ft_lock);
    lock_init (&evict_lock);
    f_table->clock_hand = 0;
    f_table->num_free = ft_max;
}

/* Allocation of a frame within physical memory, should be allocated
   within the User pool */
struct frame *
f_table_alloc (enum palloc_flags flag)
{
    ASSERT (flag == PAL_USER || flag == (PAL_ZERO | PAL_USER));
    lock_acquire (&f_table->ft_lock);
    if (flag == PAL_USER || flag == (PAL_ZERO | PAL_USER))
    {
        //TODO: double check on logic of evict
        int index = f_table_get_index ();
        //printf ("our index is %d\n", index);
        void *page = palloc_get_page (flag);
        if (index == FRAME_ERROR || page == NULL)
        { /* Evict a frame for page */
            //lock_release (&f_table->ft_lock);
            //lock_acquire (&evict_lock);
            if (!f_table_evict ())
            {
                PANIC ("ERROR: unable to evict");
            }
            //lock_release (&evict_lock);
            //lock_acquire (&f_table->ft_lock);
            page = palloc_get_page (flag);
            index = f_table->clock_hand - 1;
        }
        //page = palloc_get_page (flag);
        ASSERT (page != NULL);

        struct frame *empty_frame = f_table->frames + index;
        empty_frame->page = page;
        empty_frame->is_occupied = true;
        empty_frame->pinned = false;
        empty_frame->t = thread_current ();
        f_table->num_free--;
        // printf ("%d free frames left\n", f_table->num_free);
        lock_release (&f_table->ft_lock);

        return empty_frame;
    }
    else
    {
        lock_release (&f_table->ft_lock);
        return NULL; 
    }
}

/* Linear search frame table to find page's corresponding frame */
struct frame *
get_frame (void *page)
{
    struct frame *the_frame = NULL;
    //lock_acquire (&f_table->ft_lock);
    bool found = false;
    int i = 0;
    /* Loop through frame table until page's frame is found */
    while (i < ft_max && !found)
    { 
        struct frame *cur_frame = (f_table->frames) + i;
        if (cur_frame != NULL && cur_frame->page == page)
        { /* page found */
            found = true;
            the_frame = cur_frame;
        }
        i++;
    }
    //lock_release (&f_table->ft_lock);
    return the_frame;
}

/* Linear searches frame table array, finds unoccupied frame,
   and returns its index */
int 
f_table_get_index (void)
{
    if (f_table->num_free == 0)
    {
        return FRAME_ERROR;
    }
    struct frame *cur_frame = NULL;
    //lock_acquire (&f_table->ft_lock);
    int i;
    for (i = 0; i < ft_max; i++)
    {
        cur_frame = (f_table->frames) + i;
        if (!cur_frame->is_occupied)
        { /* Found free frame */
            //printf("\nFOUND FREE FRAME: %d\n", i);
            return i;
        }
    }
    PANIC ("ERROR: No free frame found!\n");
}

bool f_table_evict (void)
{
    printf ("\n\nIN EVICT\n\n");
    struct frame *temp_frame = NULL;
    void *temp_page = NULL;
    struct sp_entry *temp_spte = NULL;
    struct thread *cur_thread = NULL;
    uint32_t *temp_pd = NULL;
    
    bool accessed = false;
    bool dirty = false;

    bool found = false;
    while (!found)
    {
        /* Keep clock index in bounds */
        //f_table->clock_hand = f_table->clock_hand % ft_max;

        /* get necessary structs */
        temp_frame = (f_table->frames + f_table->clock_hand);
        temp_page = temp_frame->page;
        temp_spte = temp_frame->spte;
        cur_thread = temp_frame->t;
        temp_pd = cur_thread->pagedir;
        //printf ("temp_frame's address? addr: 0x%x\n", temp_frame->page);
        if (temp_frame->pinned == false && temp_frame->is_occupied == true)
        {
            accessed = pagedir_is_accessed (temp_pd, temp_page);
            dirty = pagedir_is_dirty (temp_pd, temp_page);
            /* Not referenced and not written to 
               Evict Page! */
            if (!accessed) /* (0,0) */
            {
                if (dirty)
                {
                    printf ("found a dirty page\n");
                    /*page is dirty must writeto disk, find out where*/
                    printf ("temp_spte address?: 0x%x\n", temp_spte->uaddr);
                    if (temp_spte->page_loc == IN_FILE)
                    {
                        //printf("in filesys\n");
                        /*write to filesys*/
                        lock_acquire (&file_sys_lock);
                        //TODO: double check accuracy of this
                        file_write (temp_spte->file, temp_page,
                                    temp_spte->offset);
                        lock_release (&file_sys_lock);
                        //write ()
                    }
                    else 
                    {
                        //printf("in swap\n");
                        /*in swap*/
                        //TODO:double check this too & block cur thread
                        int index = swap_out (temp_page);
                        if (index == SWAP_ERROR)
                        {
                            return false;
                        }
                        temp_spte->swap_index = index;
                        temp_spte->page_loc = IN_SWAP;
                   
                    }
                }
                /* move out from frame */
                pagedir_clear_page (temp_pd, temp_frame->page);
                f_table_free (temp_frame->page);
                found = true;
                //printf ("\n\nEVICTED A PAGE\n\n");
            }
            else
            {
                pagedir_set_accessed (temp_pd, temp_page, 0);
            }
        }
        f_table->clock_hand = ((f_table->clock_hand) + 1) % ft_max;
        //printf ("time around: %d\n", f_table->clock_hand);
    }
    return found;
}

void reset_clock_hand ()
{
    //f_table->clock_hand = 
}

/* Miles Driving */
void f_table_free (struct frame *frame)
{
    //printf ("IN F_TABLE_FREE\n");
    lock_acquire (&f_table->ft_lock);
    frame->is_occupied = false;
    frame->spte = NULL;
    frame->t = NULL;
    palloc_free_page (frame->page);
    lock_release (&f_table->ft_lock);
}
/* End Driving */
