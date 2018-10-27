#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
// Brian Driving
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


static void syscall_handler (struct intr_frame *);

/* Miles Driving */
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
/* End Driving */

/* Ryan Driving */
static void error_exit (int exit_status);
/* End Driving */


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_sys_lock);
}

static void
syscall_handler (struct intr_frame *f) 
{

  //printf ("system call!\n");


  int* my_esp = f->esp;
  
  if (!valid_ptr (my_esp))
  {
    // BAD!
    error_exit(-1);
  }

  int syscall_num = *((int*) my_esp);

  // B-Dawg drivin'.
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
      break;
    case SYS_TELL :
      tell_handler (f);
      break;
    case SYS_CLOSE :
      close_handler (f);
      break;
  }

  //thread_exit ();
}

// - Brian 🤠 
// Returns true iff ptr is properly allocated in user memory
bool
valid_ptr (int *ptr)
{
  struct thread *cur = thread_current ();

  if(ptr == NULL || is_kernel_vaddr(ptr) || 
    pagedir_get_page(cur->pagedir, ptr) == NULL)
    {
      return false;
    }
    return true;
}


// Brian driving
static void
halt_handler ()
{
  shutdown_power_off ();
}

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
/* handles syscalls to exit, closing files,
   freeing memory, and modifying exit statuses */
static void
exit_handler (struct intr_frame *f)
{
  //printf("in exit handler\n");
  // grab the esp off intr_frame
  int *my_esp = f->esp;

  if (valid_ptr (my_esp + 1))
  {
    thread_current ()->exit_code = *(my_esp + 1);
    thread_exit ();
  }
  else
  {
    //printf ("invalid pointer");
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
  //printf("in wait handler\n");
  int *my_esp = f->esp;
  // check valid pointer
  if (valid_ptr (my_esp + 1))
  {
    f->eax = process_wait (*(my_esp + 1));
  }
  else
  {
    //printf ("invalid pointer");
    f->eax = -1;
    error_exit (-1);
  }
}
/* End Driving */


/* handles a create file sys call */
static void 
create_handler (struct intr_frame *f)
{
  //printf("in create handler\n");
  int *my_esp = (int*) f->esp;
  if (valid_ptr (my_esp + 1) && valid_ptr (my_esp + 2)
      && valid_ptr ((int*) *(my_esp + 1)))
  {
     
    //both args are valid.
    lock_acquire (&file_sys_lock);
    char *f_name = (char*) *(my_esp + 1);
    unsigned int initial_size = (unsigned) *(my_esp + 2);
    f->eax = filesys_create (f_name, initial_size);
    lock_release (&file_sys_lock);
 
  }
}


static void 
filesize_handler (struct intr_frame *f)
{
  //printf("in filesize handler\n");
  int *my_esp = (int*) f->esp;
  if (valid_ptr (my_esp + 1))
  {
    lock_acquire (&file_sys_lock);

    struct list_elem *iterator;
    struct file_elem *cur_file;
    struct list *cur_file_list = &thread_current ()->file_list;

    int fd = *(my_esp + 1);

    for (iterator = list_begin(cur_file_list);
         iterator != list_end (cur_file_list);
         iterator = list_next (iterator))
      {
        cur_file = list_entry (iterator, struct file_elem, elem);
        if (cur_file != NULL && cur_file->fd == fd)
        {
          f->eax = file_length (cur_file->file); // Set return value to size
        }
      }
    lock_release (&file_sys_lock);
  }
}

static void 
read_handler (struct intr_frame *f)
{
  //printf("in read handler\n");
  int *my_esp = (int*) f->esp;
  // Check second to last arg's content and then last arg
   // Check second to last arg's content and then last arg
  if (valid_ptr (my_esp + 1) && valid_ptr (my_esp + 2)
      && valid_ptr (my_esp + 3) && valid_ptr ((int*) *(my_esp + 2)))
  {
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


      for (iterator = list_begin(cur_file_list);
           iterator != list_end (cur_file_list);
           iterator = list_next (iterator))
      {
        cur_file = list_entry (iterator, struct file_elem, elem);
        if (cur_file != NULL && cur_file->fd == fd)
        {
          
          f->eax = file_read (cur_file->file, (void*) buf, size);
        }
        else
        {
          f->eax = -1;
        }
      }
      lock_release (&file_sys_lock);
    }
  }
}


static void 
write_handler (struct intr_frame *f)
{ // TODO: get second arg from input (x)
  //printf("In write handler\n");
  int *my_esp = (int*) f->esp;

  // Check second to last arg's content and then last arg
  if (valid_ptr (my_esp + 1) && valid_ptr (my_esp + 2)
      && valid_ptr (my_esp + 3) && valid_ptr ((int*) *(my_esp + 2)))
  {
    //hex_dump(my_esp, my_esp, (int)(PHYS_BASE - (int)my_esp), true);
    int fd = *(my_esp + 1);
    char **buf_ptr = (char**)(my_esp + 2);
    int size = *(my_esp + 3);
    //printf ("\nsize: %d\n", size);
    char *buf = *buf_ptr;
    
    //printf ("\nbuf: %s\n", (buf));
    // int i;
    // for (i = size; i >= 0; i--) 
    // {
    //   printf ("\nbuffer elem %d is %s\n", i, &buf[i]);
    // }

    if (fd == 1)
    { // Write to console
      //printf ("write to console\n");
      //ASSERT(1 == 23);

      putbuf (buf, size);
     
      f->eax = size;
    }
    else
    {
      struct list_elem *iterator;
      struct file_elem *cur_file;
      lock_acquire (&file_sys_lock);
      struct list *cur_file_list = &thread_current ()->file_list;

      

      for (iterator = list_begin(cur_file_list);
           iterator != list_end (cur_file_list);
           iterator = list_next (iterator))
        {
          cur_file = list_entry (iterator, struct file_elem, elem);
          if (cur_file != NULL && cur_file->fd == fd)
          {
            
            f->eax = file_write (cur_file->file, (void*) (my_esp + 6), *(my_esp + 7));
          }
          else
          {
            f->eax = -1;
          }
        }
        lock_release (&file_sys_lock);
    }
  }
}


static void 
seek_handler (struct intr_frame *f)
{
  //printf("in seek handler\n");
  int *my_esp = (int*) f->esp;
  if (valid_ptr (my_esp + 1) && valid_ptr (my_esp + 2))
  {
    lock_acquire (&file_sys_lock);

    struct list_elem *iterator;
    struct file_elem *cur_file;
    struct list *cur_file_list = &thread_current ()->file_list;

    int fd = *(my_esp + 1);
    unsigned int position = *(my_esp + 2);

    for (iterator = list_begin(cur_file_list);
         iterator != list_end (cur_file_list);
         iterator = list_next (iterator))
      {
        cur_file = list_entry (iterator, struct file_elem, elem);
        if (cur_file != NULL && cur_file->fd == fd)
        {
          //TODO: verify this bit stuff is right
          file_seek (cur_file->file, position);
        }
      }
    lock_release (&file_sys_lock);
  }
}


static void 
tell_handler (struct intr_frame *f)
{
  //printf("in tell handler\n");
  int *my_esp = (int*) f->esp;
  if (valid_ptr ((void*) (my_esp + 1)))
  {
    lock_acquire (&file_sys_lock);

    struct list_elem *iterator;
    struct file_elem *cur_file;
    struct list *cur_file_list = &thread_current ()->file_list;

    int fd = *(my_esp + 1);

    for (iterator = list_begin(cur_file_list);
         iterator != list_end (cur_file_list);
         iterator = list_next (iterator))
      {
        cur_file = list_entry (iterator, struct file_elem, elem);
        if (cur_file != NULL && cur_file->fd == fd)
        {
          f->eax = file_tell (cur_file->file); // Set return value to pos
        }
      }
    lock_release (&file_sys_lock);
  }
}



static void 
close_handler (struct intr_frame *f) 
{
  //printf("in close handler\n");
  int *my_esp = (int*) f->esp;
  if (valid_ptr ((void*) (my_esp + 1)))
  {
    lock_acquire (&file_sys_lock);

    struct list_elem *iterator;
    struct file_elem *cur_file = NULL;
    struct list *cur_file_list = &thread_current ()->file_list;

    int fd = *(my_esp + 1);

    // TODO: optimize loop
    for (iterator = list_begin(cur_file_list);
         iterator != list_end (cur_file_list);
         iterator = list_next (iterator))
      {
        cur_file = list_entry (iterator, struct file_elem, elem);
        if (cur_file != NULL && cur_file->fd == fd)
        {
          list_remove (iterator); // Remove file from list
          file_close (cur_file->file);  // Close file.
          break;
        }
      }
      free (cur_file); // Deallocate deleted file

    lock_release (&file_sys_lock);
  }
  else
  {
    // TODO: exit()
  }
}



static void 
exec_handler (struct intr_frame *f)
{
  //printf("in exec handler\n");
  int *my_esp = (int*) f->esp;
  if (valid_ptr ((void*) (my_esp + 1)) && valid_ptr ((void*) (*(my_esp + 1))))
  {
    //ptr is valid
    char* cmd_line = (char*) *(my_esp + 1);
    tid_t new_tid = process_execute (cmd_line);
    f->eax = new_tid;
  }

  
}

static void 
remove_handler (struct intr_frame *f)
{
  //printf("in remove handler\n");
  int *my_esp = f->esp;
  if (valid_ptr ((void*) (my_esp + 1)) && valid_ptr ((void*) (*(my_esp + 1))))
  {
    lock_acquire (&file_sys_lock);
    f->eax = filesys_remove ((char*) *(my_esp + 1));
    lock_release (&file_sys_lock);
  }
  
}

static void 
open_handler (struct intr_frame *f)
{
  //printf("in open handler\n");
  int *my_esp = f->esp;
  if (valid_ptr ((void*) (my_esp + 1)) && valid_ptr ((void*) *(my_esp + 1)))
  {
    //printf("pointers valid\n");
    lock_acquire (&file_sys_lock);
    char *f_name = (char*) (*(my_esp + 1));
    //printf("openeing file: %s\n", f_name);
    struct file *cur_file = filesys_open (f_name);
    lock_release (&file_sys_lock);
    if (cur_file == NULL)
    {
      //printf("file null\n");
      f->eax = -1;
    }
    else
    {
      struct file_elem *f_elem = malloc (sizeof (f_elem));
      f_elem->file = cur_file;
      struct thread *cur = thread_current ();
      f_elem->fd = cur->fd_count;
      cur->fd_count++;
      list_push_back (&cur->file_list, &f_elem->elem);
      f->eax = f_elem->fd;
    }
    
  }
}
