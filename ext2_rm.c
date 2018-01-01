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
#include <time.h>
#include "errno.h"

unsigned char *disk;

struct ext2_inode *inode_table;
struct ext2_super_block *sb;
struct ext2_group_desc *gd;



int main(int argc, char **argv) {
	if(argc != 3) { 
        fprintf(stderr, "Usage: ext2_rm <image file name>  <Absolute path>\n");
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

 	sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    gd = (struct ext2_group_desc *)(disk + (2*EXT2_BLOCK_SIZE));
		 
 	inode_table = (struct ext2_inode *)(disk + (5*EXT2_BLOCK_SIZE));

 	remove_trailing_slash(path);
	//the block the files in that we want to remove
	unsigned int * parent_block= malloc(sizeof(unsigned int *));
	int cur_inode=pathTraversal(path,disk,parent_block);

	if(valid(cur_inode)){ 
		struct ext2_inode inode = inode_table[cur_inode-1];// represents the current inode

		if((inode.i_mode & EXT2_S_IFDIR) == EXT2_S_IFDIR){//is it a directory? raise error!
			fprintf(stderr, "ERROR: %s\n", strerror(EISDIR)); 
			exit(EISDIR);
		}
		else if((inode.i_mode & EXT2_S_IFREG) == EXT2_S_IFREG) { 
			//1) Update the directory entries
			int size=0;
			struct ext2_dir_entry *direntry = (struct ext2_dir_entry *)(disk + *parent_block*EXT2_BLOCK_SIZE +size);
			unsigned short prev_entry=0; unsigned short prev_rec_len=0;
			while(size<EXT2_BLOCK_SIZE){// going through whole block
				if(size==0 && (direntry->inode == cur_inode)){ // so if its the first block -> dummy node
					direntry->inode=0;
					break;
				}
				else if(direntry->inode==cur_inode){
					unsigned short cur_rec_len=0; cur_rec_len=direntry->rec_len; // get currents rec_len 
					direntry = (struct ext2_dir_entry *)(disk + *parent_block*EXT2_BLOCK_SIZE + (prev_entry - prev_rec_len));
					direntry->rec_len+=cur_rec_len; // add current rec_len to previous ones rec_len 
					break;
				}			
				prev_entry+=direntry->rec_len;// contstanly adds all the rec_lens
				prev_rec_len=direntry->rec_len;
				//add to size so we can increment the pointer to point to next entry!
				size+=direntry->rec_len; 
				direntry = (struct ext2_dir_entry *)(disk + *parent_block*EXT2_BLOCK_SIZE +size);				
			}
		
			//2 and 3 if hard links count==1 then change inode and data bitmap!
			if(inode.i_links_count==1){
				unsigned int * ib = (unsigned int *)(disk+EXT2_BLOCK_SIZE*4);
				//-0 represents what we want the n=10 bit too (its really 11 tho!)
				//inode bitmap
				ib[0] ^= (-0 ^ ib[0]) & (1U << (cur_inode-1));
				//block/data bitmap
				int cur_index=0;
				int inode_i_block= inode.i_block[cur_index];
				unsigned int * bb=(unsigned int *)(disk + EXT2_BLOCK_SIZE*3);

				while(inode_i_block){
					int index_of_bb=(inode_i_block-1)/32;       
					bb[index_of_bb] ^= 1U << (inode_i_block -1) % 32;
					cur_index++;
					inode_i_block= inode.i_block[cur_index];
				}

				time_t rawtime; // setting deletion time
				inode_table[cur_inode-1].i_dtime = time(&rawtime); 
				//increasing free blocks and inodes count!
				sb->s_free_inodes_count+=1; sb->s_free_blocks_count+=inode.i_blocks/2;
				gd->bg_free_inodes_count+=1; gd->bg_free_blocks_count+=inode.i_blocks/2;
			}// end of if 

			//#4 modify links count (hardlinks)
			inode_table[cur_inode-1].i_links_count=inode_table[cur_inode-1].i_links_count -1;

		}// end of else if
		else{ // If we encounter anything other than a file
		fprintf(stderr, "ERROR: %s\n", strerror(EPERM)); 
	  	exit(EPERM);

		}
	}
	else{
		fprintf(stderr, "ERROR: %s\n", strerror(ENOENT)); 
	  	exit(ENOENT);

		}
	free(path); free(parent_block);
	return 0; 

} 