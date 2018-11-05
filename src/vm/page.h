#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
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
}

void sp_table_init (struct hash *spt);
void sp_table_destroy (struct hash *spt);

bool load_page (void *uaddr);
bool load_file (struct sp_entry *spte);
// bool load_swap (struct sp_entry *spte);
bool add_file_spt (void* uaddr, bool writeable, struct file *file,
                    off_t offset, off_t bytes_read, int size);