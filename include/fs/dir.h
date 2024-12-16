#ifndef __DIR_H__
#define __DIR_H__

#include "common.h"
#include "inode.h"

#define DIR_NAME_LEN 30

typedef struct dirent {
    uint16 inode_num;
    char name[DIR_NAME_LEN];
} dirent_t;

uint16 dir_search_entry(inode_t* pip, char* name);
uint32 dir_add_entry(inode_t* pip, uint16 inode_num, char* name);
uint16 dir_delete_entry(inode_t* pip, char* name);
void   dir_print(inode_t* pip);

inode_t* path_to_inode(char* path);
inode_t* path_to_pinode(char* path, char* name);

inode_t* path_create_inode(char* path, uint16 type, uint16 major, uint16 minor); 
uint32   path_link(char* old_path, char* new_path); 
uint32   path_unlink(char* path); 


#endif
// TODO!
