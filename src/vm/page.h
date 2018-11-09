#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdbool.h>
#include <stdint.h>
#include <hash.h>
#include "filesys/off_t.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"


#define IN_FILE 0
#define IN_SWAP 1

struct sp_entry {
    int swap_index;

    uint8_t page_loc; // 0 == in filesys, 1 == in swap, 2 
    bool writeable;
    bool is_loaded;
    void *uaddr;

    struct file *file;
    off_t offset;
    off_t bytes_read;
    int size;

    struct hash_elem elem;
};

static unsigned page_hash_func (const struct hash_elem *e, void *aux UNUSED);
static bool page_less_func (const struct hash_elem *a,
                            const struct hash_elem *b,
                            void *aux UNUSED);
static void page_action_func (struct hash_elem *e, void *aux UNUSED);

void sp_table_init (struct hash *spt);
void sp_table_destroy (struct hash *spt);

bool load_page (void *uaddr);
bool load_file (struct sp_entry *spte);
// bool load_swap (struct sp_entry *spte);
bool add_file_spt (void* uaddr, bool writeable, struct file *file,
                    off_t offset, off_t bytes_read, int size);
static struct sp_entry* get_sp_entry (void *uaddr);


#endif

