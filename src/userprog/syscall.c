#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
/* Sam Driving */
#include <string.h>
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/synch.h"
#include "pagedir.h"
#include "devices/input.h"
#include "threads/malloc.h"
/* End Driving */

/* Miles Driving */
static void syscall_handler (struct intr_frame *);

static bool valid_ptr (int *ptr);
/* End Driving */

/* Brian Driving */
static void halt_handler (void);
static void exit_handler (struct intr_frame *f);
static void exec_handler (struct intr_frame *f);
static void wait_handler (struct intr_frame *f);
static void create_handler (struct intr_frame *f);
static void remove_handler (struct intr_frame *f);
static void open_handler (struct intr_frame *f);
static void filesize_handler (struct intr_frame *f);
static void read_handler (struct intr_frame *f);
static void write_handler (struct intr_frame *f);
static void seek_handler (struct intr_frame *f);
static void tell_handler (struct intr_frame *f);
static void close_handler (struct intr_frame *f);
static void error_exit (int exit_status);
/* End Driving */


/* Handles all syscalls */
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_sys_lock);
}


/* Brian Driving */
static void
syscall_handler (struct intr_frame *f) 
{
  int* my_esp = f->esp;
  
  if (!valid_ptr (my_esp))
  {
    /* BAD! */
    error_exit(-1);
  }
  int syscall_num = *(my_esp);

  switch (syscall_num)
  {
    case SYS_HALT :
      halt_handler ();
      break;
    case SYS_EXIT :
      exit_handler (f);
      break;
    case SYS_EXEC :
      exec_handler (f);
      break;
    case SYS_WAIT :
      wait_handler (f);
      break;
    case SYS_CREATE :
      create_handler (f);
      break;
    case SYS_REMOVE :
      remove_handler (f);
      break;
    case SYS_OPEN :
      open_handler (f);
      break;
    case SYS_FILESIZE :
      filesize_handler (f);
      break;
    case SYS_READ :
      read_handler (f);
      break;
    case SYS_WRITE :
      write_handler (f);
      break;
    case SYS_SEEK :
      seek_handler (f);
      // TODO: what is this
      // bool is_occupied;
      break;
    case SYS_TELL :
      tell_handler (f);
      break;
    case SYS_CLOSE :
      close_handler (f);
      break;
    default :
      error_exit (-1);
      break;
  }
}
/* End Driving */

/* Miles Driving */
/* Returns true iff ptr is properly allocated in user memory */
bool
valid_ptr (int *ptr)
{
  struct thread *cur = thread_current ();

  return !(ptr == NULL || is_kernel_vaddr (ptr) ||
           pagedir_get_page (cur->pagedir, ptr) == NULL);
}
/* End Driving */


/* Brian Driving */
/* Terminates Pintos by calling shutdown_power_off() */
static void
halt_handler ()
{
  shutdown_power_off ();
}
/* End Driving */


/* Ryan Driving */
/* Widely used method to handle bad pointer or invalid 
   arg on stack, exits the calling process with status of */
static void 
error_exit (int exit_status)
{
  struct thread *cur = thread_current ();
  cur->exit_code = exit_status;
  
  thread_exit ();
}
/* End Driving */


/* Ryan Driving */
/* Terminates the current user program, returning status to the kernel. 
   If the process's parent waits for it, this is the status that will be 
   returned. Conventionally, a status of 0 indicates success and nonzero
   values indicate errors. */
static void
exit_handler (struct intr_frame *f)
{
  /* grab the esp off intr_frame */
  int *my_esp = f->esp;

  if (valid_ptr (my_esp + 1))
  {
    thread_current ()->exit_code = *(my_esp + 1);
    thread_exit ();
  }
  else
  {
    error_exit (-1);
  }
}
/* End Driving */


/* Ryan Driving */
/* system call handler for wait: validates the pid passed in is a valid
   pid, then iff pid is still alive, waits until it terminates. Then, 
   returns the status that pid passed to exit. If pid did not call exit(),
   but was terminated by the kernel (e.g. killed due to an exception), 
   wait(pid) must return -1. */
static void 
wait_handler (struct intr_frame *f)
{
  int *my_esp = f->esp;
  /* check valid pointer */
  if (valid_ptr (my_esp + 1))
  {
    f->eax = process_wait (*(my_esp + 1));
  }
  else
  {
    error_exit (-1);
  }
}
/* End Driving */


/* Sam Driving */
/* Creates a new file called file initially initial_size bytes in size.
  Returns true if successful, false otherwise. Creating a new file does 
  not open it: opening the new file is a separate operation which would
  require a open system call. */
static void 
create_handler (struct intr_frame *f)
{
  int *my_esp = (int*) f->esp;
  if (valid_ptr (my_esp + 1) && valid_ptr (my_esp + 2)
      && valid_ptr ((int*) *(my_esp + 1)))
  { /* both args are valid. */
    lock_acquire (&file_sys_lock);

    /* Make args into clear var names */
    char *f_name = (char*) *(my_esp + 1);
    unsigned initial_size = (unsigned) *(my_esp + 2);

    f->eax = filesys_create (f_name, initial_size);
    lock_release (&file_sys_lock);
  }
  else
  {
    error_exit (-1);
  }
}
/* End Driving */


/* Brian Driving */
/* Returns the size, in bytes, of the file open as fd. */
static void 
filesize_handler (struct intr_frame *f)
{
  int *my_esp = (int*) f->esp;
  if (valid_ptr (my_esp + 1))
  {
    lock_acquire (&file_sys_lock);

    struct list_elem *iterator;
    struct file_elem *cur_file;
    struct list *cur_file_list = &thread_current ()->file_list;

    int fd = *(my_esp + 1);

    /* Loop thru file list until fd is found */
    for (iterator = list_begin (cur_file_list);
         iterator != list_end (cur_file_list);
         iterator = list_next (iterator))
      {
        cur_file = list_entry (iterator, struct file_elem, elem);
        if (cur_file != NULL && cur_file->fd == fd)
        {
          /* Set return value to size */
          f->eax = file_length (cur_file->file);
        }
      }
    lock_release (&file_sys_lock);
  }
  else
  {
    error_exit (-1);
  }
}
/* End Driving */


/* Miles Driving */
/* Reads size bytes from the file open as fd into buffer. Returns the number
   of bytes actually read (0 at end of file), or -1 if the file could not be 
   read (due to a condition other than end of file). fd 0 reads from the 
   keyboard using input_getc(). */
static void 
read_handler (struct intr_frame *f)
{
  int *my_esp = (int*) f->esp;
  /* Check second to last arg's content and then last arg */
  if (valid_ptr (my_esp + 1) && valid_ptr (my_esp + 2)
      && valid_ptr (my_esp + 3) && valid_ptr ((int*) *(my_esp + 2)))
  {
    /* Make args into clear var names */
    int fd = *(my_esp + 1);
    char **buf_ptr = (char**)(my_esp + 2);
    int size = *(my_esp + 3);
    char *buf = *buf_ptr;
    if (fd == 0)
    { 
      uint8_t *buff_ptr = (uint8_t*) buf;
      int i;
      for (i = 0; i < size; i++)
      {
        buff_ptr[i] = input_getc ();
      }
      f->eax = size;
    }
    else
    {
      struct list_elem *iterator;
      struct file_elem *cur_file;
      lock_acquire (&file_sys_lock);
      struct list *cur_file_list = &thread_current ()->file_list;

      /* Loop thru file list until fd is found */
      for (iterator = list_begin(cur_file_list);
           iterator != list_end (cur_file_list);
           iterator = list_next (iterator))
      {
        cur_file = list_entry (iterator, struct file_elem, elem);
        if (cur_file != NULL && cur_file->fd == fd)
        {
          f->eax = file_read (cur_file->file, (void*) buf, size);
          break;
        }
        else
        {
          f->eax = -1;
        }
      }
      lock_release (&file_sys_lock);
    }
  }
  else
  {
    error_exit (-1);
  }
}
/* End Driving */


/* Ryan Driving */
/* Writes size bytes from buffer to the open file fd. Returns the number of
   bytes actually written, which may be less than size if some bytes could not
   be written. */
static void 
write_handler (struct intr_frame *f)
{ 
  int *my_esp = (int*) f->esp;

  /* Check second to last arg's content and then last arg */
  if (valid_ptr (my_esp + 1) && valid_ptr (my_esp + 2)
      && valid_ptr (my_esp + 3) && valid_ptr ((int *)(*(my_esp + 2))))
  {
    /* Make args into clear var names */
    int fd = *(my_esp + 1);
    char **buf_ptr = (char**)(my_esp + 2);
    int size = *(my_esp + 3);
    char *buf = *buf_ptr;
   
    if (fd == 1)
    { /* Write to console */
      putbuf (buf, size);
      f->eax = size;
    }
    else
    {
      struct list_elem *iterator;
      struct file_elem *cur_file;
      lock_acquire (&file_sys_lock);
      struct list *cur_file_list = &thread_current ()->file_list;

      /* Loop thru file list until fd is found */
      for (iterator = list_begin (cur_file_list);
           iterator != list_end (cur_file_list);
           iterator = list_next (iterator))
        {
          cur_file = list_entry (iterator, struct file_elem, elem);
          if (cur_file != NULL && cur_file->fd == fd)
          {
            f->eax = file_write (cur_file->file, (void*) (*(my_esp + 2)),
                    *(my_esp + 3));
            break;
          }
          else
          {
            f->eax = -1;
          }
        }
        lock_release (&file_sys_lock);
    }
  }
  else
  {
    error_exit (-1);
  }
}
/* End Driving */


/* Sam Driving */
/* Changes the next byte to be read or written in open file fd to position,
   expressed in bytes from the beginning of the file. (Thus, a position of 0 is
   the file's start.) */
static void 
seek_handler (struct intr_frame *f)
{
  int *my_esp = (int*) f->esp;
  if (valid_ptr (my_esp + 1) && valid_ptr (my_esp + 2))
  {
    lock_acquire (&file_sys_lock);

    struct list_elem *iterator;
    struct file_elem *cur_file;
    struct list *cur_file_list = &thread_current ()->file_list;

    int fd = *(my_esp + 1);
    unsigned int position = *(my_esp + 2);

    /* Loop thru file list until fd is found */
    for (iterator = list_begin (cur_file_list);
         iterator != list_end (cur_file_list);
         iterator = list_next (iterator))
      {
        cur_file = list_entry (iterator, struct file_elem, elem);
        if (cur_file != NULL && cur_file->fd == fd)
        {
          file_seek (cur_file->file, position);
        }
      }
    lock_release (&file_sys_lock);
  }
  else
  {
    error_exit (-1);
  }
}
/* End Driving */


/* Brian Driving */
/* Returns the position of the next byte to be read or written in open file fd,
   expressed in bytes from the beginning of the file. */
static void 
tell_handler (struct intr_frame *f)
{
  int *my_esp = (int*) f->esp;
  if (valid_ptr (my_esp + 1))
  {
    lock_acquire (&file_sys_lock);

    struct list_elem *iterator;
    struct file_elem *cur_file;
    struct list *cur_file_list = &thread_current ()->file_list;

    int fd = *(my_esp + 1);

    /* Loop thru file list until fd is found */
    for (iterator = list_begin (cur_file_list);
         iterator != list_end (cur_file_list);
         iterator = list_next (iterator))
      {
        cur_file = list_entry (iterator, struct file_elem, elem);
        if (cur_file != NULL && cur_file->fd == fd)
        {
          f->eax = file_tell (cur_file->file); 
        }
      }
    lock_release (&file_sys_lock);
  }
  else
  {
    error_exit (-1);
  }
}
/* End Driving */


/* Miles Driving */
/* Closes file descriptor fd. Exiting or terminating a process implicitly
   closes all its open file descriptors, as if by calling this function for
   each one. */
static void 
close_handler (struct intr_frame *f) 
{
  int *my_esp = (int*) f->esp;
  if (valid_ptr (my_esp + 1) && ((int *)(*(my_esp + 1)) != NULL))
  {
    lock_acquire (&file_sys_lock);

    struct list_elem *iterator;
    struct file_elem *cur_file = NULL;
    struct list *cur_file_list = &thread_current ()->file_list;

    int fd = (int)(*(my_esp + 1));

    /* Verify is not main or idle thread */
    if (fd == 0 || fd == 1)
    {
      lock_release (&file_sys_lock);
      return;
    }

    /* Loop thru file list until fd is found */
    for (iterator = list_begin (cur_file_list);
         iterator != list_end (cur_file_list);
         iterator = list_next (iterator))
      {
        cur_file = list_entry (iterator, struct file_elem, elem);
        if (cur_file != NULL && cur_file->fd == fd)
        {
          /* Remove file from list */
          list_remove (iterator);
          /* Close file. */
          file_close (cur_file->file);
          break;
        }
      }
    lock_release (&file_sys_lock);
  }
  else
  {
    error_exit (-1);
  }
}
/* End Driving */


/* Ryan Driving */
/* Runs the executable whose name is given in cmd_line, passing any given
   arguments, and returns the new process's program id (pid). Must return 
   pid -1, which otherwise should not be a valid pid, if the program cannot 
   load or run for any reason. Thus, the parent process cannot return from the
   exec until it knows whether the child process successfully loaded
   its executable. */
static void 
exec_handler (struct intr_frame *f)
{
  int *my_esp = (int*) f->esp;
  if (valid_ptr (my_esp + 1) && valid_ptr ((int *)(*(my_esp + 1))))
  {
    /* ptr is valid */
    char* cmd_line = (char*) *(my_esp + 1);
    /* Create new process with new args */
    tid_t new_tid = process_execute (cmd_line);
    f->eax = new_tid;
  }
  else
  {
    error_exit (-1);
  }
}
/* End Driving */


/* Miles Driving */
/* Deletes the file called file. Returns true if successful, false otherwise.
   A file may be removed regardless of whether it is open or closed, and 
   removing an open file does not close it. */
static void 
remove_handler (struct intr_frame *f)
{
  int *my_esp = f->esp;
  if (valid_ptr (my_esp + 1) && valid_ptr ((int *)(*(my_esp + 1))))
  {
    lock_acquire (&file_sys_lock);
    /* delete file */
    f->eax = filesys_remove ((char*) *(my_esp + 1));
    lock_release (&file_sys_lock);
  }
  else
  {
    f->eax = -1;
    error_exit (-1);
  }
}
/* End Driving */


/* Sam Driving */
/* Opens the file called file. Returns a nonnegative integer handle called
   a "file descriptor" (fd) or -1 if the file could not be opened. */
static void 
open_handler (struct intr_frame *f)
{
  int *my_esp = f->esp;
  if (valid_ptr (my_esp + 1) && valid_ptr ((int *)(*(my_esp + 1))))
  {
    lock_acquire (&file_sys_lock);
    char *f_name = (char*) (*(my_esp + 1));
    struct file *cur_file = filesys_open (f_name);
    lock_release (&file_sys_lock);
    if (cur_file == NULL)
    { /* Bad file name */
      f->eax = -1;
    }
    else
    {
      struct file_elem *f_elem = malloc (sizeof (struct file_elem));
      f_elem->file = cur_file;
      struct thread *cur = thread_current ();
      /* Create unique fd_count every time file opened */
      cur->fd_count++;
      f_elem->fd = cur->fd_count;
      lock_acquire (&file_sys_lock);
      /* Add to file list */
      list_push_front (&cur->file_list, &f_elem->elem);
      lock_release (&file_sys_lock);
      f->eax = f_elem->fd;
    }
  }
  else
  {
    error_exit (-1);
  }
}
/* End Driving */

