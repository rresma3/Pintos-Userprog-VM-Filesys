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
    size_t swap_index;

    uint8_t page_loc; // 0 == in filesys, 1 == in swap, 2
     
    bool writeable;
    bool is_loaded;  

    struct file *file;
    off_t offset;
    off_t bytes_read;
    off_t bytes_zero;

    struct hash_elem elem;
};

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
                    off_t offset, off_t bytes_read, off_t bytes_zero);

/* debugging */
void print_spte_stats (struct sp_entry *spte);

/* Allocate a stack page from where given address points */
bool grow_stack (void *uaddr);

#endif
