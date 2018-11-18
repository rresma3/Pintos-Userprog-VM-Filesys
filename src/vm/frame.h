
#ifndef VM_FRAME_H
#define VM_FRAME_H

#define ft_max 367

#include <stdbool.h>
#include <stdint.h>
#include <list.h>
#include "threads/synch.h"

#define FRAME_ERROR -1

/*Sam driving */
struct frame 
{
    bool is_occupied;
    void *page;
    struct sp_entry *spte;
    struct thread *t;
    bool pinned;
};

struct lock evict_lock;

/*tracks the frames of phys memory and
keeps track of where the clock hand is for
eviction. The frames exist in the array*/
struct frame_table 
{
    struct frame *frames;
    struct lock ft_lock;
    int clock_hand;
    int num_free;
};

/*End Sam Driving*/
struct frame* get_frame (void *page);
void f_table_init (void);
struct frame* f_table_alloc (enum palloc_flags flag);
int f_table_get_index (void);
bool f_table_evict (void);
void reset_clock_hand (void);
void f_table_free (struct frame *frame);

void lock_ft (void);
void unlock_ft (void);

#endif
