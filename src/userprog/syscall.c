#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
// Brian Driving
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "process.h"
#include "filesys/file.c"
#include "filesys/filesys.c"
//#include "threads/synch.c"
#include "pagedir.c"



static void syscall_handler (struct intr_frame *);

static bool ptr_is_valid (void *ptr);

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

/*locks file access*/
static struct lock file_lock;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_lock);
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
    lock_acquire (&file_lock);
    
    my_esp++;
    char *f_name = (char*) my_esp;
    my_esp++;
    off_t initial_size = (unsigned) *my_esp;
    f->eax = filesys_create (f_name, initial_size);

    lock_release (&file_lock);
 
  }

}
