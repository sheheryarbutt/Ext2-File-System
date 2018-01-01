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
#include <sys/stat.h>
#include <math.h>
unsigned char *disk;

struct ext2_inode *inode_table;
struct ext2_super_block *sb;
struct ext2_group_desc *gd;

int main(int argc, char **argv) {
	unsigned char data[EXT2_BLOCK_SIZE];
	FILE * source;
	struct stat source_stat;


    if(argc != 4) {
        fprintf(stderr, "Usage: ext2_cp <image file name> <source path> <dest path>\n");
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
	perror("mmap");
	exit(1);
    }
    // setting global vars
   	sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    gd = (struct ext2_group_desc *)(disk + (2*EXT2_BLOCK_SIZE));
	inode_table = (struct ext2_inode *)(disk + (5*EXT2_BLOCK_SIZE));

	char * path= argv[2];
	char * dest= argv[3];


	source = fopen(path,"rb");

	if (source==NULL){
		// source path does not exist
		fprintf(stderr, "ERROR: %s\n", strerror(ENOENT));     
		exit(ENOENT);
	}

	if(lstat(path,&source_stat)<0){
		fclose(source);
		fprintf(stderr, "ERROR: %s\n", strerror(EXIT_FAILURE));     
		exit(EXIT_FAILURE);
	}

	// need for pathTraversal function
	unsigned int * parent_block= malloc(sizeof(unsigned int *));

	int dest_inode=pathTraversal(dest,disk,parent_block);

	if (dest_inode!=0){
		// already file/directory exist in destination path
		free(parent_block);
		fclose(source);
		fprintf(stderr, "ERROR: %s\n", strerror(EEXIST));     
		exit(EEXIST);
	}
	
	char *name;

	free(parent_block);

	char *cpyPath=malloc((unsigned int)strlen(dest) + 1);
	strcpy(cpyPath,dest);

	parent_block= malloc(sizeof(unsigned int *));
	
	name=basename(cpyPath);
	if (strlen(name)>EXT2_NAME_LEN){
		fclose(source);
		fprintf(stderr, "ERROR: %s\n", strerror(ENAMETOOLONG));     
		exit(ENAMETOOLONG);
	}

	dest[strlen(dest)-(strlen(name))]='\0';

	dest_inode=pathTraversal(dest,disk,parent_block);
	free(parent_block);

	if (dest_inode==0){
		//destination path does not exist
		fclose(source);
		fprintf(stderr, "ERROR: %s\n", strerror(ENOENT));     
		exit(ENOENT);
	}

	int source_size = (int)source_stat.st_size;
 	int blocks_needed = ceil((double)source_size/EXT2_BLOCK_SIZE);
	//no space to copy file

	if ((gd->bg_free_blocks_count<1+blocks_needed) || (gd->bg_free_inodes_count<1) ){
		fclose(source);
		fprintf(stderr, "ERROR: %s\n", strerror(ENOSPC));     
		exit(ENOSPC);
	}
		//can only copy regular files
	if(!(S_ISREG(source_stat.st_mode))){
		fclose(source);
		fprintf(stderr, "ERROR: %s\n", strerror(EPERM));     
		exit(EPERM);
	}
	else {
			// inode for new file and datablock for dir entry for new file
		int avail_ib;
		avail_ib= set_avail_ib();
		int avail_db;
		avail_db= set_avail_db();
			//find next block pointer to use for new dir entry
		int block_pointer=0; 
		while(inode_table[dest_inode-1].i_block[block_pointer]!=0){
			block_pointer++;
		}
			//update dir info
		inode_table[dest_inode-1].i_block[block_pointer]=avail_db;
		inode_table[dest_inode-1].i_blocks+=2;
		inode_table[dest_inode-1].i_size+=EXT2_BLOCK_SIZE;
			//create dir entry for new file
		create_entry((EXT2_BLOCK_SIZE*avail_db),(unsigned int)avail_ib,(unsigned short)EXT2_BLOCK_SIZE,(unsigned char)strlen(name),(unsigned char)EXT2_FT_REG_FILE,name);

			// copy data from source file into new datablocks

		unsigned int * blocks=malloc(sizeof(unsigned int)*15);
		int read_so_far;
		for (int i=0;i<12;i++){

			if (blocks_needed!=0){
				int new_db = set_avail_db();
				blocks[i]=new_db;
				read_so_far = fread(data, 1, EXT2_BLOCK_SIZE, source);
				memcpy(disk+(EXT2_BLOCK_SIZE*new_db),data,read_so_far);
				blocks_needed--;
			}else{
				blocks[i]=0;
			}

		}
			// copy data from source file using indirection
		if (blocks_needed!=0){
			int indirect_block = set_avail_db();
				// new indirect block pointer for new file
			unsigned int * block_num_table = malloc(EXT2_BLOCK_SIZE);

			for(int i=0;i<(EXT2_BLOCK_SIZE/sizeof(unsigned int));i++){ 
				if (blocks_needed != 0){
					unsigned int new_db = (unsigned int)set_avail_db();
					read_so_far = fread(data, 1, EXT2_BLOCK_SIZE, source);
					memcpy(disk+(EXT2_BLOCK_SIZE*new_db),data,read_so_far);
					block_num_table[i] =(unsigned int)new_db;
					blocks_needed--;

				}else{
					block_num_table[i]=(unsigned int)0;
				}
			}
				// store block array at indirect block pointer for new file
			memcpy(disk+(EXT2_BLOCK_SIZE*indirect_block),block_num_table,EXT2_BLOCK_SIZE);
			blocks[12]=indirect_block;

		}
			// create inode for new file
		fclose(source);
		blocks_needed = ceil((double)source_size/EXT2_BLOCK_SIZE);

		create_inode(avail_ib,EXT2_S_IFREG,source_size,1,blocks_needed*2,blocks);

	}
	return 0;
}