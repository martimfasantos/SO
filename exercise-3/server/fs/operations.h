#ifndef FS_H
#define FS_H
#include "state.h"

#define EMPTY -1

#define ROOTPATH ""

#define MODIFY 5
#define FIND 6
#define MOVE 7

int setSockAddrUn(char *path, struct sockaddr_un *addr);
int tfsMount(char *sockPath);
void init_fs();
void destroy_fs();
int is_dir_empty(DirEntry *dirEntries);
int create(char *name, type nodeType);
int delete(char *name);
int lookup(char *name);
int count_path(char**  words, char* name);
char* sort_names(char* name1, char* name2);
int parent_and_child_inumber(char* name, char* parent_name, 
                char* child_name, int* parent_inumber, int* child_inumber);
int move(char* name, char* newname);
void print_tecnicofs_tree(FILE *fp);
int print(char* outputfile);

#endif /* FS_H */
