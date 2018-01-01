#ifndef EXT2_UTILS_H
#define EXT2_UTILS_H

#include "ext2.h"

void remove_trailing_slash(char * path);
char ** all_names(char *path, int * total_length);
unsigned int pathTraversal(char *path, unsigned char *disk, unsigned int *parent_block);
int file_in_gap(char * directory_path,char *file_namee, int*file_in_dir_blockk,int *file_offsett, int * file_inodee, int * entry_offsett);
int set_avail_db();
int set_avail_ib();
void create_inode(int avail_ib,unsigned short mode,unsigned int size,unsigned short link_count,unsigned short blocks,unsigned int *block_pointers);
void create_entry(int dbaddr,unsigned int inode, unsigned short rec_len, unsigned char name_len, unsigned char file_type, char* name);
int valid(int cur_inode);

extern unsigned char *disk;
extern struct ext2_inode *inode_table;
extern struct ext2_super_block *sb;
extern struct ext2_group_desc *gd;

#endif