#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "list.h"

void syscall_init (void);

struct file_elem
{
    int fd;
    struct file *file;
    struct list_elem elem;
 };

#endif /* userprog/syscall.h */
