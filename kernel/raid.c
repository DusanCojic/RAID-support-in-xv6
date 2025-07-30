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

// raid data for every disk stored to avoid deserialization at every access
struct raid_data raid1_data_cache[VIRTIO_RAID_DISK_END];
uchar raid1_data_cache_loaded = 0;

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
      metadata_ptr[i] = buffer[j];

    // load cache
    raid1_data_cache[i-1] = metadata;
  }
}

int read_raid1(int blkn, uchar* data) {
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
  int block = blkn + 1;
  // invalid block number
  if (block < 1 || block > NUMBER_OF_BLOCKS - 1) return -1;

  read_block(disk_number, block, data);

  return 0;
}

int write_raid1(int blkn, uchar* data) {
  load_raid1_data_cache();

  int block = blkn + 1;
  // invalid block
  if (block < 1 || block > NUMBER_OF_BLOCKS - 1) return -1;

  int ret = -1;
  // iteratre over all disks
  for (int disk_num = VIRTIO_RAID_DISK_START; disk_num <= VIRTIO_RAID_DISK_END; disk_num++) {
    // check if disk is working
    if (raid1_data_cache[disk_num-1].working == 1) {
      write_block(disk_num, block, data);
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

int info_raid1(uint *blkn, uint *blks, uint *diskn) {
  (*blkn) = NUMBER_OF_BLOCKS - 1;
  (*blks) = BSIZE;
  (*diskn) = VIRTIO_RAID_DISK_END;

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


int init_raid(enum RAID_TYPE raid) {
  switch (raid) {
    case RAID1: return init_raid1();
    
    default:
      return -1;
  }

  return 0;
}

int read_raid(int blkn, uchar* data) {
  read_raid1(blkn, data);

  return 0;
}

int write_raid(int blkn, uchar* data) {
  write_raid1(blkn, data);

  return 0;
}

int disk_fail_raid(int diskn) {
  disk_fail_raid1(diskn);

  return 0;
}

int disk_repaired_raid(int diskn) {
  disk_repaired_raid1(diskn);

  return 0;
}

int info_raid(uint *blkn, uint *blks, uint *diskn) {
  info_raid1(blkn, blks, diskn);

  return 0;
}

int destroy_raid() {
  destroy_raid1();

  return 0;
}