#include "raid.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"

int init_raid(enum RAID_TYPE raid) {
  printf("INIT RAID\n");

  return 0;
}

int read_raid(int blkn, uchar* data) {
  printf("READ RAID\n");

  return 0;
}

int write_raid(int blkn, uchar* data) {
  printf("WRITE RAID\n");

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