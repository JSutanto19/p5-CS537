#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "fs.h"


int main(int argc, char* argv){
    int fd 
    
    //check if num files is correct should only be 1 file so argc = 2 
    if(argc == 2){
        fd = open(argv[1], O_RDONLY);
    } else{
        printf("Usage: program fs.img\n");
        exit(1);
    }

    if(fd < 0){
      printf("Usage: %s file not found\n", argv[1]);
      exit(1);
    }

    struct stat sbuf;
    fstat(fd, &sbuf);
    printf("Image that i read is %ld in size\n", sbuf.st_size);

    //mmap 
    void *img_ptr = mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    struct superblock *sb =  

}