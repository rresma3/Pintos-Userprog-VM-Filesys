#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "list.h"

/* Brian Driving */
void syscall_init (void);
void error_exit (int exit_status);
/* End Driving */

/*locks file access*/
struct lock file_sys_lock;


struct file_elem
{
    int fd;
    struct file *file;
    struct list_elem elem;
 };
 /* End Driving */

#endif /* userprog/syscall.h */
