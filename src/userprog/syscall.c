#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
// Brian Driving
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/synch.h"
#include "pagedir.h"
#include "devices/input.h"



static void syscall_handler (struct intr_frame *);

static bool ptr_is_valid (void *ptr);

/* Brian Driving */
static void syscall_halt_handler (void);
static void syscall_exit_handler (struct intr_frame *f);
static void syscall_exec_handler (struct intr_frame *f);
static void syscall_wait_handler (struct intr_frame *f);
static void syscall_create_handler (struct intr_frame *f);
static void syscall_remove_handler (struct intr_frame *f);
static void syscall_open_handler (struct intr_frame *f);
static void syscall_filesize_handler (struct intr_frame *f);
static void syscall_read_handler (struct intr_frame *f);
static void syscall_write_handler (struct intr_frame *f);
static void syscall_seek_handler (struct intr_frame *f);
static void syscall_tell_handler (struct intr_frame *f);
static void syscall_close_handler (struct intr_frame *f);
/* End Driving */

/* Ryan Driving */
// helper methods for exit
struct child* get_child(tid_t tid, struct thread *cur_thread);
static void exit_free_resources (struct thread *cur_thread);
/* End Driving */

/*locks file access*/
static struct lock file_sys_lock;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_sys_lock);
}

static void
syscall_handler (struct intr_frame *f) 
{

  printf ("system call!\n");


  void * my_esp = f->esp;
  
  if (!ptr_is_valid (my_esp))
  {
    // BAD!
    // TODO: exit();
  }

  int syscall_num = *((int*) my_esp);

  // WHO ELSE BUT YA BOI BRIAN BEHIND THAT MUTHAFUCKIN WHEEL, BITCH 😎
  switch (syscall_num)
  {
    case SYS_HALT :
      syscall_halt_handler ();
      break;
    case SYS_EXIT :
      syscall_exit_handler (f);
      break;
    case SYS_EXEC :
      syscall_exec_handler (f);
      break;
    case SYS_WAIT :
      syscall_wait_handler (f);
      break;
    case SYS_CREATE :
      syscall_create_handler (f);
      break;
    case SYS_REMOVE :
      syscall_remove_handler (f);
      break;
    case SYS_OPEN :
      syscall_open_handler (f);
      break;
    case SYS_FILESIZE :
      syscall_filesize_handler (f);
      break;
    case SYS_READ :
      syscall_read_handler (f);
      break;
    case SYS_WRITE :
      syscall_write_handler (f);
      break;
    case SYS_SEEK :
      syscall_seek_handler (f);
      break;
    case SYS_TELL :
      syscall_tell_handler (f);
      break;
    case SYS_CLOSE :
      syscall_close_handler (f);
      break;
  }

  thread_exit ();
}

// - Brian 🤠 
// Returns true iff ptr is properly allocated in user memory
static bool
ptr_is_valid (void *ptr)
{
  return (ptr != NULL && is_user_vaddr (ptr) 
  && pagedir_get_page (thread_current ()->pagedir, ptr) != NULL);
  // TODO: what if partially in stack?
}


// Brian driving
static void
syscall_halt_handler ()
{
  shutdown_power_off ();
}

/* Ryan Driving */
// parses the child list of a thread to find a child
struct child*
get_child(tid_t tid, struct thread *cur_thread)
{
  struct list_elem * e;
  for (e=list_begin(&cur_thread->child_list);
       e!=list_end(&cur_thread->child_list); e=list_next(e))
  {
    struct child *result = list_entry(e, struct child, child_elem);
    if(result->child_tid == tid)
      return result;
  }
  return NULL;
}
/* End Driving */

/* Ryan Driving */
static void
exit_free_resources (struct thread *cur_thread)
{

}

/* Ryan Driving */
/* handles syscalls to exit, closing files,
   freeing memory, and modifying exit statuses */
static void
syscall_exit_handler (struct intr_frame *f)
{
  // grab the esp off intr_frame
  int *my_esp = f->esp;
  if (ptr_is_valid ((void *) (my_esp + 1)))
  {
    // save the status cod
    struct thread *cur_thread = thread_current ();
    // save reference to parent
    struct thread *parent = cur_thread->parent;
    cur_thread->exit_code = *(my_esp + 1);

    /* must check if the current thread to be exited is a child
       of another thread, if so, we must update the child struct*/
    if (!list_empty(&parent->child_list))
    {
      // get the current thread's relevant child struct
      struct child *cur = get_child(thread_current()->tid,parent);
      if (cur != NULL)
      {
        cur->exited = 1;
        cur->child_exit_code = *(my_esp + 1);
        /* wake up the current thread's parent if it is waiting
           on the exit code */
        if (cur_thread->parent->waited_on_child == cur_thread->tid)
          sema_up (&cur_thread->parent->child_sema);
      }
      free (cur);
    }

    // free resources

  }
  thread_exit ();
}

/*Miles Driving*
handler that exits the process and clears up memory
close all open files and free space*/
// static void syscall_exit_handler (struct intr_frame *f)
// {
//   int *my_esp = f->esp;
//   my_esp++;
//   //check that the arg passed in is a valid pointer
//   if (ptr_is_valid ((void*) my_esp))
//   {
//     /*close all open files*/
//     int status = *my_esp;
//     struct thread *cur = thread_current ();
//     struct list_elem *cur_elem;

//     while (!list_empty (&cur->fd_list))
//     {
//       cur_elem = list_pop_back (&cur->fd_list);
//       struct file_elem *cur_file = list_entry (cur_elem, struct file_elem, elem);
//       file_close (&cur_file->file);
//       free (cur_file);

//     }
  
//     /*free up threads children*/
//     while (!list_empty (&cur->child_list))
//     {
//       cur_elem = list_pop_back (&cur->child_list);
//       struct child_elem *cur_child = list_entry (cur_elem, struct child_elem, elem);
//       free (cur_child);
//       //todo free big list
//     }

//   }
//   else 
//   {
//     //todo free resources and locks
//   }
  
// }

 /*handles a create file sys call*/
static void syscall_create_handler (struct intr_frame *f)
{
  int *my_esp = (int*) f->esp;
  if (ptr_is_valid ((void*) (my_esp + 4)) && ptr_is_valid ((void*) (my_esp + 5)))
  {
     
    //both args are valid
    lock_acquire (&file_sys_lock);
    
    my_esp++;
    char *f_name = (char*) my_esp;
    my_esp++;
    off_t initial_size = (unsigned) *my_esp;
    f->eax = filesys_create (f_name, initial_size);

    lock_release (&file_sys_lock);
 
  }

}


static void syscall_filesize_handler (struct intr_frame *f)
{
  int *my_esp = (int*) f->esp;
  if (ptr_is_valid ((void*) (my_esp + 1)))
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
  else
  {
    //TODO: exit()
  }
}

static void syscall_read_handler (struct intr_frame *f)
{
  int *my_esp = (int*) f->esp;
  // Check second to last arg's content and then last arg
  if (ptr_is_valid ((void*) (my_esp + 6)) && ptr_is_valid ((void*) (my_esp + 7)))
  {
    if (*(my_esp + 5) == 0)
    { 
      uint8_t *buff_ptr = (uint8_t*) *(my_esp + 6); // ... i think + 6
      int i;
      for (i = 0; i < *(my_esp + 7); i++)
      {
        buff_ptr[i] = input_getc ();
      }
      f->eax = *(my_esp + 7);
    }
    else
    {
      struct list_elem *iterator;
      struct file_elem *cur_file;
      struct list *cur_file_list = &thread_current ()->file_list;

      int fd = *(my_esp + 5);

      for (iterator = list_begin(cur_file_list);
           iterator != list_end (cur_file_list);
           iterator = list_next (iterator))
        {
          cur_file = list_entry (iterator, struct file_elem, elem);
          if (cur_file != NULL && cur_file->fd == fd)
          {
            lock_acquire (&file_sys_lock);

            f->eax = file_read (cur_file->file, (void*) (my_esp + 6), *(my_esp + 7));

            lock_release (&file_sys_lock);
          }
          else
          {
            f->eax = -1;
          }
        }
    }
  }
  else
  {
    f->eax = 0;
    // TODO: exit()
  }
}


static void syscall_write_handler (struct intr_frame *f)
{
  printf("In write call\n");
  int *my_esp = (int*) f->esp;
  //hex_dump(my_esp, my_esp, (int)(PHYS_BASE - (int)my_esp), true);
  int sys_call_num = *(my_esp);
  int first_arg = *(my_esp + 1);
  char **buf = (char**)(my_esp + 2);
  int size = *(my_esp + 3);


  printf ("first arg: %x\n", first_arg);
  printf ("size: %d\n", size);
  printf ("buf: %s\n", *(buf) + 6 );
 
  
  // Check second to last arg's content and then last arg
  if (ptr_is_valid ((void*) (my_esp + 6)) && ptr_is_valid ((void*) (my_esp + 7)))
  {
    if (*(my_esp + 5) == 1)
    { // Write to console
      putbuf (*(my_esp + 6), *(my_esp + 7));
      f->eax = *(my_esp + 7);
    }
    else
    {
      struct list_elem *iterator;
      struct file_elem *cur_file;
      struct list *cur_file_list = &thread_current ()->file_list;

      int fd = *(my_esp + 5);

      for (iterator = list_begin(cur_file_list);
           iterator != list_end (cur_file_list);
           iterator = list_next (iterator))
        {
          cur_file = list_entry (iterator, struct file_elem, elem);
          if (cur_file != NULL && cur_file->fd == fd)
          {
            lock_acquire (&file_sys_lock);

            f->eax = file_write (cur_file->file, (void*) (my_esp + 6), *(my_esp + 7));

            lock_release (&file_sys_lock);
          }
          else
          {
            f->eax = -1;
          }
        }
    }
  }
  else
  {
    f->eax = 0;
    // TODO: exit()
  }
}


static void syscall_seek_handler (struct intr_frame *f)
{
  int *my_esp = (int*) f->esp;
  if (ptr_is_valid ((void*) (my_esp + 5)))
  {
    lock_acquire (&file_sys_lock);

    struct list_elem *iterator;
    struct file_elem *cur_file;
    struct list *cur_file_list = &thread_current ()->file_list;

    int fd = *(my_esp + 4);

    for (iterator = list_begin(cur_file_list);
         iterator != list_end (cur_file_list);
         iterator = list_next (iterator))
      {
        cur_file = list_entry (iterator, struct file_elem, elem);
        if (cur_file != NULL && cur_file->fd == fd)
        {
          //TODO: verify this bit stuff is right
          file_seek (cur_file->file, *(my_esp + 5));
        }
      }
    lock_release (&file_sys_lock);
  }
  else
  {
    // TODO: exit()
  }
}


static void syscall_tell_handler (struct intr_frame *f)
{
  int *my_esp = (int*) f->esp;
  if (ptr_is_valid ((void*) (my_esp + 1)))
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
  else
  {
    //TODO: exit()
  }
}



static void syscall_close_handler (struct intr_frame *f) 
{
  int *my_esp = (int*) f->esp;
  if (ptr_is_valid ((void*) (my_esp + 1)))
  {
    lock_acquire (&file_sys_lock);

    struct list_elem *iterator;
    struct file_elem *cur_file;
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
          file_close (cur_file->file);  // Close file
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

static void syscall_exit_handler (struct intr_frame *f)
{
  f->eax - 0;
}
static void syscall_exec_handler (struct intr_frame *f)
{
  f->eax = 0;
}
static void syscall_wait_handler (struct intr_frame *f)
{
  f->eax = 0;
}

static void syscall_remove_handler (struct intr_frame *f)
{
  f->eax = 0;
}

static void syscall_open_handler (struct intr_frame *f)
{
  f->eax = 0;
}
