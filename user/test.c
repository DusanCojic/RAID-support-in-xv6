#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fs.h"

int main() {
  init_raid(RAID4);

  uint disk_num, block_num, block_size;
  info_raid(&block_num, &block_size, &disk_num);

  printf("%d %d %d\n", block_num, block_size, disk_num);

  uchar* blk = malloc(block_size);
  blk[0] = 1;
  blk[1] = 2;
  blk[2] = 3;

  int res_w = write_raid(5, blk);
  if (res_w != 0) {
    printf("Failed to write\n");
    free(blk);
    return 0;
  }

  free(blk);

  // disk_fail_raid(3);
  // disk_fail_raid(4);

  // uchar* buffer = malloc(block_size);
  // int res = read_raid(5, buffer);

  // if (res == 0)
  //   printf("%d %d %d\n", buffer[0], buffer[1], buffer[2]);
  // else
  //   printf("Failed to read\n");

  // disk_repaired_raid(3);

  // res = read_raid(5, buffer);

  // if (res == 0)
  //   printf("%d %d %d\n", buffer[0], buffer[1], buffer[2]);
  // else
  //   printf("Failed to read\n");

  return 0;
}