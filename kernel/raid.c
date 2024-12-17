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
struct raid_data raid_data_cache[VIRTIO_RAID_DISK_END];
uchar raid_data_cache_loaded = 0;

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

// loading raid data cache if not loaded
void load_raid_data_cache() {
  if (raid_data_cache_loaded) return;

  uchar buffer[BSIZE];
  for (int i = VIRTIO_RAID_DISK_START; i <= VIRTIO_RAID_DISK_END; i++) {
    // read first block of the disk
    read_block(i, 0, buffer);

    // deserialize data
    struct raid_data metadata;
    uchar* metadata_ptr = (uchar*)(&metadata);
    for (int i = 0; i < sizeof(metadata); i++)
      metadata_ptr[i] = buffer[i];

    // load cache
    raid_data_cache[i-1] = metadata;
  }
}

int read_raid1(int blkn, uchar* data) {
  load_raid_data_cache();

  // find working disk
  int disk_number = -1;
  for (int i = VIRTIO_RAID_DISK_START; i <= VIRTIO_RAID_DISK_END; i++)
    if (raid_data_cache[i-1].working == 1)
      disk_number = i;

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
  load_raid_data_cache();

  int block = blkn + 1;
  // invalid block
  if (block < 1 || block > NUMBER_OF_BLOCKS - 1) return -1;

  int ret = -1;
  // iteratre over all disks
  for (int disk_num = VIRTIO_RAID_DISK_START; disk_num <= VIRTIO_RAID_DISK_END; disk_num++) {
    // check if disk is working
    if (raid_data_cache[disk_num-1].working == 1) {
      write_block(disk_num, block, data);
      ret = 0;
    }
  }

  return ret;
}

int disk_fail_raid1(int diskn) {
  if (diskn < 1 || diskn > VIRTIO_RAID_DISK_END) return -1;

  

  return 0;
}

int disk_repaired_raid1(int diskn) {
  

  return 0;
}

int info_raid1(uint *blkn, uint *blks, uint *diskn) {
  

  return 0;
}

int destroy_raid1() {
  

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
  printf("DISK FAIL RAID\n");

  return 0;
}

int disk_repaired_raid(int diskn) {
  printf("DISK REPAIED RAID\n");

  return 0;
}

int info_raid(uint *blkn, uint *blks, uint *diskn) {
  printf("INFO RAID\n");

  return 0;
}

int destroy_raid() {
  printf("DESTROY RAID\n");

  return 0;
}