#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute(const char *file_name);
int process_wait(tid_t);
void process_exit(void);
void process_activate(void);

/* Ryan Driving */
// child struct for easy access to child's resources
struct child
{
    tid_t child_tid;
    struct list_elem child_elem;
    int exited;
    int child_exit_code;
};
/* End Driving */

#endif /* userprog/process.h */
