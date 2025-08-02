#include "raid.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"

// raid data structure
struct raid_data {
  enum RAID_TYPE raid_type;
  uchar working;
};

struct raid_data raid_data_cache[VIRTIO_RAID_DISK_END];
uchar raid_data_cache_loaded = 0;

void load_raid_data_cache() {
  if (raid_data_cache_loaded) return;

  uchar buffer[BSIZE];
  for (int i = VIRTIO_RAID_DISK_START; i <= VIRTIO_RAID_DISK_END; i++) {
    // read first block of the disk
    read_block(i, 0, buffer);

    // deserialize data
    struct raid_data metadata;
    uchar* metadata_ptr = (uchar*)(&metadata);
    for (int j = 0; j < sizeof(metadata); j++)
      metadata_ptr[j] = buffer[j];

    // load cache
    raid_data_cache[i-1] = metadata;
  }
}





// RAID0

int init_raid0() {
  // initializing raid data structure
  raid_data_cache[0].raid_type = RAID0;
  raid_data_cache[0].working = 1;

  // serializing raid data structure to a buffer with size of one block
  uchar buffer[BSIZE];
  uchar* metadata_ptr = (uchar*)(&raid_data_cache[0]);
  int metadata_size = sizeof(struct raid_data);

  for (int i = 0; i < metadata_size; i++)
    buffer[i] = metadata_ptr[i];

  // write raid structure to a first block of the first disk
  write_block(1, 0, buffer);

  raid_data_cache_loaded = 1;

  return 0;
}

int read_raid0(int blkn, uchar* data) {
  // cannot read first block
  if (blkn == 0)
    return -1;

  load_raid_data_cache();

  // Check if raid is working
  if (raid_data_cache[0].working == 0)
    return -1;
    
  int num_of_disks = VIRTIO_RAID_DISK_END;

  // calculate disk number where desired block is stored
  int diskn = blkn % num_of_disks + 1;
  // calculate block number on the disk
  int blockn = blkn / num_of_disks;
  if (diskn == 1 && blockn == 0) return -2; // cannot access 0th block on the first disk

  // block number outside of the range
  if (blockn > NUMBER_OF_BLOCKS - 1)
    return -1;

  // write block from the calculated disk in the calculated block
  read_block(diskn, blockn, data);

  return 0;
}

int write_raid0(int blkn, uchar* data) {
  // cannot read first block
  if (blkn == 0)
    return -1;

  load_raid_data_cache();

  // Check if raid is working
  if (raid_data_cache[0].working == 0)
    return -1;

  int num_of_disks = VIRTIO_RAID_DISK_END;

  // calculate disk number where desired block is stored
  int diskn = blkn % num_of_disks + 1;
  // calculate block number on the disk
  int blockn = blkn / num_of_disks;
  if (diskn == 1) blockn++;

  // block number outside of the range
  if (blockn > NUMBER_OF_BLOCKS - 1)
    return -1;

  // write block on the calculated disk in the calculated block
  write_block(diskn, blockn, data);

  return 0;
}

int disk_fail_raid0(int diskn) {
  // check if disk number is out of bounds
  if (diskn < 1 || diskn > VIRTIO_RAID_DISK_END)
    return -1;

  // set global working flag to 0
  raid_data_cache[0].working = 0;

  // write modified cache in the first block of the first disk
  uchar* metadata_ptr = (uchar*)(&raid_data_cache[0]);
  int metadata_size = sizeof(struct raid_data);

  uchar buffer[BSIZE];
  for (int i = 0; i < metadata_size; i++)
    buffer[i] = metadata_ptr[i];

  write_block(1, 0, buffer);

  return 0;
}

int disk_repaired_raid0(int diskn) {
  // repairing a disk in RAID0 does not do anything, since there is no redundancy
  return -1;
}

int destroy_raid0() {
  // write all zeores in the first block of the first disk
  uchar buffer[BSIZE];

  for (int i = 0; i < BSIZE; i++)
    buffer[i] = 0;

  write_block(1, 0, buffer);

  return 0;
}





// RAID1

int init_raid1() {
  // not enough disks for raid1
  if (VIRTIO_RAID_DISK_END <= 1)
    return -1;

  // initializing raid data structure
  struct raid_data metadata;
  metadata.raid_type = RAID1;
  metadata.working = 1;

  // serializing raid data structure to a buffer with size of one block
  uchar buffer[BSIZE];
  uchar* metadata_ptr = (uchar*)(&metadata);
  int metadata_size = sizeof(struct raid_data);

  for (int i = 0; i < metadata_size; i++)
    buffer[i] = metadata_ptr[i];

  // writing raid data to all disks and cache
  for (int i = VIRTIO_RAID_DISK_START; i <= VIRTIO_RAID_DISK_END; i++) {
    write_block(i, 0, buffer);
    raid_data_cache[i-1] = metadata;
  }

  raid_data_cache_loaded = 1;

  return 0;
}

int read_raid1(int blkn, uchar* data) {
  // cannot read from the first block
  if (blkn == 0) return -1;

  load_raid_data_cache();

  // find working disk
  int disk_number = -1;
  for (int i = VIRTIO_RAID_DISK_START; i <= VIRTIO_RAID_DISK_END; i++)
    if (raid_data_cache[i-1].working == 1) {
      disk_number = i;
      break;
    }

  // no working disk found
  if (disk_number == -1) return -1;

  // first block is reserved for raid data structure

  // invalid block number
  if (blkn < 1 || blkn > NUMBER_OF_BLOCKS - 1) return -1;

  read_block(disk_number, blkn, data);

  return 0;
}

int write_raid1(int blkn, uchar* data) {
  // cannot read from the first block
  if (blkn == 0) return -1;

  load_raid_data_cache();

  // invalid block
  if (blkn < 1 || blkn > NUMBER_OF_BLOCKS - 1) return -1;

  int ret = -1;
  // iteratre over all disks
  for (int disk_num = VIRTIO_RAID_DISK_START; disk_num <= VIRTIO_RAID_DISK_END; disk_num++) {
    // check if disk is working
    if (raid_data_cache[disk_num-1].working == 1) {
      write_block(disk_num, blkn, data);
      ret = 0;
    }
  }

  return ret;
}

int disk_fail_raid1(int diskn) {
  if (diskn < 1 || diskn > VIRTIO_RAID_DISK_END) return -1;

  // load cache if not loaded
  load_raid_data_cache();

  // cannot set disk to be invalid if already invalid
  if (raid_data_cache[diskn-1].working == 0) return -1;

  // reset working flag for the disk
  raid_data_cache[diskn-1].working = 0;

  // deserialize disk metadata
  // read metadata for the disk
  uchar buffer[BSIZE];
  read_block(diskn, 0, buffer);

  struct raid_data metadata;
  uchar* metadata_ptr = (uchar*)(&metadata);

  for (int i = 0; i < sizeof(struct raid_data); i++)
    metadata_ptr[i] = buffer[i];

  // reset working flag
  metadata.working = 0;

  // serialize and write back to the disk
  for (int i = 0; i < sizeof(struct raid_data); i++)
    buffer[i] = metadata_ptr[i];

  write_block(diskn, 0, buffer);

  return 0;
}

int disk_repaired_raid1(int diskn) {
  if (diskn < 1 || diskn > VIRTIO_RAID_DISK_END) return -1;

  // load cache if not loaded
  load_raid_data_cache();

  // cannot repair disk if already working
  if (raid_data_cache[diskn-1].working == 1) return -1;

  // find disk to copy data from
  int disk_to_copy = -1;
  for (int i = VIRTIO_RAID_DISK_START; i <= VIRTIO_RAID_DISK_END; i++)
    if (raid_data_cache[i-1].working == 1) {
      disk_to_copy = i;
      break;
    }

  if (disk_to_copy == -1) return -1;

  // copy every block from working disk to repaired disk
  uchar buffer[BSIZE];
  for (int block_number = 1; block_number < NUMBER_OF_BLOCKS; block_number++) {
    read_block(disk_to_copy, block_number, buffer);

    write_block(diskn, block_number, buffer);
  }

  // update cache
  raid_data_cache[diskn - 1].working = 1;

  // write updated cache to the corresponding disk
  uchar* metadata_ptr = (uchar*)(&raid_data_cache[diskn - 1]);
  write_block(diskn, 0, metadata_ptr);

  return 0;
}

int destroy_raid1() {
  load_raid_data_cache();

  // write all zeroes in first block of every disk
  uchar buffer[BSIZE];
  for (int i = VIRTIO_RAID_DISK_START; i <= VIRTIO_RAID_DISK_END; i++) {
    if (!raid_data_cache[i - 1].working) continue;

    for (int j = 0; j < BSIZE; j++)
      buffer[j] = 0;

    write_block(i, 0, buffer);
  }

  return 0;
}





// RAID01

int init_raid01() {
  // check for even number of disks, because one disk is reserved by xv6 (need even number of disks without it)
  if (VIRTIO_RAID_DISK_END % 2 != 0 || VIRTIO_RAID_DISK_END < 2)
    return -1;

  // initialize metadata
  struct raid_data metadata;
  metadata.raid_type = RAID0_1;
  metadata.working = 1;
  // serialize metadata

  uchar buffer[BSIZE];
  uchar* metadata_ptr = (uchar*)(&metadata);
  int metadata_size = sizeof(struct raid_data);

  for (int i = 0; i < metadata_size; i++)
    buffer[i] = metadata_ptr[i];

  // write serialized metadata on every disk in raid and initialize cache
  for (int i = VIRTIO_RAID_DISK_START; i <= VIRTIO_RAID_DISK_END; i++) {
    write_block(i, 0, buffer);
    raid_data_cache[i - 1] = metadata;
  }

  // indicate that the cache is loaded
  raid_data_cache_loaded = 1;

  return 0;
}

int read_raid01(int blkn, uchar* data) {
  // load cache
  load_raid_data_cache();

  // calculate disk and block number
  int logical_disks = VIRTIO_RAID_DISK_END / 2;
  int group_number = blkn % logical_disks;
  int diskn = group_number * 2 + 1;
  int blockn = blkn / logical_disks;

  // printf("Logical disk: %d\nGroup number: %d\nDisk1: %d\nDisk2: %d\nBlock: %d\n", logical_disks, group_number, diskn, diskn + 1, blockn);

  if (blockn == 0)
    return -1;

  uchar read = 0;

  // try to read block from one of the disks in mirror
  if (raid_data_cache[diskn - 1].working == 1) {
    read_block(diskn, blockn, data);
    read = 1;
  }
  else if (raid_data_cache[diskn].working == 1) {
    read_block(diskn + 1, blockn, data);
    read = 1;
  }

  // error if not read
  if (read == 0)
    return -2;
  
  return 0;
}

int write_raid01(int blkn, uchar* data) {
  // load cache
  load_raid_data_cache();

  // calculate disk and block number
  int logical_disks = VIRTIO_RAID_DISK_END / 2;
  int group_number = blkn % logical_disks;
  int diskn = group_number * 2 + 1;
  int blockn = blkn / logical_disks;

  // printf("Logical disk: %d\nGroup number: %d\nDisk1: %d\nDisk2: %d\nBlock: %d\n", logical_disks, group_number, diskn, diskn + 1, blockn);

  // 0th block oon every disk is reserved for raid metadata
  if (blockn == 0)
    return -1;

  uchar write = 0;

  // try to write on the first disk in mirror
  if (raid_data_cache[diskn - 1].working == 1) {
    write_block(diskn, blockn, data);
    write = 1;
  }

  // try to write on the second disk in mirror
  if (raid_data_cache[diskn].working == 1) {
    write_block(diskn + 1, blockn, data);
    write = 1;
  }

  // error if not written
  if (write == 0)
    return -2;

  return 0;
}

int disk_fail_raid01(int diskn) {
  // diskn out of range
  if (diskn < 1 || diskn > VIRTIO_RAID_DISK_END)
    return -1;

  // load cache
  load_raid_data_cache();

  // cannot set disk as invalid if already invalid
  if (raid_data_cache[diskn - 1].working == 0)
    return -1;

  // mark disk as not working
  raid_data_cache[diskn - 1].working = 0;

  // write changed raid data to the disk
  uchar* metadata_ptr = (uchar*)(&raid_data_cache[diskn - 1]);
  int metadata_size = sizeof(struct raid_data);

  metadata_ptr = metadata_ptr;
  metadata_size = metadata_size;

  uchar buffer[BSIZE];
  for (int i = 0; i < metadata_size; i++)
    buffer[i] = metadata_ptr[i];

  write_block(diskn, 0, buffer);
  
  return 0;
}

int disk_repaired_raid01(int diskn) {
  // load cache
  load_raid_data_cache();

  // find disk to copy data from
  int disk_to_copy_from = diskn % 2 != 0 ? diskn + 1 : diskn - 1;

  // disk to copy from is not working
  if (raid_data_cache[disk_to_copy_from - 1].working == 0)
    return -1;

  // copy every block from disk that works
  uchar buffer [BSIZE];
  for (int blockn = 0; blockn < NUMBER_OF_BLOCKS; blockn++) {
    read_block(disk_to_copy_from, blockn, buffer);
    write_block(diskn, blockn, buffer);
  }

  // set disk as working
  raid_data_cache[diskn - 1].working = 1;

  // write updated cache data to corresponding disk
  uchar* metadata_ptr = (uchar*)(&raid_data_cache[diskn - 1]);
  int metadata_size = sizeof(struct raid_data);

  for (int i = 0; i < metadata_size; i++)
    buffer[i] = metadata_ptr[i];

  write_block(diskn, 0, buffer);

  return 0;
}

int destroy_raid01() {
  // check for errors

  // write all zeores in 0th block on the first two disks
  uchar buffer[BSIZE];

  for (int i = 0; i < BSIZE; i++)
    buffer[i] = 0;

  write_block(1, 0, buffer);
  write_block(2, 0, buffer);

  return 0;
}





// RADI4

int init_raid4() {
  // cannot implement raid4 with less than 2 disks
  if (VIRTIO_RAID_DISK_END < 2)
    return -1;

  // initialize metadata
  struct raid_data metadata;
  metadata.raid_type = RAID4;
  metadata.working = 1;

  // serialize metadata
  uchar buffer[BSIZE];
  uchar* metadata_ptr = (uchar*)(&metadata);
  int metadata_size = sizeof(struct raid_data);

  for (int i = 0; i < metadata_size; i++)
    buffer[i] = metadata_ptr[i];

  // write metadata to 0th block of every disk and initialize cache
  for (int diskn = VIRTIO_RAID_DISK_START; diskn <= VIRTIO_RAID_DISK_END; diskn++) {
    write_block(diskn, 0, buffer);

    raid_data_cache[diskn - 1] = metadata;
  }

  // indicate that the cache is loaded
  raid_data_cache_loaded = 1;

  return 0;
}

void calculate_parity(uchar* data, uchar* parity) {
  for (int i = 0; i < BSIZE; i++)
    parity[i] ^= data[i];
}

void recover_missing_block_raid4(int blockn, int disk_to_skip, uchar* data) {
  uchar buffer[BSIZE];
  for (int diskn = VIRTIO_RAID_DISK_START; diskn < VIRTIO_RAID_DISK_END; diskn++) {
    if (diskn == disk_to_skip) continue;

    read_block(diskn, blockn, buffer);

    calculate_parity(buffer, data);
  }

  // add data from parity disk
  read_block(VIRTIO_RAID_DISK_END, blockn, buffer);
  calculate_parity(buffer, data);
}

int read_raid4(int blkn, uchar* data) {
  // cannot read the 0th block (raid data structure)
  if (blkn == 0)
    return -1;

  load_raid_data_cache();

  // calculate disk and block to write data
  int data_disks = VIRTIO_RAID_DISK_END - 1;
  int diskn = blkn % data_disks + 1;
  int blockn = blkn / data_disks;

  // block number outside of the range
  if (blockn < 1 || blockn > NUMBER_OF_BLOCKS - 1)
    return -1;

  // disk with requested block is working
  if (raid_data_cache[diskn - 1].working == 1) {
    read_block(diskn, blockn, data);
    return 0;
  }

  // disk with requested block is not working (recovering data, if possible)
  // parity disk is not working
  if (raid_data_cache[VIRTIO_RAID_DISK_END - 1].working == 0)
    return -2;

  uchar* parity = (uchar*)kalloc();
  memset(parity, 0, BSIZE);

  recover_missing_block_raid4(blockn, diskn, parity);

  for (int i = 0; i < BSIZE; i++)
    data[i] = parity[i];

  return 0;
}

int write_raid4(int blkn, uchar* data) {
  // cannot write to the 0th block (raid data structure)
  if (blkn == 0)
    return -1;

  load_raid_data_cache();

  // calculate disk and block to write data
  int data_disks = VIRTIO_RAID_DISK_END - 1;
  int diskn = blkn % data_disks + 1;
  int blockn = blkn / data_disks;

  // disk with the requested block is not working
  if (raid_data_cache[diskn - 1].working == 0)
    return -2;

  // block number outside of the range
  if (blockn < 1 || blockn > NUMBER_OF_BLOCKS - 1)
    return -1;
  
  write_block(diskn, blockn, data);

  if (raid_data_cache[VIRTIO_RAID_DISK_END - 1].working == 0)
    return 0;

  uchar buffer[BSIZE];

  // allocate one page for parity array (stack not large enough)
  uchar* parity = (uchar*)kalloc();
  memset(parity, 0, BSIZE);

  // /calculate parity
  for (int i = VIRTIO_RAID_DISK_START; i < VIRTIO_RAID_DISK_END; i++) {
    // not need to read current data
    if (i == diskn) continue;

    read_block(i, blockn, buffer);

    calculate_parity(buffer, parity);
  }

  // add current data to the parity
  calculate_parity(data, parity);

  // write parity to the parity disk
  write_block(VIRTIO_RAID_DISK_END, blockn, parity);

  // free alocated memory
  kfree(parity);

  return 0;
}

int disk_fail_raid4(int diskn) {
  // disk number out of bounds
  if (diskn < 1 || diskn > VIRTIO_RAID_DISK_END)
    return -1;

  // indicate that the disk is not working
  raid_data_cache[diskn - 1].working = 0;

  // write it to 0th block
  uchar buffer[BSIZE];
  uchar* metadata_ptr = (uchar*)(&raid_data_cache[diskn - 1]);
  int metadata_size = sizeof(struct raid_data);

  for (int i = 0; i < metadata_size; i++)
    buffer[i] = metadata_ptr[i];

  write_block(diskn, 0, buffer);

  return 0;
}

int disk_repaired_raid4(int diskn) {
  // diskn out of bounds
  if (diskn < 1 || diskn > VIRTIO_RAID_DISK_END)
    return -1;

  // parity disk is not working
  if (raid_data_cache[VIRTIO_RAID_DISK_END - 1].working == 0)
    return -2;

  uchar buffer[BSIZE];
  uchar* parity = (uchar*)kalloc();
  memset(parity, 0, BSIZE);

  for (int blockn = 1; blockn < NUMBER_OF_BLOCKS; blockn++) {
    // calculate parity for given block number on all disks except one that is being repaired
    recover_missing_block_raid4(blockn, diskn, parity);

    // missing data is now in parity array
    // write it on repaired disk
    write_block(diskn, blockn, parity);

    // reset parity
    memset(parity, 0, BSIZE);
  }

  // indicate that the disk is repaired
  raid_data_cache[diskn - 1].working = 1;

  // write it
  uchar* metadata_ptr = (uchar*)(&raid_data_cache[diskn - 1]);
  int metadata_size = sizeof(struct raid_data);

  for (int i = 0; i < metadata_size; i++)
    buffer[i] = metadata_ptr[i];

  write_block(diskn, 0, buffer);

  return 0;
}

int destroy_raid4() {
  // write all zeroes in 0th block of the first disk
  uchar buffer[BSIZE];
  for (int i = 0; i < BSIZE; i++)
    buffer[i] = 0;

  raid_data_cache[VIRTIO_RAID_DISK_START - 1].working = 0;
  write_block(VIRTIO_RAID_DISK_START, 0, buffer);
  
  return 0;
}





// RAID5

int init_raid5() {
  if (VIRTIO_RAID_DISK_END < 1)
    return -1;

  // initialize metadata
  struct raid_data metadata;
  metadata.raid_type = RAID5;
  metadata.working = 1;

  // serialize metadata
  uchar buffer[BSIZE];
  uchar* metadata_ptr = (uchar*)(&metadata);
  int metadata_size = sizeof(struct raid_data);

  for (int i = 0; i < metadata_size; i++)
    buffer[i] = metadata_ptr[i];

  // write in 0th block on every disk
  for (int diskn = VIRTIO_RAID_DISK_START; diskn <= VIRTIO_RAID_DISK_END; diskn++) {
    write_block(diskn, 0, buffer);

    // initialize cache entry
    raid_data_cache[diskn - 1] = metadata;
  }

  // indicate that the cache is loaded
  raid_data_cache_loaded = 1;

  return 0;
}

void recover_missing_block_raid5(int blockn, int disk_to_skip, uchar* parity) {
  uchar buffer[BSIZE];
  for (int diskn = VIRTIO_RAID_DISK_START; diskn <= VIRTIO_RAID_DISK_END; diskn++) {
    // skip disk that's not working
    if (diskn == disk_to_skip) continue;
    
    read_block(diskn, blockn, buffer);

    // calculate parity
    calculate_parity(buffer, parity);
  }
}

int read_raid5(int blkn, uchar* data) {
  int number_of_disks = VIRTIO_RAID_DISK_END;

  // calculate disk and block number
  int stripe = blkn / (number_of_disks - 1);
  int parity_location = stripe % number_of_disks + 1;
  int diskn = blkn % (number_of_disks - 1) + 1;
  diskn = (diskn >= parity_location) ? diskn + 1 : diskn;
  int blockn = stripe;

  // if disk is working, just read the block
  if (raid_data_cache[diskn - 1].working == 1) {
    read_block(diskn, blockn, data);
    return 0;
  }

  // recover by calculating parity
  memset(data, 0, BSIZE);
  recover_missing_block_raid5(blockn, diskn, data);

  return 0;
}

int write_raid5(int blkn, uchar* data) {
  int number_of_disks = VIRTIO_RAID_DISK_END;

  // calculate disk and block number
  int stripe = blkn / (number_of_disks - 1);
  int parity_location = stripe % number_of_disks + 1;
  int diskn = blkn % (number_of_disks - 1) + 1;
  diskn = (diskn >= parity_location) ? diskn + 1 : diskn;
  int blockn = stripe;

  // calculate parity using read-modify-write method
  uchar buffer[BSIZE];
  uchar* parity = (uchar*)kalloc();
  memset(parity, 0, BSIZE);

  read_block(parity_location, blockn, parity);
  read_block(diskn, blockn, buffer);

  calculate_parity(buffer, parity); // exclude old data from parity
  calculate_parity(data, parity); // add new data to parity

  write_block(diskn, blockn, data); // write data
  write_block(parity_location, blockn, parity); // write parity

  // free allocated memory
  kfree(parity);

  return 0;
}

int disk_fail_raid5(int diskn) {
  // To be implemented
  return 0;
}

int disk_repaired_raid5(int diskn) {
  // To be implemented
  return 0;
}

int destroy_raid5() {
  // To be implemented
  return 0;
}





int init_raid(enum RAID_TYPE raid) {
  switch (raid) {
    case RAID0: return init_raid0();
    case RAID1: return init_raid1();
    case RAID0_1: return init_raid01();
    case RAID4: return init_raid4();
    case RAID5: return init_raid5();
    
    default:
      return -1;
  }

  return 0;
}

int read_raid(int blkn, uchar* data) {
  return read_raid5(blkn, data);
}

int write_raid(int blkn, uchar* data) {
  return write_raid5(blkn, data);
}

int disk_fail_raid(int diskn) {
  return disk_fail_raid5(diskn);
}

int disk_repaired_raid(int diskn) {
  return disk_repaired_raid5(diskn);
}

int info_raid(uint *blkn, uint *blks, uint *diskn) {
  (*blkn) = NUMBER_OF_BLOCKS;
  (*blks) = BSIZE;
  (*diskn) = VIRTIO_RAID_DISK_END;

  return 0;
}

int destroy_raid() {
  return destroy_raid5();
}