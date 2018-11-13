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
    return hash_int((int) spte->uaddr);
}

/* Hash Table functionality */
static bool 
page_less_func (const struct hash_elem *a, const struct hash_elem *b,
                void *aux UNUSED)
{
    struct sp_entry *a_entry = hash_entry (a, struct sp_entry, elem);
    struct sp_entry *b_entry = hash_entry (b, struct sp_entry, elem);

    // compare two supp page table entries w their diff address
    if (a_entry-> uaddr < b_entry->uaddr)
    {
        return true;
    }
    return false;
}

/* Hash Table functionality */
static void 
page_action_func (struct hash_elem *e, void *aux UNUSED)
{
    struct sp_entry *spte = hash_entry (e, struct sp_entry, elem);
    // TODO: check if in swap
    if (spte->page_loc == IN_SWAP)
    {
        // clear swap spot
    }
    free (spte);
}

/* SPT cleanup */
void 
sp_table_destroy (struct hash *spt)
{
    hash_destroy (spt, page_action_func);
}

/* load data to page based on initialized spte */
bool 
load_page (struct sp_entry *spte)
{
    bool success = false;
    if (spte != NULL)
    {
        // switch on location?
        uint8_t location = spte->page_loc;
        if (location == IN_FILE)
        {
            // in filesys
            success = load_page_file (spte);
        }
        else if (location == IN_SWAP)
        {
            // load from swap
            success = load_page_swap (spte);
        }
    }
    return success;
}

/* load data to page based on an initialized spte's file data */
bool 
load_page_file (struct sp_entry *spte)
{
    struct thread *cur_thread = thread_current ();
    void *addr = pagedir_get_page (cur_thread->pagedir, spte->uaddr);

    // ensure that file pos is at the correct position
    file_seek (spte->file, spte->offset);

    // start allcating the frame
    void *frame = f_table_alloc(PAL_USER);

    if (frame != NULL)
    {
        // check for bytes
        if (file_read_at (spte->file, frame, spte->bytes_read, spte->offset)
            != spte->bytes_read)
            {
                f_table_free (frame);
                return false;
            }
        memset (frame + spte->bytes_read, 0, spte->size);
        
        bool success = false;
        success = install_page (spte->uaddr, frame, spte->writeable);

        if (!success)
        {
            // failed to install page
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

/* load data to page based on an initialized spte's swap data */
bool
load_page_swap (struct sp_entry *spte)
{
    struct thread *cur_thread = thread_current ();
    void *addr = pagedir_get_page (cur_thread->pagedir, spte->uaddr);

    // start allcating the frame
    void *frame = f_table_alloc(PAL_USER);

    if (frame != NULL)
    {
        // Map user page to newly allocated frame
        bool success = false;
        success = install_page (spte->uaddr, frame, spte->writeable);

        if (!success)
        {
            // failed to install page
            f_table_free (frame);
            return false;
        }

        /* swap data from disk into physical memory */
        swap_in (spte->swap_index, spte->uaddr);

        // TODO: updating page location
        if (spte->page_loc == IN_SWAP)
        {
            
        }
        else if (spte->page_loc == IN_FILE)
        {

        }

        spte->is_loaded = true;
        return true;
    }
    else
    {
        return false;
    }
}

/* Add file supp. page table entry to supplemental page table */
bool 
add_file_spte (void *uaddr, bool writeable, struct file *file,
              off_t offset, off_t bytes_read, int size)
{
    struct sp_entry *spte = malloc (sizeof (struct sp_entry));
    if (spte != NULL)
    {
        spte->file = file;
        spte->offset = offset;
        spte->bytes_read = bytes_read;
        spte->size = size;
        spte->writeable = writeable;
        spte->is_loaded = false;
        spte->page_loc = IN_FILE;
        spte->uaddr = uaddr;

        struct hash_elem *save = NULL;
        save = hash_insert (&thread_current ()->spt, &spte->elem);
        if (save != NULL)
        {
            // able to insert in hash table
            return true;
        }
        return false;
    }
    else
    {
        return false;
    }
    return false;
}

/* Given spt hash table and its key (uvaddr), find 
 * corresponding hash table entry */
static struct sp_entry* 
get_spt_entry (struct hash *spt, void *uaddr)
{
    struct sp_entry spte;
    spte.uaddr = pg_round_down (uaddr);

    struct hash_elem *curr = hash_find (spt, &spte.elem);
    if (curr != NULL)
    {
        return hash_entry (curr, struct sp_entry, elem);
    }
    return NULL;
}

/* Allocate a stack page from where given address points */
bool 
grow_stack (void *uaddr)
{
    // TODO:
    return false;
}
