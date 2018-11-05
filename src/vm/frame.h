#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdbool.h>
#include <stdint.h>
#include <list.h>
#include "threads/synch.h"

struct lock f_table_lock;
struct list f_table;

struct frame {
    void *page;
    bool is_occupied;
    struct spt_entry *spte;
    struct thread *t;
    bool is_evictable;
    struct list_elem elem;
}

void f_table_init (void);
void* f_table_alloc (enum palloc_flags flag);
void f_table_free (void *page);
void f_table_add (void *page);
bool f_table_evict (void *frame);

#endif