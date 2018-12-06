#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "userprog/syscall.h"

static thread_func start_process NO_RETURN;
static bool load (char *argv[], int argc, void (**eip) (void), void **esp);


/* Sam Driving */
/* struct to keep track of the arg vector */
struct args
{
  char *argument;
  void *addr;
  struct list_elem elem;
};
/* Sam end Driving */

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);
  struct thread *cur = thread_current ();
 
  if (tid == TID_ERROR)
  {
    palloc_free_page (fn_copy);
  }

  /* Miles driving */
  /* wait for the child to finish loading */
  sema_down (&cur->child_sema);
  if (!cur->load_success)
  {
    /* child loaded improperly */
    return -1;
  }
  return tid;
  /* Miles end driving */
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

/*Miles Driving
  first make a copy of the file name*/
  char* cmd_line  = (char *) malloc (sizeof (char) * (strlen (file_name) + 1));
  strlcpy (cmd_line, file_name, strlen (file_name) + 1);
  /*tokenize arguments and pass that into the new thread.
  keep arguments in a list */
  
  struct list args_list;
  list_init (&args_list);

  char *token, *save_ptr;
  int count = 0;
  /* go through the command line and start adding arguments into our list */
  for (token = strtok_r (cmd_line, " ", &save_ptr); token != NULL;
      token = strtok_r (NULL, " ", &save_ptr))
  {
    struct args *cur = (struct args *) malloc (sizeof (struct args));
    cur->argument = token;
    count += strlen (token) + 1;
    /* prepare args by adding to list */
    list_push_back (&args_list, &cur->elem);
  }
  /* args are in a list now, convert to the new string */
  int i;
  int argc = list_size (&args_list);
  char *arg_vector[argc];
  struct list_elem *iterator;
  for (i = 0; i < argc; i++)
  {
    iterator = list_pop_front (&args_list);
    struct args *cur = list_entry (iterator, struct args, elem);
    char *argument = cur->argument;
    arg_vector[i] = argument;
  }
  /* now arg_vector contains the argument vector */
  /* load the exe and get the status */
  success = load (arg_vector, argc, &if_.eip, &if_.esp);
  
  /* If load failed, quit. */
  palloc_free_page (file_name);
  
  /* get the current thread which is the child */
  struct thread *child = thread_current ();
  if (!success)
  {
    child->exit_code = -1;
    /* load failed, notify the parent and sema up */
    child->parent->load_success = false;
    sema_up (&child->parent->child_sema);
    thread_exit ();
  }
  else
  {
    /* load succeeded, notify parent and sema up */
    child->parent->load_success = true;
    sema_up (&child->parent->child_sema);
  }

  /*end Miles Driving*/

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. a*/
int
process_wait (tid_t child_tid) 
{
  /* Ryan Driving */
  int ret_exit_code = -1;
  /* save reference to current thread */
  struct thread *cur_thread = thread_current ();
  /* attempt to find the child in child_list via pid search */
  struct child *cur_child = get_child (child_tid, cur_thread);
  /* check if we found a matching child */
  if (cur_child != NULL)
  {
    /* accessing element in our child list, MUST synchronize */
    lock_acquire (&cur_thread->child_list_lock);
    /* check if the current child is being waited on already, if so, return */
    if (cur_child->waited_on == 0)
    {
      /* our child is not being waited on, so we proceed */
      cur_thread->waited_on_child = cur_child->child_tid;
      cur_child->waited_on = 1;

      sema_up (&cur_thread->exit_sema);
      /* release our child's lock before we attempt to reap */
      lock_release (&cur_thread->child_list_lock);
      /* try to reap, sema_up() will be called in exit handler */
      sema_down (&cur_thread->reap_sema);

      /* grab the child's exit code. */
      lock_acquire (&cur_thread->child_list_lock);
      list_remove (&cur_child->child_elem);
      ret_exit_code = cur_child->child_exit_code;
      lock_release (&cur_thread->child_list_lock);
    } 
    else
    {
      /* child is being waited on already */
      lock_release (&cur_thread->child_list_lock);
    }
  }

  /* garbage collect */
  cur_thread = NULL;
  cur_child = NULL;
  return ret_exit_code;
  /* End Ryan driving */
}

/* Ryan Driving */
/* parses the child list of a thread to find a child */
struct child*
get_child (tid_t tid, struct thread *cur_thread)
{
  lock_acquire (&cur_thread->child_list_lock);
  struct list_elem * e = NULL;
  for (e = list_begin (&cur_thread->child_list);
       e != list_end (&cur_thread->child_list); e = list_next (e))
  {
    /* save reference to our current child */
    struct child *result = list_entry(e, struct child, child_elem);
    ASSERT (result != NULL);

    /* check if the tid's match, if so we have found our child */
    if (result->child_tid == tid)
    {
      ASSERT (&cur_thread->child_list_lock != NULL);
      lock_release (&cur_thread->child_list_lock);
      return result;
    }
  }
  lock_release (&cur_thread->child_list_lock);
  return NULL;
}

struct file_elem *
get_file (struct list* files, int fd)
{
  struct list_elem *e;
  for (e = list_begin (files); e != list_end (files);
       e = list_next (e))
    {
      struct file_elem *f = list_entry (e, struct file_elem, elem);
        if(f->fd == fd)
          return f;
    }
  return NULL;
}


void 
free_resources (struct thread *t)
{
  /* if the current thread's parent isn't dead, we must call sema */
  if (t->parent != NULL)
  {
    /* we want to try to dereference the parent */
    sema_down (&t->parent->zombie_sema);
  }
  
  struct list_elem *iterator = NULL;
  struct file_elem *cur_file = NULL;
  struct child *cur_child = NULL;

  /* synchronize and free the memory we don't need */
  lock_acquire (&file_sys_lock);
  while (!list_empty (&t->file_list))
  {
    iterator = list_pop_back (&t->file_list);
    cur_file = list_entry (iterator, struct file_elem, elem);
    file_close (cur_file->file);
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

  /* let zombie children know they can exit */
  sema_up (&t->zombie_sema);
  t->parent = NULL;
  /* End Ryan driving */
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur_thread = thread_current ();
  uint32_t *pd;
  bool be_reaped = false;

  /* Brian driving */
  /* save reference to parent */
  struct thread *parent = cur_thread->parent;

  /* must check if the current thread to be exited is a child
     of another thread, if so, we must update the child struct*/
  /* lock_acquire (&parent->child_list_lock); */
  if (parent != NULL && !list_empty (&parent->child_list))
  {
    /* get the current thread's relevant child struct */
    struct child *cur = get_child (cur_thread->tid, parent);
    if (cur != NULL)
    {
      cur->child_exit_code = cur_thread->exit_code;
      /* wake up the current thread's parent if it is waiting
         on the exit code */
      sema_down (&parent->exit_sema);
      if (cur_thread->parent->waited_on_child == cur_thread->tid)
      {
        cur->waited_on = 0;
        be_reaped = true;
      }    
    }
  }

  if (cur_thread->cwd)
  {
    dir_close (cur_thread->cwd);
  }
  /* garbage collection */
  /* tokenize the first part of the name */
  char *save_ptr;
  char *name = strtok_r (cur_thread->name, " ", &save_ptr);
  printf ("%s: exit(%d)\n", name, cur_thread->exit_code);
  if (be_reaped)
    sema_up (&cur_thread->parent->reap_sema);

  /* close the executable file */
  if (cur_thread->executable != NULL)
    file_close (cur_thread->executable);
  free_resources (cur_thread);

  /* End Brian driving */

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur_thread->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur_thread->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
  // free (cur_thread);
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (char* argv[], int argc, void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (char *argv[], int argc, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Brian driving */
  /* Open executable file. */
  lock_acquire (&file_sys_lock);
  /* Brian end driving */

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();

  if (t->pagedir == NULL)
  {
    goto done;
  } 
  process_activate ();



  /* open the file from the file sys */
  file = filesys_open (argv[0]);
  if (file == NULL) 
    {
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      goto done; 
    }


  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
      {
          goto done;
      }
        
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
      {
           goto done;
      }
       
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                                 {
                                    goto done;
                                 }
                                
                
            }
          else
          {
              goto done;
          }
            
          break;
        }
    }

  thread_current ()->cwd = dir_open_root();


  /* Set up stack. */
  if (!setup_stack (argv, argc, esp))
  {
    goto done;
  }
    
  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;
  success = true;
  
  /* Ryan Driving */
  /* Deny all attempted writes to executables */
  file_deny_write (file);
  t->executable = file;

 done:
  /* We arrive here whether the load is successful or not. */
  lock_release (&file_sys_lock);
  return success;
  /* Ryan end driving */
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (char *argv[], int argc, void **esp) 
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
      {
        /* Sam driving */
        *esp = PHYS_BASE;
        char *esp_cpy = PHYS_BASE;
        /* keep arguments in a list */
        int64_t count = 0;
        /* go through the command line and start adding 
          arguments into our list */
        int arg_addrs[argc];
        int i;
        for (i = 0; i < argc; i++)
        {
            /* count number of bytes needed */
            count += strlen (argv[i]) + 1;
            /* check for page size */
             if (count > PGSIZE)
               return false;
      
            /* add the arg addresses to an array */
            esp_cpy -= strlen (argv[i]) + 1;
            arg_addrs[i] = (int) esp_cpy;
            memcpy (esp_cpy, (void *) argv[i], strlen (argv[i]) + 1);
        }

        int zero = 0;
        /* align on 4 byte word */
        int word_align = ((unsigned int) esp_cpy % 4);

        /* get the size of the args */
        int j;
        for (j = 0; j < word_align; j++)
        {
          esp_cpy -= 1;
          count++;
        }

        /* check size again */
         if (count > PGSIZE)
              return false;

        /* sentinel */
        esp_cpy -= sizeof (char *);
        
        /* addresses */
        int k;
        for (k = argc - 1; k >= 0; k--)
        {
          esp_cpy -= sizeof (char*);
          memcpy (esp_cpy, &arg_addrs[k], sizeof (char*));
        }

        /* argument */
        void *copy_of_esp = esp_cpy;
        esp_cpy -= sizeof (char **);
        memcpy (esp_cpy, &copy_of_esp, sizeof (char *));

        /* argc */
        esp_cpy -= sizeof (int);
        /* using list size since we pushed our arguments into a list */
        memcpy (esp_cpy, &argc, sizeof (int));

        /* return address */
        esp_cpy -= sizeof (void *);
        memcpy (esp_cpy, &zero, sizeof (void *));
        *esp = esp_cpy;
        /* End Sam driving */
      }
      else
        palloc_free_page (kpage);
    }
 
  return success;
  
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
