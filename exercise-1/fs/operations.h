#ifndef FS_H
#define FS_H
#include "state.h"

#define NOSYNC 0
#define MUTEX 1
#define RWLOCK 2

#define READONLY 3
#define READWRITE 4

void init_fs();
void destroy_fs();
int is_dir_empty(DirEntry *dirEntries);
int create(char *name, type nodeType);
int delete(char *name);
int lookup(char *name);
void getSynch(char* input);
void lock(int synchstrategy, int rwlocktype);
void unlock(int locktype);
void destroylocks(int synchstrategy);
void print_tecnicofs_tree(FILE *fp);

#endif /* FS_H */
