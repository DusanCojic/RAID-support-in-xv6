#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fs.h"

int main() {
  //init_raid(RAID1);

  uchar data1[BSIZE] = {5, 3, 10};
  write_raid(0, data1);

  disk_fail_raid(1);

  disk_repaired_raid(1);

  uchar data2[BSIZE];
  read_raid(0, data2);

  printf("%d %d %d\n", data2[0], data2[1], data2[2]);

  return 0;
}