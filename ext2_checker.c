#include "ext2_utils.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>
unsigned char *disk;

struct ext2_inode *inode_table;
struct ext2_super_block *sb;
struct ext2_group_desc *gd;

int main(int argc, char **argv) {

    if(argc != 2) {
        fprintf(stderr, "Usage: ext2_checker <image file name>\n");
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
	perror("mmap");
	exit(1);
    }
    int fixes=0;
    sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    gd = (struct ext2_group_desc *)(disk + (2*EXT2_BLOCK_SIZE));
	inode_table = (struct ext2_inode *)(disk + (5*EXT2_BLOCK_SIZE));

	int actual_free_inodes=0;
	int actual_free_blocks=0;
	unsigned int * bb=(unsigned int *)(disk + (EXT2_BLOCK_SIZE*3));

	for (int i=0; i<4; i++){ //4= 128/32 because 128 is the total block and 32 is 4 bytes=unsigned int
		int j=0;	
		while(j<32){
			if (bb[i]&(1<<j)){
				actual_free_blocks++;
			}	
			j++;
		}	
	}
	int j=0;
	unsigned int * ib = (unsigned int *)(disk+(EXT2_BLOCK_SIZE*4));
	while(j<32){
			if (ib[0]&(1<<j)){
				actual_free_inodes++;
			}
			j++;
	}
	int diff;
	if (sb->s_free_blocks_count!=actual_free_blocks){
		diff=abs(sb->s_free_blocks_count-actual_free_blocks);
		sb->s_free_blocks_count=actual_free_blocks;
		fixes+=diff;
		printf("Fixed: superblock's free blocks counter was off by %d compared to the bitmap\n",diff);
	}
	if (sb->s_free_inodes_count!=actual_free_inodes){
		diff=abs(sb->s_free_inodes_count-actual_free_inodes);
		sb->s_free_inodes_count=actual_free_inodes;
		fixes+=diff;
		printf("Fixed: superblock's free inodes counter was off by %d compared to the bitmap\n",diff);
	}
	if (gd->bg_free_blocks_count!=actual_free_blocks){
		diff=abs(gd->bg_free_blocks_count-actual_free_blocks);
		gd->bg_free_blocks_count=actual_free_blocks;
		fixes+=diff;
		printf("Fixed: block group's free blocks counter was off by %d compared to the bitmap\n",diff);
	}
	if (gd->bg_free_inodes_count!=actual_free_inodes){
		diff=abs(gd->bg_free_inodes_count-actual_free_inodes);
		gd->bg_free_inodes_count=actual_free_inodes;
		fixes+=diff;
		printf("Fixed: block group's free inodes counter was off by %d compared to the bitmap\n",diff);
	}
	if(fixes!=0){
		printf("%d file system inconsistencies repaired!\n",fixes);
	}else{
		printf("No file system inconsistencies detected!\n");
	}			   	   
    return 0;
}