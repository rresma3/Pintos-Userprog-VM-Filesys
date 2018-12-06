#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"


/* A directory. */
struct dir 
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry 
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };

/* Function to determine if the directory is in use at all */
bool dir_is_empty (struct inode *inode);


/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
//TODO: find out how to modify that inode struct to modify is_dir
bool
dir_create (block_sector_t sector, size_t entry_cnt)
{
  return inode_create (sector, entry_cnt * sizeof (struct dir_entry));
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL; 
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) 
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) 
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (e.in_use && !strcmp (name, e.name)) 
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode) 
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  lock_inode (dir_get_inode ((struct dir*) dir));
  if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;
  unlock_inode (dir_get_inode ((struct dir*) dir));

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Directory operations, must lock */
  lock_inode (dir_get_inode (dir));

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    goto done;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* must add branch to tree, add child to parent */
  /* Ryan Driving */
  struct inode *inode = inode_open (inode_sector);
  if (inode == NULL)
    goto done;
  block_sector_t temp_parent = inode_get_inumber(dir_get_inode (dir));
  inode_set_parent_dir (inode, temp_parent);
  inode_close (inode);
  /* End Driving */

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  unlock_inode (dir_get_inode (dir));
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) 
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Directory operations, must lock */
  lock_inode (dir_get_inode (dir));

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

  /* Miles Drving */
  bool is_dir = inode_is_dir (inode);

  /* Cannot remove directory that is open by a process 
     or in use as a process's CWD */ 
  if(is_dir && inode_get_open_cnt(inode) > 1)
    goto done;

  /* Cannot remove root directory */
  if(is_dir && inode_get_inumber(inode) == ROOT_DIR_SECTOR)
    goto done;
  
  /* Directory should only be allowed to be removed when it is empty */
  if(is_dir && !dir_is_empty(inode))
    goto done;
  /* End Driving */

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e) 
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  unlock_inode (dir_get_inode (dir));
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  lock_inode (dir_get_inode (dir));
  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
      dir->pos += sizeof e;
      if (e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          unlock_inode (dir_get_inode (dir));
          return true;
        } 
    }
  unlock_inode (dir_get_inode (dir));
  return false;
}

/* Brian Driving */
/* Indicates whether or not a directory at INODE is still in use or not,
   adapted from dir_readdir */
bool 
dir_is_empty (struct inode *inode)
{
  struct dir_entry e;
  int temp_pos = 0;

  /* Iterate through inode structure while bytes are to be read */
  while (inode_read_at (inode, &e, sizeof e, temp_pos) == sizeof e)
  {
    temp_pos += sizeof e;
    if (e.in_use == true)
      return false;
  }
  return true;
}
/* End Driving */

/* Ryan Driving */
/* Tokenizes a given directory path */
struct inode* 
path_to_inode (char *path)
{
  printf ("top of method\n");
  /* Hard copy of our path */
  int length = strlen (path) + 1;
  char *path_cpy = malloc (length);
  strlcpy (path_cpy, path, length);
  printf ("after copy\n");

  if (length <= 1)
    return false;

  struct dir  *temp_dir;
  struct inode *temp_inode;
  if (path_cpy[0] == '/')
  {
    temp_dir = dir_open_root ();
    temp_inode = temp_dir->inode;
  }
  else
  {
    temp_dir = thread_current ()->cwd;
    if (temp_dir == NULL)
    {
      temp_dir = dir_open_root ();
    }
    temp_inode = temp_dir->inode;
  }
  char *token, *save_ptr;
  for (token = strtok_r (path_cpy, "/", &save_ptr); token != NULL;
      token = strtok_r (NULL, "/", &save_ptr))
  {
    /*token is the name of the current dir in the path*/
    if (!dir_lookup (temp_dir, token, &temp_inode))
    {
      dir_close (temp_dir);
      return NULL;
    }
    //FIXME: idk whats wrong with this
    if (!get_data_is_dir(temp_inode))
    {
      /*found the file*/
      dir_close (temp_dir);
      return temp_inode;
    }
    temp_dir->inode = temp_inode;
    temp_dir->pos = 0;
    printf ("inside loop\n");
    //TODO: make sure we dont have to change the offset
  }
  return NULL;
  dir_close (temp_dir);
  printf ("end of method\n");
}
/* End Driving */


