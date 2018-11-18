#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/pte.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "lib/kernel/hash.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include <string.h>
#include <stdio.h>

/* Brian Driving */
static unsigned page_hash_func (const struct hash_elem *e, void *aux UNUSED);
static bool page_less_func (const struct hash_elem *a,
                            const struct hash_elem *b,
                            void *aux UNUSED);
static void page_action_func (struct hash_elem *e, void *aux UNUSED);
/* End driving */


/* Miles driving */
/* SPT initialization */
void 
sp_table_init (struct hash *spt)
{
    hash_init (spt, page_hash_func, page_less_func, NULL);
}

/* Hash Table functionality */
static unsigned 
page_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
    struct sp_entry *spte = hash_entry (e, struct sp_entry, elem);
    return hash_int ((int) spte->uaddr);
}

/* Hash Table functionality 
Sam driving*/
static void 
page_action_func (struct hash_elem *e, void *aux UNUSED)
{
    struct sp_entry *spte = hash_entry (e, struct sp_entry, elem);
    free (spte);
}
/*End driving*/

/* Hash Table functionality */
static bool 
page_less_func (const struct hash_elem *a, const struct hash_elem *b,
                void *aux UNUSED)
{
    struct sp_entry *a_entry = hash_entry (a, struct sp_entry, elem);
    struct sp_entry *b_entry = hash_entry (b, struct sp_entry, elem);

    /* compare two supp page table entries w their diff address */
    if (a_entry-> uaddr < b_entry->uaddr)
    {
        return true;
    }
    return false;
}
/* End driving */

/* Sam driving */
/* SPT cleanup */
void 
sp_table_destroy (struct hash *spt)
{
    hash_destroy (spt, page_action_func);
}
/* End driving */

/* Ryan driving */
/* load data to page based on initialized spte */
bool 
load_page (struct sp_entry *spte)
{
    bool success = false;
    if (spte != NULL)
    {
        /* check to see where it is */
        uint8_t location = spte->page_loc;
        if (location == IN_FILE)
        {
            /* in the filesystem */
            success = load_page_file (spte);
        }
        else if (location == IN_SWAP)
        {
            /* in the swap */
            success = load_page_swap (spte);
        }
    }
    return success;
}
/* End driving */

/* Brian driving */
/* load data to page based on an initialized spte's file data */
bool 
load_page_file (struct sp_entry *spte)
{
    /* ensure that file pos is at the correct position */
    file_seek (spte->file, spte->offset);

    /* start allocating the frame */
    struct frame *frame = f_table_alloc (PAL_USER | PAL_ZERO);
    
    if (frame != NULL)
    {
        /* check for bytes */
        frame->spte = spte;
        if (file_read_at (spte->file, frame->page, spte->bytes_read, 
            spte->offset) != spte->bytes_read)
            {
                f_table_free (frame);
                return false;
            }
        memset (frame->page + spte->bytes_read, 0, spte->bytes_zero);

        bool success = false;
        success = install_page (spte->uaddr, frame->page, spte->writeable);
        
        if (!success)
        { 
            /* failed to install page */
            f_table_free (frame);
            return false;
        }
        spte->is_loaded = true;
        return true;
    }
    else
    {
        return false;
    }
}
/* End driving */

/* Ryan driving */
/* load data to page based on an initialized spte's swap data */
bool
load_page_swap (struct sp_entry *spte)
{

    /* start allcating the frame */
    struct frame *frame = f_table_alloc (PAL_USER | PAL_ZERO);

    if (frame != NULL)
    {
        /* Map user page to newly allocated frame */
        bool success = false;
        success = install_page (spte->uaddr, frame, spte->writeable);
        pagedir_set_dirty (thread_current ()->pagedir, spte->uaddr, 1);
        pagedir_set_accessed (thread_current ()->pagedir, spte->uaddr, 1);
        /* End driving */

        /* Sam driving */
        if (!success)
        { 
            /* failed to install page */
            f_table_free (frame);
            return false;
        }
        /* swap data from disk into physical memory */
        swap_in (spte->swap_index, spte->uaddr);
        spte->page_loc = IN_FILE;
        spte->is_loaded = true;
        return true;
    }
    else
    {
        return false;
    }
}
/* End driving */

/* Miles driving */
/* Add file supp. page table entry to supplemental page table */
bool 
add_file_spte (void *uaddr, bool writeable, struct file *file,
              off_t offset, off_t bytes_read, off_t bytes_zero)
{
    struct sp_entry *spte = malloc (sizeof (struct sp_entry));
    if (spte != NULL)
    {
        spte->file = file;
        spte->offset = offset;
        spte->bytes_read = bytes_read;
        spte->bytes_zero = bytes_zero;
        spte->writeable = writeable;
        spte->is_loaded = false;
        spte->page_loc = IN_FILE;
        spte->uaddr = uaddr;

        return (hash_insert (&thread_current ()->spt, &spte->elem) == NULL);
    }
    else
    {
        return false;
    }
}
/* End driving */

/* Sam driving */
/* Given spt hash table and its key (uvaddr), find 
   corresponding hash table entry */
struct sp_entry* 
get_spt_entry (struct hash *spt, void *uaddr)
{
    lock_acquire (&evict_lock);
    struct sp_entry spte;
    spte.uaddr = pg_round_down (uaddr);

    struct hash_elem *curr = hash_find (spt, &spte.elem);
    if (curr != NULL)
    {
        struct sp_entry *sp_ptr =  hash_entry (curr, struct sp_entry, elem);
        lock_release (&evict_lock);
        return sp_ptr;
    }
    lock_release (&evict_lock);
    return NULL; 
}
/* End driving */

/* Ryan driving */
/* Allocate a stack page from where given address points */
bool
grow_stack (void *uaddr)
{
    if ((PHYS_BASE - pg_round_down (uaddr)) > MAX_STACK)
    {
        return false;
    }

    struct sp_entry *spte = malloc (sizeof (struct sp_entry));
    if (spte != NULL)
    {
        spte->is_loaded = true;
        spte->writeable = true;
        spte->page_loc = IN_SWAP;
        spte->uaddr = pg_round_down (uaddr);

        struct frame *frame = f_table_alloc (PAL_USER | PAL_ZERO);
        frame->spte = spte;
        frame->pinned = true;

        if (frame == NULL)
        {
            free (spte);
            return false;
        }
        if (install_page (spte->uaddr, frame->page, spte->writeable) == false)
        {
            free (spte);
            f_table_free (frame);
            return false;
        }
        return (hash_insert (&thread_current ()->spt, &spte->elem) == NULL);

    }
    else
    {
        return false;
    }
}
/* End driving */
