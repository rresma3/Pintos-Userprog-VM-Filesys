#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"
#include "threads/malloc.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool is_dir) 
{
  block_sector_t inode_sector = 0;
  struct dir *dir = path_to_dir ((char *) name);
  char *f_name = path_to_f_name ((char *) name);
  /* Empty names must return false */
  if (f_name == NULL || dir == NULL)
  {
    free (f_name);
    return false;
  }

  /* Cannot allow creation of . and ..  */
  if(!strcmp(f_name, ".") || !strcmp(f_name, "..") ) 
  {
    free (f_name);
    return false;
  }


  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, is_dir)
                  && dir_add (dir, f_name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);
  free (f_name);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  if (name == NULL || strlen (name) == 0)
    return NULL;

  struct dir *dir = path_to_dir ((char *) name);
  char *f_name = path_to_f_name ((char *) name);
  /* Empty names must return false */
  if (f_name == NULL || dir == NULL)
  {
    free (f_name);
    return NULL;
  }
  struct inode *inode = NULL;

  /* Changing up to a directory */
  if (!strcmp (f_name, ".."))
  {
    /* Find the sector where our parent dir lives */
    block_sector_t parent_sector = inode_get_parent_dir (dir_get_inode (dir));
    inode = inode_open (parent_sector);
    if (inode == NULL)
    {
      free (f_name);
      return NULL;
    }
  }
  /* Staying in same directory or Absolute Path */
  else if (!strcmp (f_name, ".") || (strlen (f_name) == 0 &&
           inode_get_inumber(dir_get_inode (dir)) == ROOT_DIR_SECTOR))
  {
    free (f_name);
    return (struct file *) dir;
  }
  /* Regular case, search for the dir in the file system */
  else
    dir_lookup (dir, f_name, &inode);
  

  dir_close (dir);
  free (f_name);
  
  if (inode == NULL)
    return NULL;

  if (inode_is_dir (inode))
    return (struct file *) dir_open (inode);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir = path_to_dir ((char *) name);
  char *f_name = path_to_f_name ((char *) name);
  /* Empty names must return false */
  if (f_name == NULL || dir == NULL)
  {
    free (f_name);
    return false;
  }
  bool success = dir != NULL && dir_remove (dir, f_name);
  dir_close (dir); 
  free (f_name);
  return success;
}

/* Changes the current working directory of the process 
   to dir, which may be relative or absolute. Returns true 
   if successful, false on failure. */
bool 
filesys_chdir (const char *name)
{
  struct dir *dir = path_to_dir ((char *) name);
  char *f_name = path_to_f_name ((char *) name);
  struct inode *inode = NULL;
  struct thread *cur_thread = thread_current ();
  
  /* Empty names must return false */
  if (f_name == NULL || dir == NULL)
  {
    free (f_name);
    return false;
  }
  
  /* Changing up to a directory */
  if (!strcmp (f_name, ".."))
  {
    /* Find the sector where our parent dir lives */
    block_sector_t parent_sector = inode_get_parent_dir (dir_get_inode (dir));
    inode = inode_open (parent_sector);
    if (inode == NULL)
    {
      free (f_name);
      return false;
    }
  }
  /* Staying in same directory or Absolute Path */
  else if (!strcmp (f_name, ".") || (strlen (f_name) == 0 &&
           inode_get_inumber(dir_get_inode (dir)) == ROOT_DIR_SECTOR))
  {
    cur_thread->cwd = dir;
    free (f_name);
    return true;
  }
  /* Regular case, search for the dir in the file system */
  else
  {
    dir_lookup (dir, f_name, &inode);
  }

  /* Update current thread's directory */
  dir_close (dir);
  free(f_name);

  dir = dir_open (inode);
  if (dir != NULL)
  {
    dir_close(cur_thread->cwd);
    cur_thread->cwd = dir;
    return true;
  }

  return false;
}

/* Creates the directory named dir, which may be relative 
   or absolute. Returns true if successful, false on failure.  
   Fails if dir already exists or if any directory name in dir, 
   besides the last, does not already exist. That is, mkdir("/a/b/c") 
   succeeds only if /a/b already exists and /a/b/c does not. */
bool 
filesys_mkdir (const char *name)
{
  /* simply make an initially empty directory */
  return filesys_create (name, 0, true);
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
