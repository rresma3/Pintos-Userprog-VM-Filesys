#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdbool.h>
#include <stdint.h>
#include "lib/kernel/hash.h"
#include "filesys/off_t.h"
#include "filesys/file.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"


#define IN_FILE 0
#define IN_SWAP 1

struct sp_entry {
    void *uaddr;
    int swap_index;

    uint8_t page_loc; // 0 == in filesys, 1 == in swap, 2
     
    bool writeable;
    bool is_loaded;  

    struct file *file;
    off_t offset;
    off_t bytes_read;
    off_t bytes_zero;
    int size;

    struct hash_elem elem;
};

static unsigned page_hash_func (const struct hash_elem *e, void *aux UNUSED);
static bool page_less_func (const struct hash_elem *a,
                            const struct hash_elem *b,
                            void *aux UNUSED);
static void page_action_func (struct hash_elem *e, void *aux UNUSED);

/* Initialization/Destruction of the supplemental page table management */
void sp_table_init (struct hash *spt);
void sp_table_destroy (struct hash *spt);

/* loading of a page into memory, whether from swap or filesys */
bool load_page (void *uaddr);
bool load_page_file (struct sp_entry *spte);
bool load_page_swap (struct sp_entry *spte);

/* Add a file supplemental page table entry to the current thread's
 * supplemental page table */
bool add_file_spte (void* uaddr, bool writeable, struct file *file,
                    off_t offset, off_t bytes_read, int size);

/* Given spt hash table and its key (uvaddr), find 
 * corresponding hash table entry */
static struct sp_entry* get_spt_entry (struct hash *spt, void *uaddr);

/* Allocate a stack page from where given address points */
bool grow_stack (void *uaddr);

#endif

