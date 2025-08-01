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





// RAID0

// raid data that will be stored on the first block on the first disk
struct raid_data raid0_data_cache;
uchar raid0_data_cache_loaded = 0;

int init_raid0() {
  // initializing raid data structure
  raid0_data_cache.raid_type = RAID0;
  raid0_data_cache.working = 1;

  // serializing raid data structure to a buffer with size of one block
  uchar buffer[BSIZE];
  uchar* metadata_ptr = (uchar*)(&raid0_data_cache);
  int metadata_size = sizeof(struct raid_data);

  for (int i = 0; i < metadata_size; i++)
    buffer[i] = metadata_ptr[i];

  // write raid structure to a first block of the first disk
  write_block(1, 0, buffer);

  raid0_data_cache_loaded = 1;

  return 0;
}

// loading raid data cache if not loaded
void load_raid0_data_cache() {
  if (raid0_data_cache_loaded == 1) return; // cache loaded

  uchar buffer[BSIZE]; // buffer to put the content of the first block with cache data
  struct raid_data metadata;
  uchar* metadata_ptr = (uchar*)(&metadata);
  int metadata_size = sizeof(struct raid_data);

  read_block(1, 0, buffer); // read first block of the first disk

  // initialize cache data structure
  for (int i = 0; i < metadata_size; i++)
    metadata_ptr[i] = buffer[i];

  // indicate that the cache is loaded
  raid0_data_cache_loaded = 1;
}

int read_raid0(int blkn, uchar* data) {
  // cannot read first block
  if (blkn == 0)
    return -1;

  load_raid0_data_cache();

  // Check if raid is working
  if (raid0_data_cache.working == 0)
    return -1;
    
  int num_of_disks = VIRTIO_RAID_DISK_END - 1;

  // calculate disk number where desired block is stored
  int diskn = blkn % num_of_disks+ 1;
  // calculate block number on the disk
  int blockn = blkn / num_of_disks;
  if (diskn == 1) blockn++;

  // write block from the calculated disk in the calculated block
  read_block(diskn, blockn, data);

  return 0;
}

int write_raid0(int blkn, uchar* data) {
  // cannot read first block
  if (blkn == 0)
    return -1;

  load_raid0_data_cache();

  // Check if raid is working
  if (raid0_data_cache.working == 0)
    return -1;

  int num_of_disks = VIRTIO_RAID_DISK_END - 1;

  // calculate disk number where desired block is stored
  int diskn = blkn % num_of_disks + 1;
  // calculate block number on the disk
  int blockn = blkn / num_of_disks;
  if (diskn == 1) blockn++;

  // write block on the calculated disk in the calculated block
  write_block(diskn, blockn, data);

  return 0;
}

int disk_fail_raid0(int diskn) {
  // check if disk number is out of bounds
  if (diskn < 1 || diskn > VIRTIO_RAID_DISK_END)
    return -1;

  // set global working flag to 0
  raid0_data_cache.working = 0;

  // write modified cache in the first block of the first disk
  uchar* metadata_ptr = (uchar*)(&raid0_data_cache);
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

// raid data for every disk stored to avoid deserialization at every access
struct raid_data raid1_data_cache[VIRTIO_RAID_DISK_END];
uchar raid1_data_cache_loaded = 0;

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
    raid1_data_cache[i-1] = metadata;
  }

  raid1_data_cache_loaded = 1;

  return 0;
}

// loading raid data cache if not loaded
void load_raid1_data_cache() {
  if (raid1_data_cache_loaded) return;

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
    raid1_data_cache[i-1] = metadata;
  }
}

int read_raid1(int blkn, uchar* data) {
  // cannot read from the first block
  if (blkn == 0) return -1;

  load_raid1_data_cache();

  // find working disk
  int disk_number = -1;
  for (int i = VIRTIO_RAID_DISK_START; i <= VIRTIO_RAID_DISK_END; i++)
    if (raid1_data_cache[i-1].working == 1) {
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

  load_raid1_data_cache();

  // invalid block
  if (blkn < 1 || blkn > NUMBER_OF_BLOCKS - 1) return -1;

  int ret = -1;
  // iteratre over all disks
  for (int disk_num = VIRTIO_RAID_DISK_START; disk_num <= VIRTIO_RAID_DISK_END; disk_num++) {
    // check if disk is working
    if (raid1_data_cache[disk_num-1].working == 1) {
      write_block(disk_num, blkn, data);
      ret = 0;
    }
  }

  return ret;
}

int disk_fail_raid1(int diskn) {
  if (diskn < 1 || diskn > VIRTIO_RAID_DISK_END) return -1;

  // load cache if not loaded
  load_raid1_data_cache();

  // cannot set disk to be invalid if already invalid
  if (raid1_data_cache[diskn-1].working == 0) return -1;

  // reset working flag for the disk
  raid1_data_cache[diskn-1].working = 0;

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
  load_raid1_data_cache();

  // cannot repair disk if already working
  if (raid1_data_cache[diskn-1].working == 1) return -1;

  // find disk to copy data from
  int disk_to_copy = -1;
  for (int i = VIRTIO_RAID_DISK_START; i <= VIRTIO_RAID_DISK_END; i++)
    if (raid1_data_cache[i-1].working == 1) {
      disk_to_copy = i;
      break;
    }

  if (disk_to_copy == -1) return -1;

  // copy every block from working disk to repaired disk
  for (int block_number = 1; block_number < NUMBER_OF_BLOCKS; block_number++) {
    uchar buffer[BSIZE];
    read_block(disk_to_copy, block_number, buffer);

    write_block(diskn, block_number, buffer);
  }

  // update cache
  raid1_data_cache[diskn - 1].working = 1;

  // write updated cache to the corresponding disk
  uchar* metadata_ptr = (uchar*)(&raid1_data_cache[diskn - 1]);
  write_block(diskn, 0, metadata_ptr);

  return 0;
}

int destroy_raid1() {
  load_raid1_data_cache();

  // write all zeroes in first block of every disk
  for (int i = VIRTIO_RAID_DISK_START; i <= VIRTIO_RAID_DISK_END; i++) {
    if (!raid1_data_cache[i - 1].working) continue;

    uchar buffer[BSIZE];
    for (int j = 0; j < BSIZE; j++)
      buffer[j] = 0;

    write_block(i, 0, buffer);
  }

  return 0;
}





// RAID 0+1

// cache that stores raid data structure
struct raid_data raid01_data_cache[VIRTIO_RAID_DISK_END];
uchar raid01_data_cache_loaded = 0;

int init_raid01() {
  // check for even number of disks, because one disk is reserved by xv6 (need even number of disks without it)
  if (VIRTIO_RAID_DISK_END % 2 == 0 || VIRTIO_RAID_DISK_END - 1 < 2)
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
  for (int i = VIRTIO_RAID_DISK_START; i < VIRTIO_RAID_DISK_END; i++) {
    write_block(i, 0, buffer);
    raid01_data_cache[i-1] = metadata;
  }

  // indicate that the cache is loaded
  raid01_data_cache_loaded = 1;

  return 0;
}

void load_raid01_data_cache() {
  if (raid01_data_cache_loaded == 1)
    return;

  // read 0 block on every disk and initialize cache
  for (int diskn = VIRTIO_RAID_DISK_START; diskn < VIRTIO_RAID_DISK_END; diskn++) {
    uchar buffer[BSIZE];
    read_block(diskn, 0, buffer);

    struct raid_data metadata;
    uchar* metadata_ptr = (uchar*)(&metadata);
    int metadata_size = sizeof(struct raid_data);

    for (int i = 0; i < metadata_size; i++)
      metadata_ptr[i] = buffer[i];

    raid01_data_cache[diskn - 1] = metadata;
  }

  // indicate that the cache is loaded
  raid01_data_cache_loaded = 1;
}

int read_raid01(int blkn, uchar* data) {
  // To be implemented
  return 0;
}

int write_raid01(int blkn, uchar* data) {
  // load cache
  load_raid01_data_cache();

  // calculate disk and block number
  int logical_disks = (VIRTIO_RAID_DISK_END - 1) / 2;
  int group_number = blkn % logical_disks;
  int diskn = group_number * 2 + 1;
  int blockn = blkn / logical_disks;

  printf("%d %d %d %d", logical_disks, group_number, diskn, blockn);

  // 0th block oon every disk is reserved for raid metadata
  if (blockn == 0)
    return -1;

  uchar write = 0;

  // try to write on the first disk in mirror
  if (raid01_data_cache[diskn].working == 1) {
    write_block(diskn, blockn, data);
    write = 1;
  }

  // try to write on the second disk in mirror
  if (raid01_data_cache[diskn + 1].working == 1) {
    write_block(diskn + 1, blockn, data);
    write = 1;
  }

  // error if not written
  if (write == 0)
    return -2;

  return 0;
}

int disk_fail_raid01(int diskn) {
  // To be implemented
  return 0;
}

int disk_repaired_raid01(int diskn) {
  // To be implemented
  return 0;
}

int destroy_raid01() {
  // To be implemented
  return 0;
}





int init_raid(enum RAID_TYPE raid) {
  switch (raid) {
    case RAID0: return init_raid0();
    case RAID1: return init_raid1();
    case RAID0_1: return init_raid01();
    
    default:
      return -1;
  }

  return 0;
}

int read_raid(int blkn, uchar* data) {
  return read_raid01(blkn, data);
}

int write_raid(int blkn, uchar* data) {
  return write_raid01(blkn, data);
}

int disk_fail_raid(int diskn) {
  return disk_fail_raid01(diskn);
}

int disk_repaired_raid(int diskn) {
  return disk_repaired_raid01(diskn);
}

int info_raid(uint *blkn, uint *blks, uint *diskn) {
  (*blkn) = NUMBER_OF_BLOCKS - 1;
  (*blks) = BSIZE;
  (*diskn) = VIRTIO_RAID_DISK_END;

  return 0;
}

int destroy_raid() {
  return destroy_raid01();
}