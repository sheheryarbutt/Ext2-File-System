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

    if(argc != 3) {
        fprintf(stderr, "Usage: ext2_mkdir <image file name> <dest path>\n");
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

	char * path = argv[2];
	remove_trailing_slash(path);
	// need for pathTraversal function
	unsigned int * parent_block= malloc(sizeof(unsigned int *));
	int cur_inode=pathTraversal(path,disk,parent_block);
	
	if (cur_inode!=0){
		// file/directory already exists at path
		free(parent_block);
		fprintf(stderr, "ERROR: %s\n", strerror(EEXIST));     
		exit(EEXIST);
	}

	free(parent_block);
	char *cpyPath=malloc((unsigned int)strlen(path) + 1);
	strcpy(cpyPath,path);
	parent_block= malloc(sizeof(unsigned int *));
	// name of dir we want to create
	char *name;
	name=basename(cpyPath);

	if (strlen(name)>EXT2_NAME_LEN){
		fprintf(stderr, "ERROR: %s\n", strerror(ENAMETOOLONG));     
		exit(ENAMETOOLONG);
	}
	path[strlen(path)-(strlen(name))]='\0';
	cur_inode=pathTraversal(path,disk,parent_block);

	if (cur_inode==0){
		// path to directory we want to create new dir in, doesnt exist
		fprintf(stderr, "ERROR: %s\n", strerror(ENOENT));     
		exit(ENOENT);
	}
	if ((gd->bg_free_blocks_count<2) || (gd->bg_free_inodes_count<1) ){
		// no space left to create new dir
		fprintf(stderr, "ERROR: %s\n", strerror(ENOSPC));     
		exit(ENOSPC);
	}

	//var for available datablocks and inodes
	int avail_db,avail_db2;
	int avail_ib;

	//cannot create directory in reserved inodes, except root
	if(cur_inode==EXT2_ROOT_INO || cur_inode > EXT2_GOOD_OLD_FIRST_INO){

		struct ext2_inode inode = inode_table[cur_inode-1];

		if(!((inode.i_mode & EXT2_S_IFDIR) == EXT2_S_IFDIR)){
			// path to create directory in is not to a directory
			fprintf(stderr, "ERROR: %s\n", strerror(ENOTDIR));     
			exit(ENOTDIR);
		}
		else { 
			int block_pointer=0; 
			//find next block pointer to use for new dir entry
			while(inode_table[cur_inode-1].i_block[block_pointer]!=0){
				block_pointer++;
			}
			//datablock to store new dir entry
			avail_db= set_avail_db();
			//update dir info 
			inode_table[cur_inode-1].i_block[block_pointer]=avail_db;
			inode_table[cur_inode-1].i_blocks+=2;
			inode_table[cur_inode-1].i_size+=EXT2_BLOCK_SIZE;
			inode_table[cur_inode-1].i_links_count+=1;

			//datablock and inode for new dir
			avail_ib= set_avail_ib();
			avail_db2= set_avail_db();

			unsigned int * blocks=malloc(sizeof(unsigned int)*15);

			blocks[0]=avail_db2;
			// create inode for new dir
			create_inode(avail_ib,EXT2_S_IFDIR,EXT2_BLOCK_SIZE,2,2,blocks);
			// create dir entry for new dir
			create_entry((EXT2_BLOCK_SIZE*avail_db),(unsigned int)avail_ib,(unsigned short)EXT2_BLOCK_SIZE,(unsigned char)strlen(name),(unsigned char)EXT2_FT_DIR,name);
			// create dir entries for . and ..
			create_entry((EXT2_BLOCK_SIZE*avail_db2),(unsigned int)avail_ib,(unsigned short)12,(unsigned char)1,(unsigned char)EXT2_FT_DIR,".");
			create_entry((EXT2_BLOCK_SIZE*avail_db2)+12,(unsigned int)cur_inode,(unsigned short)1012,(unsigned char)2,(unsigned char)EXT2_FT_DIR,"..");

		}

	}else{
		fprintf(stderr, "ERROR: %s\n", strerror(EPERM));     
		exit(EPERM);
		
	}
					   	   
    return 0;
}