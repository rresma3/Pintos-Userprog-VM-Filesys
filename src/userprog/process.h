#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

/* exit and wait helper functions */
struct child* get_child (tid_t tid, struct thread *cur_thread);
void free_resources (struct thread *t);

/* Ryan Driving */
/* child struct for easy access to child's resources */
struct child
{
    struct list_elem child_elem;
    tid_t child_tid;
    int waited_on;
    int child_exit_code;
};
/* End Ryan Driving */ 

#endif /* userprog/process.h */
