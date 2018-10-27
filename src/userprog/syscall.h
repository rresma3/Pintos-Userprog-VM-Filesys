#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "list.h"

void syscall_init (void);

/*locks file access*/
struct lock file_sys_lock;


struct file_elem
{
    int fd;
    struct file *file;
    struct list_elem elem;
 };

#endif /* userprog/syscall.h */
