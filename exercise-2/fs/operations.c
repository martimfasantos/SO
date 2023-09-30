#include "operations.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>


pthread_mutex_t mutex;

extern inode_t inode_table[INODE_TABLE_SIZE];
extern int locks_vector[LOCKSVECTOR_SIZE];

/* Given a path, fills pointers with strings for the parent path and child
 * file name
 * Input:
 *  - path: the path to split. ATENTION: the function may alter this parameter
 *  - parent: reference to a char*, to store parent path
 *  - child: reference to a char*, to store child file name
 */
void split_parent_child_from_path(char * path, char ** parent, char ** child) {

	int n_slashes = 0, last_slash_location = 0;
	int len = strlen(path);

	// deal with trailing slash ( a/x vs a/x/ )
	if (path[len-1] == '/') {
		path[len-1] = '\0';
	}

	for (int i=0; i < len; ++i) {
		if (path[i] == '/' && path[i+1] != '\0') {
			last_slash_location = i;
			n_slashes++;
		}
	}

	if (n_slashes == 0) { // root directory
		*parent = "";
		*child = path;
		return;
	}

	path[last_slash_location] = '\0';
	*parent = path;
	*child = path + last_slash_location + 1;

}


/*
 * Initializes tecnicofs and creates root node.
 */
void init_fs() {
	inode_table_init();
	
	/* create root inode */
	int root = inode_create(T_DIRECTORY);
	
	if (root != FS_ROOT) {
		printf("failed to create node for tecnicofs root\n");
		exit(EXIT_FAILURE);
	}
}


/*
 * Destroy tecnicofs and inode table.
 */
void destroy_fs() {
	inode_table_destroy();
}

void init_locks_vector(int locks_vector[]){
	for (int i = 0; i < LOCKSVECTOR_SIZE; i++){
		locks_vector[i] = EMPTY;
	}
}

/*
 * Checks if content of directory is not empty.
 * Input:
 *  - entries: entries of directory
 * Returns: SUCCESS or FAIL
 */

int is_dir_empty(DirEntry *dirEntries) {
	if (dirEntries == NULL) {
		return FAIL;
	}
	for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
		if (dirEntries[i].inumber != FREE_INODE) {
			return FAIL;
		}
	}
	return SUCCESS;
}


/*
 * Looks for node in directory entry from name.
 * Input:
 *  - name: path of node
 *  - entries: entries of directory
 * Returns:
 *  - inumber: found node's inumber
 *  - FAIL: if not found
 */
int lookup_sub_node(char *name, DirEntry *entries) {
	if (entries == NULL) {
		return FAIL;
	}
	for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (entries[i].inumber != FREE_INODE && strcmp(entries[i].name, name) == 0) {
            return entries[i].inumber;
        }
    }
	return FAIL;
}

/**
 * Adds a lock to the lock vector
 */
void add_locksvector(int inumber, int locks_vector[]){

	for (int i = 0; i < LOCKSVECTOR_SIZE; i++){
		if (locks_vector[i] == EMPTY){
			locks_vector[i] = inumber;
			return;
		}
	}

	exit(EXIT_FAILURE);
}

/**
 * Unlocks all locks in the lock vector
 */
void unlock_locksvector(int locks_vector[]){

	for (int i=0; i < LOCKSVECTOR_SIZE; i++){
		if (locks_vector[i] == EMPTY)
			return;

		else if (pthread_rwlock_unlock(&inode_table[locks_vector[i]].rwlock) != SUCCESS){
			printf("lock failed to unlock.\n"); 
            exit(EXIT_FAILURE);
		}
	}
}


/*
 * Creates a new node given a path.
 * Input:
 *  - name: path of node
 *  - nodeType: type of node
 * Returns: SUCCESS or FAIL
 */
int create(char *name, type nodeType){

	int parent_inumber, child_inumber;
	char *parent_name, *child_name, name_copy[MAX_FILE_NAME];
	/* use for copy */
	type pType;
	union Data pdata;
	int locks_vector[LOCKSVECTOR_SIZE];
	init_locks_vector(locks_vector);

	strcpy(name_copy, name);
	split_parent_child_from_path(name_copy, &parent_name, &child_name);

	parent_inumber = lookup(parent_name, locks_vector, MODIFY);

	if (parent_inumber == FAIL) {
		printf("failed to create %s, invalid parent dir %s\n",
		        name, parent_name);
		unlock_locksvector(locks_vector);
		return FAIL;
	}

	inode_get(parent_inumber, &pType, &pdata);

	if(pType != T_DIRECTORY) {
		printf("failed to create %s, parent %s is not a dir\n",
		        name, parent_name);
		unlock_locksvector(locks_vector);
		return FAIL;
	}

	if (lookup_sub_node(child_name, pdata.dirEntries) != FAIL) {
		printf("failed to create %s, already exists in dir %s\n",
		       child_name, parent_name);
		unlock_locksvector(locks_vector);
		return FAIL;
	}

	/* create node and add entry to folder that contains new node */
	child_inumber = inode_create(nodeType);

	if (child_inumber == FAIL) {
		printf("failed to create %s in  %s, couldn't allocate inode\n",
		        child_name, parent_name);
		unlock_locksvector(locks_vector);
		return FAIL;
	}

	if (pthread_rwlock_wrlock(&inode_table[child_inumber].rwlock)!= SUCCESS){
        printf("Error: rwlock in inode number %d failed to lock (rdlock)\n", child_inumber);
		unlock_locksvector(locks_vector);
        return FAIL;
    }

	add_locksvector(child_inumber, locks_vector);

	if (dir_add_entry(parent_inumber, child_inumber, child_name) == FAIL) {
		printf("could not add entry %s in dir %s\n",
		       child_name, parent_name);
		unlock_locksvector(locks_vector);
		return FAIL;
	}

	/* unlock all the locks */
	unlock_locksvector(locks_vector);

	return SUCCESS;
}


/*
 * Deletes a node given a path.
 * Input:
 *  - name: path of node
 * Returns: SUCCESS or FAIL
 */
int delete(char *name){

	int parent_inumber, child_inumber;
	char *parent_name, *child_name, name_copy[MAX_FILE_NAME];
	/* use for copy */
	type pType, cType;
	union Data pdata, cdata;
	int locks_vector[LOCKSVECTOR_SIZE];
	init_locks_vector(locks_vector);

	strcpy(name_copy, name);
	split_parent_child_from_path(name_copy, &parent_name, &child_name);

	parent_inumber = lookup(parent_name, locks_vector, MODIFY);

	if (parent_inumber == FAIL) {
		printf("failed to delete %s, invalid parent dir %s\n",
		        child_name, parent_name);
		return FAIL;
	}

	inode_get(parent_inumber, &pType, &pdata);

	if(pType != T_DIRECTORY) {
		printf("failed to delete %s, parent %s is not a dir\n",
		        child_name, parent_name);
		unlock_locksvector(locks_vector);
		return FAIL;
	}

	child_inumber = lookup_sub_node(child_name, pdata.dirEntries);

	if (child_inumber == FAIL) {
		printf("could not delete %s, does not exist in dir %s\n",
		       name, parent_name);
		unlock_locksvector(locks_vector);
		return FAIL;
	}

	/* lock child inode */
	if (pthread_rwlock_wrlock(&inode_table[child_inumber].rwlock)!= SUCCESS){
        printf("Error: rwlock in inode number %d failed to lock (wrlock)\n", child_inumber);
		unlock_locksvector(locks_vector);
        return FAIL;
    }

	/* add child to the locks vector */
	add_locksvector(child_inumber, locks_vector);

	inode_get(child_inumber, &cType, &cdata);

	if (cType == T_DIRECTORY && is_dir_empty(cdata.dirEntries) == FAIL) {
		printf("could not delete %s: is a directory and not empty\n",
		       name);
		unlock_locksvector(locks_vector);
		return FAIL;
	}

	/* remove entry from folder that contained deleted node */
	if (dir_reset_entry(parent_inumber, child_inumber) == FAIL) {
		printf("failed to delete %s from dir %s\n",
		       child_name, parent_name);
		unlock_locksvector(locks_vector);
		return FAIL;
	}

	if (inode_delete(child_inumber) == FAIL) {
		printf("could not delete inode number %d from dir %s\n",
		       child_inumber, parent_name);
		unlock_locksvector(locks_vector);
		return FAIL;
	}

	/* unlock all the locks */
	unlock_locksvector(locks_vector);

	return SUCCESS;
}


/*
 * Lookup for a given path.
 * Input:
 *  - name: path of node
 * Returns:
 *  inumber: identifier of the i-node, if found
 *     FAIL: otherwise
 */
int lookup(char *name, int locks_vector[], int mode){
	char full_path[MAX_FILE_NAME];
	char delim[] = "/";
	char *saveptr;

	strcpy(full_path, name);

	/* start at root node */
	int current_inumber = FS_ROOT;

	/* use for copy */
	type nType;
	union Data data;

	/* wrlock the root when the path empty (only happens when creating/deleting
	files/directories which have the root as a parent) */
	if (mode == MOVE){
		if (strcmp(full_path, "") == 0){
			if (pthread_rwlock_trywrlock(&inode_table[current_inumber].rwlock) == 0)
				add_locksvector(current_inumber, locks_vector);
		}

		/* rdlock the root */
		else {
			if (pthread_rwlock_tryrdlock(&inode_table[current_inumber].rwlock) == SUCCESS)
				add_locksvector(current_inumber, locks_vector);
		}
			
	}

	else {
		if (strcmp(full_path, "") == 0){
			if (pthread_rwlock_wrlock(&inode_table[current_inumber].rwlock)!= SUCCESS) {
				printf("Error: rwlock in inode number %d failed to lock (wrlock)\n", current_inumber);
				unlock_locksvector(locks_vector);
				return FAIL;
			}
		}

		/* rdlock the root */
		else {
			if (pthread_rwlock_rdlock(&inode_table[current_inumber].rwlock)!= SUCCESS ){
				printf("Error: rwlock in inode number %d failed to lock (rdlock)\n", current_inumber);
				unlock_locksvector(locks_vector);
				return FAIL;
			}
		}
		/* add the root to the locksvector */
		add_locksvector(current_inumber, locks_vector);
	}

	/* get root inode data */
	inode_get(current_inumber, &nType, &data);

	char *path = strtok_r(full_path, delim, &saveptr);

	/* search for all sub nodes */
	while (path != NULL && (current_inumber = lookup_sub_node(path, data.dirEntries)) != FAIL) {
		
		path = strtok_r(NULL, delim, &saveptr);

		if (mode == MOVE){
			if (pthread_rwlock_tryrdlock(&inode_table[current_inumber].rwlock) == SUCCESS){
				add_locksvector(current_inumber, locks_vector);
				inode_get(current_inumber, &nType, &data);
				break;
			}

			if (path == NULL){
				if (pthread_rwlock_trywrlock(&inode_table[current_inumber].rwlock)== SUCCESS){
					add_locksvector(current_inumber, locks_vector);
					break;
				} 
			}

		}

		else if (path == NULL){
			if (pthread_rwlock_wrlock(&inode_table[current_inumber].rwlock)!= SUCCESS){
        		printf("Error: rwlock in inode number %d failed to lock (wrlock)\n", current_inumber);
				unlock_locksvector(locks_vector);
        		return FAIL;
    		} 	 

			add_locksvector(current_inumber, locks_vector);
			break;
		}

		else if (pthread_rwlock_rdlock(&inode_table[current_inumber].rwlock)!= SUCCESS){
        	printf("Error: rwlock in inode number %d failed to lock (rdlock)\n", current_inumber);
			unlock_locksvector(locks_vector);
        	return FAIL;
    	}
		
		add_locksvector(current_inumber, locks_vector);

		inode_get(current_inumber, &nType, &data);

	}

	if (mode == MODIFY || mode == MOVE){
		return current_inumber;
		/* Bug: Sometimes returns FAIL unintencionally */
	}

	else if (mode == FIND){
		unlock_locksvector(locks_vector);
		return current_inumber;
	}

	else {	 /* error */ 
		printf("Error: invalid type not set properly\n");
		return FAIL;
	}
	
}

/*
 * Lookup for a parent and child inumber
 * Input:
 *  - name: path of node 
 *  - locks_vector: vector of locks in use
 * 	-parent_name: path of parent node
 * 	-child_name: path of child node
 * 	-parent_inumber: parent inumber
 * 	-child_inumber: child inumber
 * Returns: 
 *     SUCESS: 
 *     FAIL: if parent dir does not exist or any of the locks fail
 */
int parent_and_child_inumber(char* name, int locks_vector[], char* parent_name, char* child_name, int* parent_inumber, int* child_inumber){

	type pType;
	union Data pdata;

	*parent_inumber = lookup(parent_name, locks_vector, MOVE);

	if (*parent_inumber == FAIL) {
		printf("failed to move %s, invalid parent dir %s\n",
		        name, parent_name);
		unlock_locksvector(locks_vector);
		return FAIL;
	}

	inode_get(*parent_inumber, &pType, &pdata);

	if(pType != T_DIRECTORY ) {
		printf("failed to move %s, parent %s is not a dir\n",
		        name, parent_name);
		unlock_locksvector(locks_vector);
		return FAIL;
	}

	*child_inumber = lookup_sub_node(child_name, pdata.dirEntries);

	/* If child_inumber exists (when getting the child inumber from the first argument
	in the move function) */
	if (*child_inumber != FAIL){
		/* lock child inode */
		if (pthread_rwlock_wrlock(&inode_table[*child_inumber].rwlock)!= SUCCESS){
			printf("Error: rwlock in inode number %d failed to lock (wrlock)\n", *child_inumber);
			unlock_locksvector(locks_vector);
			return FAIL;
		}

		/* add child to the locks vector */
		add_locksvector(*child_inumber, locks_vector);

	}

	return SUCCESS;

}

/*
 * Size of path
 * Input:
 *  -name: path of node
 * Returns: 
 *     length: length of the path
 */
int path_length(char* name){
	char namecopy[MAX_FILE_NAME];
	char delim[] = "/";
	char *saveptr;

	int length = 0;

	strcpy(namecopy, name);

	char *word = strtok_r(name, delim, &saveptr);

	while (word != NULL){
		length++;
		word = strtok_r(NULL, delim, &saveptr);
	}

    return length; 
}

/*
 * Moves a file or directory
 * Input:
 *  - name: path of node 
 *  -newname: new path of the node
 * Returns: SUCESS OR FAIL
 */
int move(char* name, char* newname){

	int parent_inumber_name, child_inumber_name;
	int parent_inumber_newname, child_inumber_newname;
	char *parent_name, *child_name, *parent_newname, *child_newname;
	char name_copy[MAX_FILE_NAME], newname_copy[MAX_FILE_NAME];
	int name_length, new_name_length;

	int locks_vector[LOCKSVECTOR_SIZE];
	init_locks_vector(locks_vector);

	strcpy(name_copy, name);
	split_parent_child_from_path(name_copy, &parent_name, &child_name);
	name_length = path_length(name);

	strcpy(newname_copy, newname);
	split_parent_child_from_path(newname_copy, &parent_newname, &child_newname);
	new_name_length = path_length(newname);


	if ((strcmp(name, newname) < 0 && name_length <= new_name_length) || name_length < new_name_length){

		if (parent_and_child_inumber(name, locks_vector, parent_name, child_name, &parent_inumber_name, &child_inumber_name) == FAIL)
			return FAIL;

		if (child_inumber_name == FAIL) {
			printf("failed to move %s in  %s, does not exist\n",
		        child_name, parent_name);
			unlock_locksvector(locks_vector);
			return FAIL;
		}

		if (parent_and_child_inumber(name, locks_vector, parent_newname, child_newname, &parent_inumber_newname, &child_inumber_newname) == FAIL)
			return FAIL;

		if (child_inumber_newname != FAIL) {
			printf("failed to move %s in  %s, already exists\n",
		        child_newname, parent_newname);
			unlock_locksvector(locks_vector);
			return FAIL;
		}

	}
	else {

		if (parent_and_child_inumber(name, locks_vector, parent_newname, child_newname, &parent_inumber_newname, &child_inumber_newname) == FAIL)
			return FAIL;

		if (child_inumber_newname != FAIL) {
			printf("failed to create %s in  %s, couldn't allocate inode\n",
		        child_newname, parent_newname);
			unlock_locksvector(locks_vector);
			return FAIL;
		}
		
		if (parent_and_child_inumber(name, locks_vector, parent_name, child_name, &parent_inumber_name, &child_inumber_name) == FAIL)
			return FAIL;

		if (child_inumber_name == FAIL) {
			printf("failed to create %s in  %s, couldn't allocate inode\n",
		        child_name, parent_name);
			unlock_locksvector(locks_vector);
			return FAIL;
		}
	}

	char child_namecopy[MAX_FILE_NAME] = "/";
	strcat(child_namecopy, child_name);
	
	/* Verifify the directory which is being moved is not its new parent 
	(child name com come with an extra '/' depending on the input given) */
	if (strcmp(child_namecopy, parent_newname) == 0 || strcmp(child_name, parent_newname) == 0){
		printf("failed to move %s from dir %s\n",
			child_name, parent_name);
		unlock_locksvector(locks_vector);
		return FAIL;
	}

	/* remove entry from parent (old directory) */
	if (dir_reset_entry(parent_inumber_name, child_inumber_name) == FAIL) {
		printf("failed to delete %s from dir %s\n",
			child_name, parent_name);
		unlock_locksvector(locks_vector);
		return FAIL;
	}

	/* add to the new parent (new directory) */
	if (dir_add_entry(parent_inumber_newname, child_inumber_name, child_newname) == FAIL) {
		printf("could not add entry %s in dir %s\n",
			child_name, parent_name);
		unlock_locksvector(locks_vector);
		return FAIL;
	}
	

	/* unlock all the locks */
	unlock_locksvector(locks_vector);

	return SUCCESS;

}

/*
 * Prints tecnicofs tree.
 * Input:
 *  - fp: pointer to output file
 */
void print_tecnicofs_tree(FILE *fp){
	inode_print_tree(fp, FS_ROOT, "");
}
