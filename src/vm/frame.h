/**SPT: Don’t forget to consider how many processes might touch the SPT
    *FT: How many processes could concurrently access the frame table?
    */


#ifndef VM_FRAME_H
#define VM_FRAME_H

#define ft_max 992

#include <stdbool.h>
#include <stdint.h>
#include <list.h>
#include "threads/synch.h"


struct frame {
    bool is_occupied;
    void *page;
    struct spt_entry *spte;
    struct thread *t;
    bool second_chance;
};


/*tracks the frames of phys memory and
keeps track of where the clock hand is for
eviction. The frames exist in the array*/
struct frame_table {
    //TODO: get actual macro
    struct frame *frames;
    struct lock ft_lock;
    int clock_hand;
    int num_free;
    
};




struct frame* get_frame (void *page);
void f_table_init (void);
void* f_table_alloc (enum palloc_flags flag);
int f_table_get_index (void);
bool f_table_evict (void);
void f_table_free (void *page);

#endif
