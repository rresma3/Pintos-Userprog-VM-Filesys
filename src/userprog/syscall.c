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
#include "threads/malloc.h"



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
static void free_resources(struct thread *t);
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
if (ptr != NULL && is_user_vaddr (ptr) 
  && pagedir_get_page (thread_current ()->pagedir, ptr) != NULL)
  {
    return true;
  }
  else
  {
    return false;
  }
  // TODO: what if partially in stack?
}


// Brian driving
static void
syscall_halt_handler ()
{
  printf("in halt handler\n");
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
    free(result);
  }
  return NULL;
}
/* End Driving */

static void 
free_resources(struct thread *t)
{
  // if the current thread's parent isn't dead, we must call sema
  if (&t->parent != NULL)
  {
    // we want to try to dereference the parent
    sema_down (&t->parent->zombie_sema);
  }

  struct list_elem *iterator;
  struct file_elem *cur_file;
  struct child *cur_child;
  // synchronize and free the memory we don't need
  lock_acquire (&file_sys_lock);
  while (!list_empty (&t->file_list))
  {
    iterator = list_pop_back (&t->file_list);
    cur_file = list_entry (iterator, struct file_elem, elem);
    file_close (&cur_file->file);
    free (cur_file);
  }
  lock_release (&file_sys_lock);

  lock_acquire (&t->child_list_lock);
  while (!list_empty (&t->child_list))
  {
    iterator = list_pop_back (&t->child_list);
    cur_child = list_entry (iterator, struct child, child_elem);
    free (cur_child);
  }
  lock_release (&t->child_list_lock);

  // let zombie children know they can exit
  sema_up (&t->zombie_sema);
  t->parent = NULL;
}

/* Ryan Driving */
/* handles syscalls to exit, closing files,
   freeing memory, and modifying exit statuses */
static void
syscall_exit_handler (struct intr_frame *f)
{
  printf("in exit handler\n");
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
    if (parent != NULL && !list_empty(&parent->child_list))
    {
      lock_acquire (&parent->child_list_lock);
      // get the current thread's relevant child struct
      struct child *cur = get_child(thread_current()->tid,parent);
      if (cur != NULL)
      {
        // TODO: may not need?
        cur->exited = 1;
        cur->child_exit_code = *(my_esp + 1);
        /* wake up the current thread's parent if it is waiting
           on the exit code */
        if (cur_thread->parent->waited_on_child == cur_thread->tid)
          sema_up (&cur_thread->parent->reap_sema);
      }
      free (cur);
      lock_release (&parent->child_list_lock);
    }
  }
  free_resources (thread_current ());
  thread_exit ();
}

 /*handles a create file sys call*/
static void syscall_create_handler (struct intr_frame *f)
{
  printf("in create handler\n");
  int *my_esp = (int*) f->esp;
  if (ptr_is_valid ((void*) (my_esp + 1)) && ptr_is_valid ((void*) (my_esp + 2))
      && ptr_is_valid ((void*) *(my_esp + 1)))
  {
     
    //both args are valid.
    lock_acquire (&file_sys_lock);
    char *f_name = (char*) *(my_esp + 1);
    unsigned int initial_size = (unsigned) *(my_esp + 2);
    f->eax = filesys_create (f_name, initial_size);

    lock_release (&file_sys_lock);
 
  }

}


static void syscall_filesize_handler (struct intr_frame *f)
{
  printf("in filesize handler\n");
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
}

static void syscall_read_handler (struct intr_frame *f)
{
  printf("in read handler\n");
  int *my_esp = (int*) f->esp;
  // Check second to last arg's content and then last arg
   // Check second to last arg's content and then last arg
  if (ptr_is_valid ((void*) (my_esp + 1)) && ptr_is_valid ((void*) (my_esp + 2))
      && ptr_is_valid ((void*) (my_esp + 3)) && ptr_is_valid ((void*) (*(my_esp + 2))))
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
      struct list *cur_file_list = &thread_current ()->file_list;


      for (iterator = list_begin(cur_file_list);
           iterator != list_end (cur_file_list);
           iterator = list_next (iterator))
        {
          cur_file = list_entry (iterator, struct file_elem, elem);
          if (cur_file != NULL && cur_file->fd == fd)
          {
            lock_acquire (&file_sys_lock);

            f->eax = file_read (cur_file->file, (void*) buf, size);

            lock_release (&file_sys_lock);
          }
          else
          {
            f->eax = -1;
          }
        }
    }
  }
}


static void syscall_write_handler (struct intr_frame *f)
{ // TODO: get second arg from input (x)
  printf("In write handler\n");
  int *my_esp = (int*) f->esp;

  // Check second to last arg's content and then last arg
  if (ptr_is_valid ((void*) (my_esp + 1)) && ptr_is_valid ((void*) (my_esp + 2))
      && ptr_is_valid ((void*) (my_esp + 3)) && ptr_is_valid ((void*) (*(my_esp + 2))))
  {
    int fd = *(my_esp + 1);
    char **buf_ptr = (char**)(my_esp + 2);
    int size = *(my_esp + 3);
    char *buf = *buf_ptr;
    

    printf ("first arg: %x\n", fd);
    printf ("size: %d\n", size);
    //printf ("buf: %s\n", (buf));

    if (fd == 1)
    { // Write to console
      printf ("write to console\n");
      //ASSERT(1 == 23);

      putbuf (buf, size);
     
      f->eax = size;
    }
    else
    {
      struct list_elem *iterator;
      struct file_elem *cur_file;
      struct list *cur_file_list = &thread_current ()->file_list;

      

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
}


static void syscall_seek_handler (struct intr_frame *f)
{
  printf("in seek handler\n");
  int *my_esp = (int*) f->esp;
  if (ptr_is_valid ((void*) (my_esp + 1)) && ptr_is_valid ((void*) (my_esp + 2)))
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


static void syscall_tell_handler (struct intr_frame *f)
{
  printf("in tell handler\n");
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
}



static void syscall_close_handler (struct intr_frame *f) 
{
  printf("in close handler\n");
  int *my_esp = (int*) f->esp;
  if (ptr_is_valid ((void*) (my_esp + 1)))
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



static void syscall_exec_handler (struct intr_frame *f)
{
  // printf ("in exec handler\n");
  // int *my_esp = f->esp;
  // if (ptr_is_valid ((void*) (my_esp + 1)) && ptr_is_valid ((void*) *(my_esp + 1)))
  // {
  //   char* cmd_line = (char*) (*(my_esp + 1));
  //   printf ("cmd_line: %s\n", cmd_line);
  //   struct thread *cur = thread_current ();
  //   int child_pid = 0;
  //   child_pid = process_execute (cmd_line);
  //   sema_down (&cur->load_sema);
    
     
  // }
  f->eax = 0;
}
static void syscall_wait_handler (struct intr_frame *f)
{
  printf("in wait handler\n");
  f->eax = 0;
}

static void syscall_remove_handler (struct intr_frame *f)
{
  printf("in remove handler\n");
  int *my_esp = f->esp;
  if (ptr_is_valid ((void*) (my_esp + 1)) && ptr_is_valid ((void*) (*(my_esp + 1))))
  {
    lock_acquire (&file_sys_lock);
    f->eax = filesys_remove ((char*) *(my_esp + 1));
    lock_release (&file_sys_lock);
  }
  
}

static void syscall_open_handler (struct intr_frame *f)
{
  printf("in open handler\n");
  int *my_esp = f->esp;
  if (ptr_is_valid ((void*) (my_esp + 1)) && ptr_is_valid ((void*) *(my_esp + 1)))
  {
    printf("pointers valid\n");
    lock_acquire (&file_sys_lock);
    char *f_name = (char*) (*(my_esp + 1));
    printf("openeing file: %s\n", f_name);
    struct file *cur_file = filesys_open (f_name);
    lock_release (&file_sys_lock);
    if (cur_file == NULL)
    {
      printf("file null\n");
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
