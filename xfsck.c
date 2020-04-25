#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <limits.h>
#include <string.h>
#define stat xv6_stat // avoid clash with host struct stat
#define dirent xv6_dirent // avoid clash with host struct stat
#include "types.h"
#include "fs.h"
#include "stat.h"
#undef stat
#undef dirent

int main(int argc, char * argv[]) {
    int fd;
    if (argc == 2)
        fd = open(argv[1], O_RDONLY);
    else {
        fprintf(stderr, "%s", "Usage: xcheck <file_system_image>\n");
        exit(1);
    }
    if (fd < 0) {
        fprintf(stderr, "%s", "image not found.\n");
        exit(1);
    }

    struct stat sbuf;

    fstat(fd, & sbuf);

    void * img_ptr = mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if ( * ((int * ) img_ptr) == -1) {
        fprintf(stderr, "%s", "ERROR: Mmap failed.\n");
        exit(1);
    }

    struct superblock * sb = (struct superblock * )(img_ptr + BSIZE);

    int usedBlocks[sb -> size];
    memset(usedBlocks, 0, (sb -> size) * sizeof(usedBlocks[0]));

    int usedInodes[sb -> ninodes];
    memset(usedInodes, 0, (sb -> ninodes) * sizeof(usedInodes[0]));

    int refDirectory[sb -> ninodes];
    memset(refDirectory, 0, (sb -> ninodes) * sizeof(refDirectory[0]));

    struct dinode * dip = (struct dinode * )(img_ptr + 2 * BSIZE);

    //size of super block * 512 
    //size of stat  
    //check 1
    if (sb -> size * BSIZE < sbuf.st_size) {
        fprintf(stderr, "%s", "ERROR: superblock is corrupted.\n");
        exit(1);
    }

    // FS layout:
    // unused | superblock | inodes - 25 blocks | unused | bitmap | data 
    // 01234....2627<28>
    // 1 + 1 + 25 + 1 + 1 + 995 = 1024 blocks
    // Test 2
    for (int i = 0; i < sb -> ninodes; i++) {
        if (dip[i].type < 0 || dip[i].type > 3) {
            fprintf(stderr, "%s", "ERROR: bad inode.\n");
            exit(1);
        }
    }

    //Test 3
    void * bitmap = (void * )(img_ptr + 3 * BSIZE + ((sb -> ninodes / IPB) * BSIZE));
    void * startData = bitmap + (((sb -> nblocks) / (BSIZE * 8)));
    if ((sb -> nblocks) % (BSIZE * 8) > 0) {
        startData = startData + BSIZE;
    }
    uint startData_number = (startData - img_ptr) / BSIZE;
    for (int i = 0; i < sb -> ninodes; i++) {
        if (dip[i].type > 0 && dip[i].type <= 3) {

            for (int x = 0; x < NDIRECT; x++) {
                if (dip[i].addrs[x] != 0 && (dip[i].addrs[x] < startData_number || dip[i].addrs[x] >= startData_number + sb -> nblocks)) {
                    fprintf(stderr, "%s", "ERROR: bad direct address in inode.\n");
                    exit(1);
                }
                if ((dip[i].addrs[x] != 0) && usedBlocks[dip[i].addrs[x]] == 1) {
                    fprintf(stderr, "%s", "ERROR: direct address used more than once.\n");
                    exit(1);
                }
                usedBlocks[dip[i].addrs[x]] = 1;
            }

            if (dip[i].addrs[NDIRECT] != 0 && (dip[i].addrs[NDIRECT] < startData_number || dip[i].addrs[NDIRECT] >= startData_number + sb -> nblocks)) {
                fprintf(stderr, "%s", "ERROR: bad indirect address in inode.\n");
                exit(1);
            }
            if ((dip[i].addrs[NDIRECT] != 0) && usedBlocks[dip[i].addrs[NDIRECT]] == 1) {
                fprintf(stderr, "%s", "ERROR: indirect address used more than once.\n");
                exit(1);
            }
            usedBlocks[dip[i].addrs[NDIRECT]] = 1;
            uint * indirect = (uint * )(img_ptr + BSIZE * dip[i].addrs[NDIRECT]);
            for (int x = 0; x < NINDIRECT; x++) {
                if (indirect[x] != 0 && (indirect[x] < startData_number || indirect[x] >= startData_number + sb -> nblocks)) {
                    fprintf(stderr, "%s", "ERROR: bad indirect address in inode.\n");
                    exit(1);
                }
                if ((indirect[x] != 0) && usedBlocks[indirect[x]] == 1) {
                    fprintf(stderr, "%s", "ERROR: indirect address used more than once.\n");
                    exit(1);
                }
                usedBlocks[indirect[x]] = 1;
            }
        }
    }

    //test 8
    for (int i = 0; i < sb -> ninodes; i++) {
        int totalBlks = 0;
        if (dip[i].type > 0 && dip[i].type <= 3 && dip[i].size > 0) {
            for (int j = 0; j < NDIRECT; j++) {
                if (dip[i].addrs[j] != 0) {
                    totalBlks++;
                }
            }

            if (dip[i].addrs[NDIRECT] != 0) {
                for (int k = 0; k < NINDIRECT; k++) {
                    uint * p = (uint * )(img_ptr + dip[i].addrs[NDIRECT] * BSIZE);
                    if (p[k] != 0) {
                        totalBlks++;
                    }
                }
            }

            if (dip[i].size < (totalBlks - 1) * BSIZE || dip[i].size > (totalBlks * BSIZE)) {
                fprintf(stderr, "%s", "ERROR: incorrect file size in inode.\n");
                exit(1);
            }
        }
    }

    //Test 4
    for (int i = 0; i < sb -> ninodes; i++) {
        if (dip[i].type == 1) {
            struct xv6_dirent * dirEntry = (struct xv6_dirent * )(img_ptr + dip[i].addrs[0] * BSIZE);
            if (strcmp(dirEntry[0].name, ".") != 0 || strcmp(dirEntry[1].name, "..") != 0 || dirEntry[0].inum != i) {
                fprintf(stderr, "%s", "ERROR: directory not properly formatted.\n");
                exit(1);
            }
        }
    }

    //Test 5
    char * startBitmap = (char * )(img_ptr + 3 * BSIZE + ((sb -> ninodes / IPB) * BSIZE));
    for (int i = 0; i < sb -> ninodes; i++) {
        if (dip[i].type > 0 && dip[i].type <= 3) {
            for (int j = 0; j < NDIRECT + 1; j++) {
                if (dip[i].addrs[j] == 0) {
                    continue;
                }
                uint bitArrPos = (dip[i].addrs[j]) / 8;
                uint bitPos = (dip[i].addrs[j]) % 8;
                if (((startBitmap[bitArrPos] >> bitPos) & 1) == 0) {
                    fprintf(stderr, "%s", "ERROR: address used by inode but marked free in bitmap.\n");
                    exit(1);
                }
            }
            uint * indirect = (uint * )(img_ptr + BSIZE * dip[i].addrs[NDIRECT]);
            for (int x = 0; x < NINDIRECT; x++) {
                uint indirectAddr = indirect[x];
                if (indirectAddr == 0) {
                    continue;
                }
                uint bitArrPos = (indirectAddr) / 8;
                uint bitPos = (indirectAddr) % 8;
                if (((startBitmap[bitArrPos] >> bitPos) & 1) == 0) {
                    fprintf(stderr, "%s", "ERROR: address used by inode but marked free in bitmap.\n");
                    exit(1);
                }
            }
        }
    }

    //Test 6
    for (int i = 0; i < sb -> nblocks; i++) {
        uint bitArrPos = (i + startData_number) / 8;
        uint bitPos = (i + startData_number) % 8;
        if (((startBitmap[bitArrPos] >> bitPos) & 1) == 1 && usedBlocks[i + startData_number] != 1) {
            fprintf(stderr, "%s", "ERROR: bitmap marks block in use but it is not in use.\n");
            exit(1);
        }
    }

    //Read through the image and save important things
    for (int i = 0; i < sb -> ninodes; i++) {
        if (dip[i].type == 1) {
            for (int x = 0; x < NDIRECT; x++) {
                if (dip[i].addrs[x] == 0)
                    continue;
                struct xv6_dirent * dirEntry = (struct xv6_dirent * )(img_ptr + dip[i].addrs[x] * BSIZE);
                for (int j = 0; j < (BSIZE / sizeof(struct xv6_dirent)); j++) {
                    if (x > 0 || j > 1) {
                        refDirectory[dirEntry[j].inum] += 1;
                    }
                    usedInodes[dirEntry[j].inum] += 1;
                }
            }
            uint * indirect = (uint * )(img_ptr + BSIZE * dip[i].addrs[NDIRECT]);
            for (int x = 0; x < NINDIRECT; x++) {
                if (indirect[x] == 0)
                    continue;
                struct xv6_dirent * dirEntry = (struct xv6_dirent * )(img_ptr + indirect[x] * BSIZE);
                for (int j = 0; j < (BSIZE / sizeof(struct xv6_dirent)); j++) {
                    usedInodes[dirEntry[j].inum] += 1;
                    refDirectory[dirEntry[j].inum] += 1;
                }
            }
        }
    }

    // test 9
    for (int i = 1; i < sb -> ninodes; i++) {
        if (dip[i].type > 0 && dip[i].type <= 3 && usedInodes[i] == 0) {
            fprintf(stderr, "%s", "ERROR: inode marked used but not found in a directory.\n");
            exit(1);
        }
    }
    // test 10
    for (int i = 1; i < sb -> ninodes; i++) {
        if (usedInodes[i] != 0 && dip[i].type == 0) {
            fprintf(stderr, "%s", "ERROR: inode referred to in directory but marked free.\n");
            exit(1);
        }
    }
    // test 11
    for (int i = 1; i < sb -> ninodes; i++) {
        if (dip[i].type == 2 && usedInodes[i] != dip[i].nlink) {
            fprintf(stderr, "%s", "ERROR: bad reference count for file.\n");
            exit(1);
        }
    }
    // test 12
    for (int i = 1; i < sb -> ninodes; i++) {
        if (dip[i].type == 1 && refDirectory[i] > 1) {
            fprintf(stderr, "%s", "ERROR: directory appears more than once in file system.\n");
            exit(1);
        }
    }

    //accessible directory
    int numdir = 0;
    for (int i = 0; i < sb -> ninodes; ++i) {
        if (dip[i].type == 1) {
            numdir++;
        }
    }

    //Store reference for root directory

    for (int i = 0; i < sb -> ninodes; ++i) {
       if (dip[i].type == 1) {
            //check if it reaches the parent
        }
    }

}