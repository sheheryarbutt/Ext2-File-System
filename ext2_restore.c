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

int file_in_gap(char * directory_path,char *file_namee, int*file_in_dir_blockk,int *file_offsett, int * file_inodee, int * entry_offsett);

int main(int argc, char **argv) {
	if(argc != 3) { 
        fprintf(stderr, "Usage: ext2_restore <image file name> <Absolute path>\n");
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



	//This part gives us directory_path and file_name
	char *absPath=malloc((unsigned int)strlen(path)); strcpy(absPath,path);
	if((unsigned int)strlen(absPath)==1 && strcmp(absPath,"/")==0){
		fprintf(stderr, "ERROR: %s\n", strerror(EISDIR)); 
		exit(EISDIR);
	}
	remove_trailing_slash(absPath); // remove the trailing slashes
	char * file_name=basename(absPath); unsigned int file_name_length= (unsigned int)strlen(file_name);
	unsigned int absPath_length=(unsigned int) strlen(absPath);
	absPath[absPath_length - (file_name_length + 1)]='\0';
	char directory_path[absPath_length - file_name_length]; strcpy(directory_path,absPath);
	
	if((unsigned int)strlen(directory_path) == 0){ // if only "/file" is given 
		strcpy(directory_path,"/");
	}

	int * file_in_dir_block=malloc(sizeof(int *)); *file_in_dir_block=-1;
	int * file_offset=malloc(sizeof(int *));  *file_offset=-1; 
	int * entry_offset=malloc(sizeof(int *));  *entry_offset=-1;
	int * file_inode=malloc(sizeof(int *)); *file_inode=0;
	int in_gap=file_in_gap(directory_path,file_name,file_in_dir_block, file_offset, file_inode,entry_offset);

	if(in_gap){//maybe we found the file
		if(*file_inode>EXT2_GOOD_OLD_FIRST_INO || *file_inode<=32){ 

				unsigned int * ib = (unsigned int *)(disk+EXT2_BLOCK_SIZE*4);
				// -0 represents what we want the n=10 bit too (its really 11 tho!)
				if ( ib[0]&(1<< ((*file_inode) - 1 ))){// check if its a 1 or 0
					fprintf(stderr, "ERROR: %s\n", strerror(EPERM)); 
					exit(EPERM); 
				}
				//block/data bitmap
				struct ext2_inode inode = inode_table[*file_inode-1];//access the specific inode!
				int cur_index=0;
				int inode_i_block= inode.i_block[cur_index];// accessing the first i_block[0]= some block #
				unsigned int * bb=(unsigned int *)(disk + EXT2_BLOCK_SIZE*3);//accessing the data bitmap

				while(inode_i_block){// going through all the data blocks gotten  from i_block[] of the inode!
					int index_of_bb=(inode_i_block-1)/32;// gonna be 0,1,2 or 3  
					// If data block is replaced -> return error
					if(bb[index_of_bb]& (1<< (inode_i_block -1) % 32)){
						fprintf(stderr, "ERROR: %s\n", strerror(EPERM)); 
						exit(EPERM); 
					}
					cur_index++;
					inode_i_block= inode.i_block[cur_index];
				}// After this while we know all data blocks havent been reused!

				ib[0] ^= (-1 ^ ib[0]) & (1U << (*file_inode-1)); // change inode bitmap val. to 1
	
				cur_index=0; inode_i_block= inode.i_block[cur_index];
				while(inode_i_block){// going through all the data blocks 
					int index_of_bb=(inode_i_block-1)/32;// gonna be 0,1,2 or 3  
					bb[index_of_bb] ^= 1U << (inode_i_block -1) % 32;
					cur_index++;
					inode_i_block= inode.i_block[cur_index];
				}
				//decreasing number of free inodes and blocks
				sb->s_free_inodes_count-=1;  sb->s_free_blocks_count-=inode.i_blocks/2;
				gd->bg_free_inodes_count-=1; gd->bg_free_blocks_count-=inode.i_blocks/2;
				//changing links count and d_time
				 inode_table[*file_inode-1].i_links_count=1;  inode_table[*file_inode-1].i_dtime=0;
				//Updating rec_len of the enteries!
				struct ext2_dir_entry *direntry = (struct ext2_dir_entry *)(disk + (*file_in_dir_block)*EXT2_BLOCK_SIZE + (*entry_offset));
				struct ext2_dir_entry *gapEntry = (struct ext2_dir_entry *)(disk + (*file_in_dir_block)*EXT2_BLOCK_SIZE + (*file_offset));
				int entry_rec_len= direntry->rec_len;
				direntry->rec_len=(*file_offset)-(*entry_offset);
				gapEntry->rec_len=entry_rec_len - (direntry->rec_len);

		}
		else{
			fprintf(stderr, "ERROR: %s\n", strerror(EPERM)); 
			exit(EPERM); 
		}
	}
	else{// no such file found in gaps!
		fprintf(stderr, "ERROR: %s\n", strerror(EPERM)); 
		exit(EPERM); 
	}
	free(file_in_dir_block); free(file_offset);
	free(entry_offset); free(file_inode);

}

 	/* Returns a 1 if we found a file in some gap, 0 otherwise*/
int file_in_gap(char * directory_path,char *file_namee, int*file_in_dir_blockk,int *file_offsett, int * file_inodee, int * entry_offsett){

	unsigned int * parent_block= malloc(sizeof(unsigned int *));
	int cur_inode = pathTraversal(directory_path, disk, parent_block); //get the inode of parent directory

	char *file_name=malloc((unsigned int) strlen(file_namee));strcpy(file_name,file_namee);
	int file_found=0; int *file_inode=file_inodee;
	int *file_in_dir_block=file_in_dir_blockk;
	int *entry_offset=entry_offsett;// offset of the entry that has the gap!
	int *file_offset=file_offsett;// when file is found what's the offset

	if(cur_inode>EXT2_GOOD_OLD_FIRST_INO || cur_inode==2){
		struct ext2_inode inode = inode_table[cur_inode-1];// represents the parent directories inode struct
		if((inode.i_mode & EXT2_S_IFDIR) == EXT2_S_IFDIR){//file containing the removed fill is a directory!

			int i=0; // represents the block # in which directory entries are
			while(inode.i_block[i]){//loop through all directory enteries blocks essentially!
				int size=0;
				struct ext2_dir_entry *direntry = (struct ext2_dir_entry *)(disk + inode.i_block[i]*EXT2_BLOCK_SIZE +size);// first i_block[0] = some # 
				int current_rec_len; int not_in_gap=1; //1 for not in gap, 0 for in gap!
				int offset_of_next_entry=0; // represents the offset orignally (with gaps)
				//int size_of_current_entry=0; // represents the size orignally (with gaps)
				int left_over_size; // size still left over
				while(size<EXT2_BLOCK_SIZE){// going through whole block
					char name2 [direntry->name_len+1]; strcpy(name2,direntry->name); name2[direntry->name_len]='\0';
					if(not_in_gap){//When im not in the gaps, update these values!
						*entry_offset=size;
						offset_of_next_entry+=direntry->rec_len; 
						left_over_size=direntry->rec_len; 
					}
					current_rec_len=direntry->rec_len;

					int real_rec_len; // represents the real rec_len (w/o gaps) 
					if(direntry->name_len % 4 == 0){ // if name_len is a multiple of 4
						real_rec_len=direntry->name_len + 8; // dont need to add padding
					}else {
						real_rec_len=direntry->name_len + 12 - (direntry->name_len % 4);
					}

					if(i>0 && size==0 && direntry->inode==0){
						direntry=(struct ext2_dir_entry *)(disk +  inode.i_block[i]*EXT2_BLOCK_SIZE +size); //update direntry +44+16 (instead of 44+36)
						char name [direntry->name_len+1]; strcpy(name,direntry->name); name[direntry->name_len]='\0';
						if(strcmp(name,file_name)==0){// means we found a gap with same name! 
							break; 
						}
					} 
					// if the current is greater then what it's suppose to be, clearly GAP!
					if(left_over_size>real_rec_len){
						left_over_size-=real_rec_len; 
						not_in_gap=0; // change to 0 because we found a gap  
						//meaning we are getting a corrupted file/no more gaps
						if(left_over_size<=0 || current_rec_len==0 || current_rec_len>EXT2_BLOCK_SIZE){
							size=offset_of_next_entry;// get out of gaps and look at the next directory entry
							not_in_gap=1;
						}
						direntry=(struct ext2_dir_entry *)(disk +  inode.i_block[i]*EXT2_BLOCK_SIZE +size+ real_rec_len); 
						char name [direntry->name_len+1]; strcpy(name,direntry->name); name[direntry->name_len]='\0';
						if(strcmp(name,file_name)==0 && not_in_gap==0){// means we found a gap with same name! 
							file_found=1; 
							size+=real_rec_len; //now size represents where gap was found!
							*file_offset=size; *file_in_dir_block=inode.i_block[i];
							*file_inode=direntry->inode;
							break; 
						}
						else {
							size+=real_rec_len; 
						}		
				    }//end of if (for being inside the gap!)
				    else{
				    	size+=real_rec_len;
				    	direntry = (struct ext2_dir_entry *)(disk + inode.i_block[i]*EXT2_BLOCK_SIZE +size);
				    	if(not_in_gap==0){
					    	direntry=(struct ext2_dir_entry *)(disk +  inode.i_block[i]*EXT2_BLOCK_SIZE +size); 
							char name2 [direntry->name_len+1]; strcpy(name2,direntry->name); name2[direntry->name_len]='\0';
							if(strcmp(name2,file_name)==0){// means we found a gap with same name! 
								file_found=1; 
								size+=real_rec_len; //now size represents where gap was found!
								*file_offset=size; *file_in_dir_block=inode.i_block[i];
								*file_inode=direntry->inode;
								break; 
							}
				    	}
				    }			   
					if(size==offset_of_next_entry){
						not_in_gap=1;
					}
				}// end of while loop (iterating in a block)			
				if (file_found){
					*file_offset=size;
					*file_in_dir_block=inode.i_block[i];
					*file_inode=direntry->inode;
					break; // break out of outer loop!
				}
				else{
					i++;// update block counter to next!	
				}

			}//end of while loop iterating over all blocks
		}// end of if (checking if its a directory)
		else {//not a directory
			fprintf(stderr, "ERROR: %s\n", strerror(ENOTDIR)); 
			exit(ENOTDIR);
		}
	}//end of it (checking inode of directory>11 || ==2)!

	return file_found;
}