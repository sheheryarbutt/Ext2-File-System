#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <time.h>
#include <libgen.h>
#include "ext2_utils.h"

/*Removes trailing slash from an absolute path */
void remove_trailing_slash(char * path){
	char* absPath=path;
	char last_element[2]; last_element[1]='\0'; 
	last_element[0]=absPath[(unsigned int)strlen(absPath)-1];
	 if (strcmp(last_element, "/")==0 ){ // removes "/" from the end
    	int path_length=(unsigned int)strlen(absPath);
    	char temp_new_path[path_length]; strcpy(temp_new_path,absPath);
    	temp_new_path[path_length-1]='\0'; strcpy(absPath,temp_new_path);    
    }
    return;
}

int valid(int cur_inode){
	if( cur_inode==2 || (cur_inode>11 && cur_inode<=32)){
		return 1;
	}
	return 0;
}

/* returns an array of strings containing all the directory/files from an
absolute path so /home/alice/lru.c returns ["home", "alice","lru.c"]  */
char ** all_names(char *path, int * total_length){
	char *absPath=malloc((unsigned int)strlen(path) + 1);
	strcpy(absPath,path);
	char **names=malloc(256* sizeof(char*));
	remove_trailing_slash(absPath);

	int num_names=0;
	while((unsigned int)strlen(absPath)){
		unsigned int path_len=(unsigned int)strlen(absPath);
		char * temp_name=basename(absPath);
		unsigned int temp_name_len=(unsigned int) strlen(temp_name);
		names[num_names]=malloc(temp_name_len); strcpy(names[num_names],temp_name);
		absPath[path_len-(temp_name_len+1)]='\0';
		num_names++;
	}
	 *total_length=num_names;
	return names;
}
 
/* Returns the inode of a file or directory for an absolute path*/
unsigned int pathTraversal(char *path, unsigned char *disk, unsigned int *parent_block){
	char *absPath=path; unsigned int * temp_parent_block=parent_block;

	int cur_inode=EXT2_ROOT_INO;//start off with root node 2!
	struct ext2_inode *inode_table = (struct ext2_inode *)(disk + EXT2_BLOCK_SIZE*5);//pointer to first inode!
	struct ext2_inode inode = inode_table[cur_inode-1];// this gets the root node!

	if((unsigned int) strlen(absPath)==1 && strcmp(absPath,"/")==0){
		*temp_parent_block=inode.i_block[0];
		return 2; }
	int *path_length=malloc(sizeof(int));
    *path_length=0;
	char ** path_names=all_names(absPath,path_length);

	int size=0;
	int valid_path=0;

	 for(int i=*path_length; i>0;i--){
	 	char * cur_name= path_names[i-1]; 
	 	int found_or_not=0;
	 	int current_i_block_index=0;
		int current_i_block=inode.i_block[current_i_block_index];
		while (current_i_block){
			size=0;
			struct ext2_dir_entry *direntry = (struct ext2_dir_entry *)(disk + current_i_block*EXT2_BLOCK_SIZE +size); //first directory!

		 	while(size<EXT2_BLOCK_SIZE){
				char name [direntry->name_len+1]; strncpy(name,direntry->name,direntry->name_len);
				name[direntry->name_len]='\0';

				if(strcmp(name,cur_name)==0){//if the names match! file/directory found!
					if(i>1 && direntry->file_type & EXT2_FT_DIR){
						found_or_not=1;
						cur_inode=(int)direntry->inode;	//update cur_inode to new one!
						inode=inode_table[cur_inode-1];
						direntry=(struct ext2_dir_entry *)(disk + current_i_block*EXT2_BLOCK_SIZE +size);
						size=EXT2_BLOCK_SIZE+1; // to braek out of inner loop
					}
					else if (i==1){// end of path so it can be dir or file!
						*temp_parent_block=current_i_block;
						found_or_not=1;
						cur_inode=(int)direntry->inode;
						valid_path=1;
						size=EXT2_BLOCK_SIZE+1;
					}
				}
				size+=direntry->rec_len;
				direntry = (struct ext2_dir_entry *)(disk + current_i_block*EXT2_BLOCK_SIZE +size);
			}// end of inner while
			if(found_or_not){
				current_i_block=0; 
			}
			else{
				//since we havent found what we want, it may be in next block
				current_i_block_index++;
				current_i_block=inode.i_block[current_i_block_index];
			}
		}//end of outer while
		if(found_or_not==0){ //since we didnt find the current file/dir, break out of for loop
			valid_path=0;
			break; 
		}
	 }// end of for loop 
	 if(valid_path){
			return cur_inode;
		}
		else {
			return 0;
		}

	return 0;
}
int set_avail_db(){

	unsigned int * bb=(unsigned int *)(disk + (EXT2_BLOCK_SIZE*3));			   

	int i;
	 //4= 128/32 because 128 is the total blocks and 32 bits is 4 bytes=unsigned int
	for (i=0; i<4; i++){
		int j=0;	
		while(j<32){
			
			if (!(bb[i]&(1<<j))){
				// updating to notify data block in now in use
				bb[i]|=(1<<j);
				sb->s_free_blocks_count-=1;
				gd->bg_free_blocks_count-=1;
				return (32*i)+(j+1);
			}
			
			j++;
		}
		
	}
	return -1;					   
}
int set_avail_ib(){

	unsigned int * ib = (unsigned int *)(disk+(EXT2_BLOCK_SIZE*4));
	int j=EXT2_GOOD_OLD_FIRST_INO-1;
	while(j<32){
			if (!(ib[0]&(1<<j))){
				// updating to notify inode in now in use
				ib[0]|=1<<j;
				sb->s_free_inodes_count-=1;
				gd->bg_free_inodes_count-=1;
				return j+1;
			}
			
			j++;
	}
	return -1;
}
void create_inode(int avail_ib,unsigned short mode,unsigned int size,unsigned short link_count,unsigned short blocks,unsigned int *block_pointers){
	time_t rawtime; // for setting creation time
	struct ext2_inode inode;
	inode.i_mode=mode;        
	inode.i_uid=0;        
	inode.i_size=size;     
	inode.i_ctime=(unsigned int)(time( &rawtime ));       /* Creation time */
	inode.i_dtime=0;
	inode.i_gid=0;        
	inode.i_links_count=link_count; 
	inode.i_blocks=blocks;    
	inode.osd1=0;        
	for (int i=0;i<15;i++){
		inode.i_block[i]=block_pointers[i];
	}     /* Pointers to blocks */
	free(block_pointers);

	inode.i_generation=0;  
	inode.i_file_acl=0;    
	inode.i_dir_acl=0;    
	inode.i_faddr=0;       
	inode_table[avail_ib-1]=inode;
	if ((mode & EXT2_S_IFDIR) == EXT2_S_IFDIR){
		gd->bg_used_dirs_count+=1;
	}
}

void create_entry(int dbaddr,unsigned int inode, unsigned short rec_len, unsigned char name_len, unsigned char file_type, char* name){
	//store entry at dbaddr= data block address
	struct ext2_dir_entry* new_entry = (struct ext2_dir_entry *)(disk+dbaddr); 
	new_entry->inode=inode;
	new_entry->rec_len=rec_len;
	new_entry->name_len=name_len;
	new_entry->file_type=file_type;
	// store name after dir entry struct
	memcpy(disk+dbaddr + sizeof(struct ext2_dir_entry), name, name_len); 
}