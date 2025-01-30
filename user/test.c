#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fs.h"

int main() {
  init_raid(RAID1);

  uint disk_num, block_num, block_size;
  info_raid(&block_num, &block_size, &disk_num);

  printf("%d\n", block_size);

  uchar* blk = malloc(block_size);

  free(blk);

  return 0;
}