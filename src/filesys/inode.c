#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include <stdio.h>
/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* Our macros */
#define IB_NUM_BLOCKS 128
#define NUM_BLOCKS_DIRECT 120

/* File Allocation Levels */
#define DIRECT_ALLOC_SPACE 61440
#define INDIRECT_ALLOC_SPACE 126976

/* must support file of 8MB size */
#define MAX_FILE_SIZE 8980480
                  


/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE (512) bytes long. */
struct inode_disk
  {
    block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */  
    int direct_block_index;
    int indirect_block_index;
    int dbly_indirect_index;
    block_sector_t direct_blocks[NUM_BLOCKS_DIRECT];               
    block_sector_t indirect_block;
    block_sector_t dbly_indirect_block;
    unsigned magic;                     /* Magic number. */
  };

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
    struct lock inode_lock;             /* Synchronization per file */
  };

/*an indirect block pointer*/
struct indirect_block
  {
    /* n pointers to blocks */
    block_sector_t blocks[IB_NUM_BLOCKS];
    /*pointer to next free block in array
      not on disk*/
    // TODO: may not need
    int index;
  };

block_sector_t byte_to_direct_sector (const struct inode *inode, off_t pos);
block_sector_t byte_to_indirect_sector (const struct inode *inode, 
                                        off_t pos);
block_sector_t byte_to_dbly_indirect_sector (const struct inode *inode, 
                                             off_t pos);    
bool inode_expand (struct inode_disk *disk_inode, off_t new_size);

void free_indirect_block (struct indirect_block);



/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* Index of directly allocated sector */
block_sector_t 
byte_to_direct_sector (const struct inode *inode, off_t pos)
{
  ASSERT (pos < DIRECT_ALLOC_SPACE);
  /* Our sector index can be found in direct allocated blocks */
  int sector_index = pos / BLOCK_SECTOR_SIZE;
  return inode->data.direct_blocks[sector_index];
}

/* Index of singly indirect allocated sector */
block_sector_t 
byte_to_indirect_sector (const struct inode *inode, off_t pos)
{
  ASSERT (pos < INDIRECT_ALLOC_SPACE);
  /* Must index into the indirect block so we must subtract direct block
     number from total sectors spanned by pos */
  int sector_index = (pos / BLOCK_SECTOR_SIZE) - NUM_BLOCKS_DIRECT;
  /* must read in the right sector from the indirect block index into temp 
     buffer */
  struct indirect_block temp_indirect;
  block_read (fs_device, inode->data.indirect_block_index, 
              &temp_indirect.blocks);
  /* Ensure we can index into our array of blocks */
  ASSERT (sector_index < IB_NUM_BLOCKS);
  return temp_indirect.blocks[sector_index];
}

/* Index of doubly indirect allocated sector */
block_sector_t 
byte_to_dbly_indirect_sector (const struct inode *inode, off_t pos)
{
  ASSERT (pos < MAX_FILE_SIZE);
  /* Must index into the double indirect block so we must subtract direct 
     and indirect blocks from total sectors spanned by pos */
  int sector_index;
  int index_IB;
  int index_DIB;
  sector_index = (pos / BLOCK_SECTOR_SIZE) - NUM_BLOCKS_DIRECT - IB_NUM_BLOCKS;
  index_DIB = sector_index / IB_NUM_BLOCKS;
  index_IB = sector_index % IB_NUM_BLOCKS;
  /* read in the right sector from the double indirect block index into 
     a temporary buffer */
  struct indirect_block temp_dbl_indirect;
  /* must also then read in the right indirect block index into a buffer */
  struct indirect_block temp_indirect;
  /* read in double indirect */
  block_read (fs_device, inode->data.dbly_indirect_block,
               &temp_dbl_indirect.blocks);
  ASSERT (index_DIB < IB_NUM_BLOCKS);
  /* read in the specified singly indirect block stored inside
     the double indirect */
  block_read (fs_device, temp_dbl_indirect.blocks[index_DIB], 
              &temp_indirect.blocks);
  return temp_indirect.blocks[index_IB];
}                     


/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  // TODO: 3 levels: check if pos is in direct level, 1st indirect, 2nd indirect
  if (pos < inode_length (inode))
  {
    /* Check if specified pos is within direct allocation */
    if (pos < DIRECT_ALLOC_SPACE)
    {
      return byte_to_direct_sector (inode, pos);
    }
    /* Check if specified pos is within indirect allocation */
    else if (pos < INDIRECT_ALLOC_SPACE)
    {
      return byte_to_indirect_sector (inode, pos);
    }
    else
    {
      /* Otherwise pos is within scope of doubly indirect allocation */
      return byte_to_dbly_indirect_sector (inode, pos);
    }
  }
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Function to expand a file to new_size */
bool
inode_expand (struct inode_disk *disk_inode, off_t new_size)
{

  ASSERT (new_size >= disk_inode->length);
  size_t sectors = bytes_to_sectors (new_size) - 
                   bytes_to_sectors (disk_inode->length);
  //printf ("in expand!\n");
  if (sectors == 0)
  {
    disk_inode->length = new_size;
    return true;
  }
         
  static char zeros[BLOCK_SECTOR_SIZE];
  /* Direct Allocation */
  while (disk_inode->direct_block_index < NUM_BLOCKS_DIRECT)
  {
    if (!free_map_allocate (1, 
        &disk_inode->direct_blocks[disk_inode->direct_block_index]))
    {
      return false;
    }
    block_write (fs_device, 
                 disk_inode->direct_blocks[disk_inode->direct_block_index], 
                 zeros);
    disk_inode->direct_block_index++;
    sectors--;
    if (sectors == 0)
    {
      disk_inode->length = new_size;
      return true;
    }
  }

  /* Indirect Allocation */
  /* read in the 1st IB */ 
  ASSERT (disk_inode->direct_block_index >= NUM_BLOCKS_DIRECT);
  struct indirect_block temp_IB;
  //printf ("read 0\n");
  block_read (fs_device, disk_inode->indirect_block, &temp_IB.blocks);
  /* Allocate while in scope of our singly indirect block */
  while (disk_inode->indirect_block_index < IB_NUM_BLOCKS)
  {
    if (!free_map_allocate (1, 
        &temp_IB.blocks[disk_inode->indirect_block_index]))
    {
      return false;
    }
    //printf ("write 0\n");
    /* Zero out newly allocated block */
    block_write (fs_device, temp_IB.blocks[disk_inode->indirect_block_index], 
                 zeros);
    disk_inode->indirect_block_index++;
    sectors--;
    if (sectors == 0)
    {
      //printf ("write 1\n");
      /* Update metadata before returning true */
      block_write (fs_device, disk_inode->indirect_block, &temp_IB.blocks);
      disk_inode->length = new_size;
      return true;
    }
  }
  /* Update metadata before proceeding */
  block_write (fs_device, disk_inode->indirect_block, &temp_IB.blocks);


  /* Double Indirect Allocation */
  /* Ensure all the levels preceding DIB Allocation */
  ASSERT (disk_inode->direct_block_index >= NUM_BLOCKS_DIRECT);
  ASSERT (disk_inode->indirect_block_index >= IB_NUM_BLOCKS);
  /* Read in our DIB */
  struct indirect_block temp_DIB;
  //printf ("read 1\n");
  block_read (fs_device, disk_inode->dbly_indirect_block, &temp_DIB.blocks);
  /* Find which IB we are at in the file */
  temp_DIB.index = disk_inode->dbly_indirect_index / IB_NUM_BLOCKS;
  /* Allocate as long as we are in the scope of MAX_FILE_SIZE */
  while (disk_inode->dbly_indirect_index < IB_NUM_BLOCKS * IB_NUM_BLOCKS)
  { 
    /* Check to see if we need to allocate for a new second level IB*/
    if (disk_inode->dbly_indirect_index % IB_NUM_BLOCKS == 0)
    {
      if (!free_map_allocate (1, &temp_DIB.blocks[temp_DIB.index]))
      {
        return false;
      }
      temp_DIB.index++;
    }
    //printf ("read 2\n");
    /* Read in our second level IB */
    block_read (fs_device, temp_DIB.blocks[temp_DIB.index], &temp_IB.blocks);
    /* Find which third level data block to allocate for */
    temp_IB.index = disk_inode->dbly_indirect_index % IB_NUM_BLOCKS;
    /* Allocate data blocks while we are in the scope of current IB */
    while (temp_IB.index < IB_NUM_BLOCKS)
    {
      if (!free_map_allocate (1, &temp_IB.blocks[temp_IB.index]))
      {
        return false;
      }
      //printf ("write 2\n");
      /* Zero out newly allocated block */
      block_write (fs_device, temp_IB.blocks[disk_inode->indirect_block_index], 
                   zeros);
      temp_IB.index++;
      disk_inode->dbly_indirect_index++;
      sectors--;
      if (sectors == 0)
      {
        // FIXME: repetitive logic
        /* Update metadate before returning */
        //printf ("write 3\n");
        block_write (fs_device, temp_DIB.blocks[temp_DIB.index], &temp_IB.blocks);
        //printf ("write 4\n");
        block_write (fs_device, disk_inode->dbly_indirect_block, &temp_DIB.blocks);
        disk_inode->length = new_size;
        return true;
      }
    }
    //TODO: check to actually see if we have filled up current IB
    /* Update our second level IB block */
    //printf ("write 5\n");
    block_write (fs_device, temp_DIB.blocks[temp_DIB.index], &temp_IB.blocks);
  }
  /* Update our first level DIB block */
  //printf ("write 6\n");
  block_write (fs_device, disk_inode->dbly_indirect_block, &temp_DIB.blocks);
  return false;

}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      //size_t sectors = bytes_to_sectors (length);
      disk_inode->length = 0;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->direct_block_index = 0;
      disk_inode->indirect_block_index = 0;
      disk_inode->dbly_indirect_index = 0;
      
      static char zeros[BLOCK_SECTOR_SIZE];
      /* First find a free sector for our indexed blocks */
      free_map_allocate (1, &disk_inode->indirect_block);
      free_map_allocate (1, &disk_inode->dbly_indirect_block);
      /* Zero out indirect and dbly indirect block sectors */
      //printf ("write 7\n");
      block_write (fs_device, disk_inode->indirect_block, zeros);
      //printf ("write 8\n");
      block_write (fs_device, disk_inode->dbly_indirect_block, zeros);

      if (inode_expand (disk_inode, length))
      {
        //printf ("write 9\n");
        block_write (fs_device, sector, disk_inode);
        success = true;
      }

      /* Write the created inode disk to sector */
      //TODO: figure out where to write this
      // block_write (fs_device, sector, disk_inode);
      //FIXME: possible need to zero out this sector

      // if (free_map_allocate (sectors, &disk_inode->start)) 
      //   {
      //     block_write (fs_device, sector, disk_inode);
      //     if (sectors > 0) 
      //       {
      //         static char zeros[BLOCK_SECTOR_SIZE];
      //         size_t i;
              
      //         for (i = 0; i < sectors; i++) 
      //           block_write (fs_device, disk_inode->start + i, zeros);
      //       }
      //     success = true; 
      //   } 
      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  lock_init (&inode->inode_lock);
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk. (Does it?  Check code.)
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks.
   TODO: figure out what to do here */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length)); 
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. 
   TODO: make sure this is done.*/
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  /*check if we are reading past the files allocation*/
  if (size + offset > inode_length(inode))
  {
    return bytes_read;
  }

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  if (size + offset >= inode_length(inode))
  {
    /*we need to expand the file*/
    if (!inode_expand (&inode->data, (offset + size)))
    {
      return 0;
    }
  }
  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
