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

/* Frame table initialization
Miles driving */
void 
f_table_init (void)
{
    /*malloc for the entire struct */
    f_table = malloc (sizeof (struct frame_table));
    ASSERT (f_table != NULL);
    /*get blank spaces for the array of frames*/
    f_table->frames = (struct frame *) calloc (sizeof (struct frame), ft_max);
    ASSERT (f_table->frames != NULL);
    /*init lock for sync */
    lock_init (&f_table->ft_lock);
    lock_init (&evict_lock);
    /*set clock hand to 0 for evict algo */
    f_table->clock_hand = 0;
    /*keep track of number of free spaces */
    f_table->num_free = ft_max;
}
/*end miles driving*/

/* Allocation of a frame within physical memory, should be allocated
   within the User pool
   Miles driving */
struct frame *
f_table_alloc (enum palloc_flags flag)
{
    /*check that we are allocating from user pool */
    ASSERT (flag == PAL_USER || flag == (PAL_ZERO | PAL_USER));
    lock_ft ();
    if (flag == PAL_USER || flag == (PAL_ZERO | PAL_USER))
    {
        /*get the index of a free frame */
        int index = f_table_get_index ();
        void *page = palloc_get_page (flag);
        if (index == FRAME_ERROR || page == NULL)
        { /* Evict a frame for page */
            if (!f_table_evict ())
            {
                PANIC ("ERROR: unable to evict");
            }
            /*try to allocate the page again*/
            page = palloc_get_page (flag);
            /*set the index to the frame we just freed*/
            index = f_table->clock_hand - 1;
        }
        ASSERT (page != NULL);

        /*access the recently freed frame & occupy it*/
        struct frame *empty_frame = f_table->frames + index;
        empty_frame->page = page;
        empty_frame->is_occupied = true;
        empty_frame->pinned = false;
        empty_frame->t = thread_current ();
        f_table->num_free--;
        
        unlock_ft ();
        return empty_frame;
    }
    else
    {
        /*can't palloc from anything but user pool. return null page*/
        unlock_ft ();
        return NULL; 
    }
}
/*End Miles driving */

/* Linear search frame table to find page's corresponding frame
Ryan driving */
struct frame *
get_frame (void *page)
{
    struct frame *the_frame = NULL;
    bool found = false;
    int i = 0;
    /* Loop through frame table until page's frame is found */
    while (i < ft_max && !found)
    { 
        /*Get the current frame */
        struct frame *cur_frame = (f_table->frames) + i;
        if (cur_frame != NULL && cur_frame->page == page)
        { 
            /* page found */
            found = true;
            the_frame = cur_frame;
        }
        i++;
    }
    return the_frame;
}

/* Linear searches frame table array, finds unoccupied frame,
   and returns its index */
int 
f_table_get_index (void)
{
    /*first save time and check if we don't have free frames*/
    if (f_table->num_free == 0)
    {
        return FRAME_ERROR;
    }

    struct frame *cur_frame = NULL;
    int i;
    for (i = 0; i < ft_max; i++)
    {
        /*get the current frame */
        cur_frame = (f_table->frames) + i;
        if (!cur_frame->is_occupied)
        { 
            /* Found free frame */
            return i;
        }
    }
    /*Shouldn't reach this because we checked that already*/
    PANIC ("ERROR: No free frame found!\n");
}
/*end Ryan driving */

/*Evicts a page from the frame table using the clock algorithm*/
bool 
f_table_evict (void)
{
    /*Sam driving */
    struct frame *temp_frame = NULL;
    void *temp_page = NULL;
    struct sp_entry *temp_spte = NULL;
    struct thread *cur_thread = NULL;
    uint32_t *temp_pd = NULL;
    
    bool accessed = false;
    bool dirty = false;
    bool found = false;

    /*loop through the frame table*/
    while (!found)
    {
        /* get necessary structs */
        temp_frame = (f_table->frames + f_table->clock_hand);
        temp_page = temp_frame->page;
        temp_spte = temp_frame->spte;
        cur_thread = temp_frame->t;
        temp_pd = cur_thread->pagedir;

        /*first check our frame isnt pinned for write and it is ocupied*/
        if (temp_frame->pinned == false && temp_frame->is_occupied == true)
        {
            /*get necessary bits*/
            accessed = pagedir_is_accessed (temp_pd, temp_page);
            dirty = pagedir_is_dirty (temp_pd, temp_page);
            
            if (!accessed) /* (0,0) */
            {
                /*found a page to evict */
                if (dirty)
                {
                    /*page is dirty
                    find out where to write it to on disk
                    Brian driving*/
                    if (temp_spte->page_loc == IN_FILE)
                    {
                        /*write to filesys*/
                        file_write (temp_spte->file, temp_page,
                                    temp_spte->offset);
                    }
                    else 
                    {
                        /*write to swap*/
                        int index = swap_out (temp_page);
                        if (index == SWAP_ERROR)
                        {
                            /*swap failed*/
                            return false;
                        }
                        /*save the index of where we swaped
                        and update the location of the page in the
                        spte*/
                        temp_spte->swap_index = index;
                        temp_spte->page_loc = IN_SWAP;
                    }
                    /*end Brian driving*/
                }
                /* move out from frame
                update pagedir and free actual page */
                pagedir_clear_page (temp_pd, temp_spte->uaddr); 
                f_table_free (temp_frame);        
                found = true;
            }
            else
            {
                /* update clock bit */
                pagedir_set_accessed (temp_pd, temp_page, 0);
            }
        }
        /*wrap clock hand around to begininng*/
        f_table->clock_hand = ((f_table->clock_hand) + 1) % ft_max;
    }
    return found;
}
/* End Sam driving */


/*Frees a frame of its page and updates meta data  
Miles Driving */
void f_table_free (struct frame *frame)
{
    lock_ft ();
    frame->is_occupied = false;
    frame->spte = NULL;
    frame->t = NULL;
    palloc_free_page (frame->page);
    unlock_ft ();
}
/* End Miles Driving */


/* Brian Driving 
Wrapper for our file table lock_acquire () */
void lock_ft (void)
{
    if (!lock_held_by_current_thread (&file_sys_lock) &&
        !lock_held_by_current_thread (&f_table->ft_lock))
        {
            lock_acquire (&f_table->ft_lock);
        }   
}

/* Wrapper for our file table lock_release () */
void unlock_ft (void)
{
    if (lock_held_by_current_thread (&f_table->ft_lock))
    {
        lock_release (&f_table->ft_lock);
    }
}
/*End Brian Driving */
