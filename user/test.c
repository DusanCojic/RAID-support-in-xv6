#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fs.h"

int main() {
  init_raid(RAID0);

  uint disk_num, block_num, block_size;
  info_raid(&block_num, &block_size, &disk_num);

  printf("%d %d %d\n", block_num, block_size, disk_num);

  uchar* blk = malloc(block_size);
  blk[0] = 1;
  blk[1] = 2;
  blk[2] = 3;

  write_raid(1, blk);
  free(blk);

  uchar* buffer = malloc(block_size);
  read_raid(1, buffer);

  printf("%d %d %d\n", buffer[0], buffer[1], buffer[2]);

  return 0;
}