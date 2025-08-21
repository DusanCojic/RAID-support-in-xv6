#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fs.h"

void child_test(int id, int block, int value, int size) {
    uchar* blk = malloc(size);

    blk[0] = value;
    blk[1] = value + 1;
    blk[2] = value + 2;

    if (write_raid(block, blk) != 0) {
        printf("Thread %d: write failed\n", id);
        free(blk);
        return;
    }

    uchar* buffer = malloc(size);
    if (read_raid(block, buffer) == 0)
        printf("Thread %d read: %d %d %d\n", id, buffer[0], buffer[1], buffer[2]);
    else
        printf("Thread %d: read failed\n", id);

    free(blk);
    free(buffer);

    exit(0);
}

int main() {
    init_raid(RAID5);

    uint disk_num, block_num, block_size;
    info_raid(&block_num, &block_size, &disk_num);

    printf("%d %d %d\n", block_num, block_size, disk_num);

    // first child process
    if (fork() == 0)
        child_test(1, 5, 1, block_size);

    // second child process
    if (fork() == 0)
        child_test(2, 5, 4, block_size);

    // second child process
    if (fork() == 0)
        child_test(3, 5, 7, block_size);

    // second child process
    if (fork() == 0)
        child_test(4, 5, 10, block_size);

    wait(0);
    wait(0);
    wait(0);
    wait(0);

    uchar* buffer = malloc(block_size);
    if (read_raid(5, buffer) != 0) {
        printf("Read failed\n");
        return 0;
    }
    else
        printf("%d %d %d\n", buffer[0], buffer[1], buffer[2]);

    destroy_raid();
    free(buffer);

    return 0;
}