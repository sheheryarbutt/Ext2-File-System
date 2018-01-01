#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <libgen.h>
#include "ext2.h"
#include "ext2_utils.h"
#include "errno.h"

unsigned char *disk;

struct ext2_inode *inode_table;
struct ext2_super_block *sb;
struct ext2_group_desc *gd;



int main(int argc, char **argv) {
	if(argc != 3) { 
        fprintf(stderr, "Usage: ext2_ls <image file name> <Absolute path>\n");
         exit(1);
    }
    int fd = open(argv[1], O_RDWR);

	char *path = malloc(sizeof(char)*((unsigned int)strlen(argv[2]) + 1));
	strcpy(path, argv[2]);
	path[(unsigned int)strlen(argv[2])] = '\0';

    disk = mmap(NULL, 128 * EXT2_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
		perror("mmap");
		exit(1);
 	}

	inode_table = (struct ext2_inode *)(disk + (5*EXT2_BLOCK_SIZE));

	unsigned int * parent_block= malloc(sizeof(unsigned int *));
	int cur_inode;
	if((unsigned int)strlen(path)==1 && strcmp(path,"/")==0){
		cur_inode=2;
	}
	else{
		remove_trailing_slash(path);
	 	cur_inode=pathTraversal(path,disk,parent_block);
	}
	
	struct ext2_inode inode = inode_table[cur_inode-1];// represents the current inode
	if(valid(cur_inode) && (inode.i_mode & EXT2_S_IFDIR) == EXT2_S_IFDIR){//inode of the directory
		//S_ISDIR(inode.i_mode)
		int size=0;

		int current_i_block_index=0;
		int current_i_block=inode.i_block[current_i_block_index];
		// READ THROUGH ALL THE BLOCKS
		while (current_i_block){
			struct ext2_dir_entry *direntry = (struct ext2_dir_entry *)(disk + current_i_block*EXT2_BLOCK_SIZE +size); 
			//read through each individual block
			while(size<EXT2_BLOCK_SIZE){
				if(direntry->inode){
				char name [direntry->name_len+1]; strncpy(name,direntry->name,direntry->name_len);
				name[direntry->name_len]='\0';	
				printf("%s\n ",name);
				 //add to size so we can increment the pointer to point to next entry!
				size+=direntry->rec_len;
				direntry = (struct ext2_dir_entry *)(disk + current_i_block*EXT2_BLOCK_SIZE +size);
				}else{ 
					size+=direntry->rec_len;
				}
			}// end of inner while
			size=0;
			current_i_block_index++;
			current_i_block=inode.i_block[current_i_block_index];
		}//end of outter while
	}
	else{
		fprintf(stderr, "ERROR: %s\n", strerror(ENOENT)); 
	  	exit(ENOENT);
	}
	free(path); free(parent_block);
	return 0;
}