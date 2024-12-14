#include "kernel/types.h"
#include "user/user.h"

int main() {
  init_raid(RAID0);

  return 0;
}