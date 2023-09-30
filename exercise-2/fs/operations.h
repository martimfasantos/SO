#ifndef FS_H
#define FS_H
#include "state.h"

#define EMPTY -1

#define NOSYNC 0
#define MUTEX 1
#define RWLOCK 2

#define READONLY 3
#define READWRITE 4

#define MODIFY 5
#define FIND 6
#define MOVE 7


void init_fs();
void destroy_fs();
void init_locks_vector(int locks_vector[]);
int is_dir_empty(DirEntry *dirEntries);
void add_locksvector(int inumber, int locks_vetor[]);
void unlock_locksvector(int locks_vector[]);
int create(char *name, type nodeType);
int delete(char *name);
int lookup(char *name, int locks_vector[], int mode);
int count_path(char**  words, char* name);
char* sort_names(char* name1, char* name2);
int parent_and_child_inumber(char* name, int locks_vector[], char* parent_name, 
                char* child_name, int* parent_inumber, int* child_inumber);
int move(char* name, char* newname);
void getSynch(char* input);
void lock(int synchstrategy, int rwlocktype);
void unlock(int locktype);
void destroylocks(int synchstrategy);
void print_tecnicofs_tree(FILE *fp);

#endif /* FS_H */
