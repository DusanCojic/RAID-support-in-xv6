#include "types.h"

enum RAID_TYPE {RAID_NONE = 0, RAID0, RAID1, RAID0_1, RAID4, RAID5};
int init_raid(enum RAID_TYPE raid);
int read_raid(int blkn, uchar* data);
int write_raid(int blkn, uchar* data);
int disk_fail_raid(int diskn);
int disk_repaired_raid(int diskn);
int info_raid(uint *blkn, uint *blks, uint *diskn);
int destroy_raid();

void init_raidlock();