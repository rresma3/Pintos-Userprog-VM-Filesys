#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include <hash.h>
#include "vm/frame.h"
#include "vm/page.h"
#include <string.h>

void sp_table_init (struct hash *spt)
{
    hash_init (spt, page_hash_func, page_less_func, NULL);
}

static unsigned page_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
    struct sp_entry *spte = hash_entry (e, struct sp_entry, elem);
    return hash_int((int) spte->uaddr);
}

static bool page_less_func (const struct hash_elem *a,
                            const struct hash_elem *b,
                            void *aux UNUSED)
{
    struct sp_entry *a_entry = hash_entry (a, struct sp_entry, elem);
    struct sp_entry *b_entry = hash_entry (b, struct sp_entry, elem);

    if (a_entry-> uaddr < b_entry->uaddr)
    {
        return true;
    }
    return false;
}

static void page_action_func (struct hash_elem *e, void *aux UNUSED)
{
    struct sp_entry *spte = hash_entry (e, struct sp_entry, elem);
    free (spte);
}

void sp_table_destroy (struct hash *spt)
{
    hash_destroy (spt, page_action_func);
}

bool load_page (void *uaddr)
{
    struct sp_entry *spte = get_sp_entry(uaddr);
    if (spte != NULL)
    {
        uint8_t location = spte->page_loc;
        bool success = false;
        if (location == 0)
        {
            // in filesys
            success = load_file (spte);
        }
        else if (location == 1)
        {
            // load from swap?
        }
        return success;
    }
    else
    {
        return false;
    }
}

bool load_file (struct sp_entry *spte)
{
    void *addr = pagedir_get_page (thread_current ()->pagedir, spte->uaddr);

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

bool add_file_spt (void *uaddr, bool writeable, struct file *file,
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
}

static struct sp_entry* get_sp_entry (void *uaddr)
{
    struct sp_entry spte;
    spte.uaddr = pg_round_down (uaddr);

    struct hash_elem *curr = hash_find (&thread_current ()->spt, &spte.elem);
    if (curr != NULL)
    {
        return hash_entry (curr, struct sp_entry, elem);
    }
    return NULL;
}
