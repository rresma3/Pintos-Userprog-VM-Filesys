#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "vm/frame.h"

void f_table_init (void) 
{
    list_init (&f_table);
    lock_init (&f_table_lock);
}

void* f_table_alloc (enum palloc_flags flag) 
{
    if (flag != NULL && flag == PAL_USER)
    {
        void *frame = palloc_get_page(flag);
        if (frame != NULL)
        {
            f_table_add (frame);
        } 
        else
        {
            if (!f_table_evict(frame))
            {
                // error somehow
            }
        }
        return frame;
    } 
    else 
    {
        return NULL;
    }
}

void f_table_free (void *page)
{
    struct list_elem *e;

    lock_acquire (&f_table_lock);
    for (e = list_begin (&f_table); e != list_end (&f_table);
        e = list_next(e))
        {
            struct frame *curr_frame = list_entry (e, struct frame, elem);
            if (curr_frame->page == page)
            {
                list_remove (e);
                // may or may not need to free here
                free (curr_frame);
                break;
            }
        }
    lock_release (&f_table_lock);
    palloc_free_page (page);
}

struct frame* get_frame (void *page)
{
    lock_acquire (&f_table_lock);
    struct list_elem * e = NULL;
    for (e = list_begin (&f_table);
         e != list_end (&f_table); e = list_next (e))
    {
        /* save reference to our current child */
        struct frame *result = list_entry(e, struct frame, elem);
        ASSERT (result != NULL);

        /* check if the tid's match, if so we have found our child */
        // TODO:
        // checking if pages are the same
        if (true)
        {
            lock_release (&f_table_lock);
            return result;
        }
    }
    lock_release (&f_table_lock);
    return NULL;
}

void f_table_add (void *page)
{
    struct frame *curr_frame = malloc (sizeof (struct frame));
    curr_frame->page = page;
    curr_frame->t = thread_current ();

    lock_acquire (&f_table_lock);
    list_push_back (&f_table, &curr_frame->elem);
    lock_release (&f_table_lock);
}

bool f_table_evict (void *frame)
{
    // TODO:
    // Second chance eviction algorithm
    return false;
}