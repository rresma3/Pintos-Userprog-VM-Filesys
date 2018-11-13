#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "vm/frame.h"
#include <stdio.h>

struct frame_table *f_table;

void f_table_init (void) 
{
    f_table = malloc (sizeof (struct frame_table));
    ASSERT (f_table != NULL);
    f_table->frames = (struct frame*) calloc (sizeof(struct frame), ft_max);
    ASSERT (f_table->frames != NULL);
    lock_init (&f_table->ft_lock);
    f_table->num_free = ft_max;
}   

void* f_table_alloc (enum palloc_flags flag) 
{
    ASSERT (flag == PAL_USER);
    printf ("allocating frame in table\n");
    if (flag == PAL_USER)
    {
        void *page = palloc_get_page(flag);
        if (page != NULL)
        {
            int idx = f_table_get_idx ();
            if (idx != -1)
            {
                lock_acquire (&f_table->ft_lock);
                struct frame *empty_frame = f_table->frames + idx;
                empty_frame->page = page;
                empty_frame->is_occupied = true;
                empty_frame->t = thread_current ();
                f_table->num_free--;
                
            }
            else 
            {
                // TODO:need to evict a frame
                ASSERT (1 == 0);
            }

        } 
        else
        {
            ASSERT (0);
        }
        return page;
    } 
    else 
    {
        return NULL;
    }
}

struct frame* get_frame (void *page)
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
    }
    lock_release (&f_table->ft_lock);
    return the_frame;
}

int f_table_get_idx (void)
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
    ASSERT (1 == 0);
}

//FIXME: evicting from the table is easy.
//but how do you Remove references to the frame from any 
//page table that refers to it.?
bool f_table_evict ( void *page UNUSED)
{
    // TODO:
    // Second chance eviction algorithm
    return false;
}

void f_table_free (void *page)
{
    page++;
    return;
}