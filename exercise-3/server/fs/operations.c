#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include "operations.h"
#include "../../tecnicofs-api-constants.h"

extern inode_t inode_table[INODE_TABLE_SIZE];
extern pthread_mutex_t mutex;
extern int numberThreads;

int sockfd;
struct sockaddr_un server_addr;
socklen_t addrlen;
char *path;


int setSockAddrUn(char *path, struct sockaddr_un *addr) {

  if (addr == NULL)
    return 0;

  bzero((char *)addr, sizeof(struct sockaddr_un));
  addr->sun_family = AF_UNIX;
  strcpy(addr->sun_path, path);

  return SUN_LEN(addr);
}

/*
 * Given a path, mounts the socket and binds it to the
 * server address.
 * Input:
 * 	- sockPath: the server address path
 * Returns: SUCESS or exits with failure
 */
int tfsMount(char *sockPath) {

	if ((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
    	perror("server: can't open socket");
        exit(EXIT_FAILURE);
    }

   	path = sockPath;

	unlink(path);

	addrlen = setSockAddrUn(sockPath, &server_addr);

	if (bind(sockfd, (struct sockaddr *) &server_addr, addrlen) < 0) {
		perror("server: bind error");
		exit(EXIT_FAILURE);
	}
	
	return SUCCESS;
}

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

	strcpy(name_copy, name);
	split_parent_child_from_path(name_copy, &parent_name, &child_name);

	parent_inumber = lookup(parent_name);

	if (parent_inumber == FAIL) {
		printf("failed to create %s, invalid parent dir %s\n",
		        name, parent_name);
		return FAIL;
	}

	inode_get(parent_inumber, &pType, &pdata);

	if(pType != T_DIRECTORY) {
		printf("failed to create %s, parent %s is not a dir\n",
		        name, parent_name);
		return FAIL;
	}

	if (lookup_sub_node(child_name, pdata.dirEntries) != FAIL) {
		printf("failed to create %s, already exists in dir %s\n",
		       child_name, parent_name);
		return FAIL;
	}

	/* create node and add entry to folder that contains new node */
	child_inumber = inode_create(nodeType);

	if (child_inumber == FAIL) {
		printf("failed to create %s in  %s, couldn't allocate inode\n",
		        child_name, parent_name);
		return FAIL;
	}

	if (dir_add_entry(parent_inumber, child_inumber, child_name) == FAIL) {
		printf("could not add entry %s in dir %s\n",
		       child_name, parent_name);
		return FAIL;
	}


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

	strcpy(name_copy, name);
	split_parent_child_from_path(name_copy, &parent_name, &child_name);

	parent_inumber = lookup(parent_name);

	if (parent_inumber == FAIL) {
		printf("failed to delete %s, invalid parent dir %s\n",
		        child_name, parent_name);
		return FAIL;
	}

	inode_get(parent_inumber, &pType, &pdata);

	if(pType != T_DIRECTORY) {
		printf("failed to delete %s, parent %s is not a dir\n",
		        child_name, parent_name);
		return FAIL;
	}

	child_inumber = lookup_sub_node(child_name, pdata.dirEntries);

	if (child_inumber == FAIL) {
		printf("could not delete %s, does not exist in dir %s\n",
		       name, parent_name);
		return FAIL;
	}

	inode_get(child_inumber, &cType, &cdata);

	if (cType == T_DIRECTORY && is_dir_empty(cdata.dirEntries) == FAIL) {
		printf("could not delete %s: is a directory and not empty\n",
		       name);
		return FAIL;
	}

	/* remove entry from folder that contained deleted node */
	if (dir_reset_entry(parent_inumber, child_inumber) == FAIL) {
		printf("failed to delete %s from dir %s\n",
		       child_name, parent_name);
		return FAIL;
	}

	if (inode_delete(child_inumber) == FAIL) {
		printf("could not delete inode number %d from dir %s\n",
		       child_inumber, parent_name);
		return FAIL;
	}

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
int lookup(char *name){
	char full_path[MAX_FILE_NAME];
	char delim[] = "/";
	char *saveptr;

	strcpy(full_path, name);

	/* start at root node */
	int current_inumber = FS_ROOT;

	/* use for copy */
	type nType;
	union Data data;

	/* get root inode data */
	inode_get(current_inumber, &nType, &data);

	char *path = strtok_r(full_path, delim, &saveptr);

	/* search for all sub nodes */
	while (path != NULL && (current_inumber = lookup_sub_node(path, data.dirEntries)) != FAIL) {
		
		inode_get(current_inumber, &nType, &data);
		path = strtok_r(NULL, delim, &saveptr);

	}

	return current_inumber;
	
}

/*
 * Lookup for a parent and child inumber
 * Input:
 *  - name: path of node 
 *  - locks_vector: vector of locks in use
 * 	- parent_name: path of parent node
 * 	- child_name: path of child node
 * 	- parent_inumber: parent inumber
 * 	- child_inumber: child inumber
 * Returns: 
 *     SUCESS: 
 *     FAIL: if parent dir does not exist or any of the locks fail
 */
int parent_and_child_inumber(char* name, char* parent_name, char* child_name, int* parent_inumber, int* child_inumber){

	type pType;
	union Data pdata;

	*parent_inumber = lookup(parent_name);

	if (*parent_inumber == FAIL) {
		printf("failed to move %s, invalid parent dir %s\n",
		        name, parent_name);
		return FAIL;
	}

	inode_get(*parent_inumber, &pType, &pdata);

	if(pType != T_DIRECTORY ) {
		printf("failed to move %s, parent %s is not a dir\n",
		        name, parent_name);
		return FAIL;
	}

	*child_inumber = lookup_sub_node(child_name, pdata.dirEntries);

	return SUCCESS;

}

/*
 * Size of path
 * Input:
 *  - name: path of node
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
 *  - newname: new path of the node
 * Returns: SUCESS OR FAIL
 */
int move(char* name, char* newname){

	int parent_inumber_name, child_inumber_name;
	int parent_inumber_newname, child_inumber_newname;
	char *parent_name, *child_name, *parent_newname, *child_newname;
	char name_copy[MAX_FILE_NAME], newname_copy[MAX_FILE_NAME];
	int name_length, new_name_length;

	strcpy(name_copy, name);
	split_parent_child_from_path(name_copy, &parent_name, &child_name);
	name_length = path_length(name);

	strcpy(newname_copy, newname);
	split_parent_child_from_path(newname_copy, &parent_newname, &child_newname);
	new_name_length = path_length(newname);


	if ((strcmp(name, newname) < 0 && name_length <= new_name_length) || name_length < new_name_length){

		if (parent_and_child_inumber(name, parent_name, child_name, &parent_inumber_name, &child_inumber_name) == FAIL)
			return FAIL;

		if (child_inumber_name == FAIL) {
			printf("failed to move %s in  %s, does not exist\n",
		        child_name, parent_name);
			return FAIL;
		}

		if (parent_and_child_inumber(name, parent_newname, child_newname, &parent_inumber_newname, &child_inumber_newname) == FAIL)
			return FAIL;

		if (child_inumber_newname != FAIL) {
			printf("failed to move %s in  %s, already exists\n",
		        child_newname, parent_newname);
			return FAIL;
		}

	}
	else {

		if (parent_and_child_inumber(name, parent_newname, child_newname, &parent_inumber_newname, &child_inumber_newname) == FAIL)
			return FAIL;

		if (child_inumber_newname != FAIL) {
			printf("failed to create %s in  %s, couldn't allocate inode\n",
		        child_newname, parent_newname);
			return FAIL;
		}
		
		if (parent_and_child_inumber(name, parent_name, child_name, &parent_inumber_name, &child_inumber_name) == FAIL)
			return FAIL;

		if (child_inumber_name == FAIL) {
			printf("failed to create %s in  %s, couldn't allocate inode\n",
		        child_name, parent_name);
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
		return FAIL;
	}

	/* remove entry from parent (old directory) */
	if (dir_reset_entry(parent_inumber_name, child_inumber_name) == FAIL) {
		printf("failed to delete %s from dir %s\n",
			child_name, parent_name);
		return FAIL;
	}

	/* add to the new parent (new directory) */
	if (dir_add_entry(parent_inumber_newname, child_inumber_name, child_newname) == FAIL) {
		printf("could not add entry %s in dir %s\n",
			child_name, parent_name);
		/* add entry  to the old directory again */
		if (dir_add_entry(parent_inumber_name, child_inumber_name, child_name) == FAIL)
			printf("entry %s was lost during the proccess\n",child_name);
		
		return FAIL;
	}

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

/*
 * Prints de tecnicofs tree to a file.
 * Input:
 * 	- filename: name of the outputfile
 */ 
int print(char *filename){

	FILE *outputfile;
	outputfile = fopen(filename, "w");

	if(outputfile == NULL)
		return FAIL;

	/* we would rwlock the root to garantee that no other thread is modifing the tecnicofs
	but since we changed the locks to a global mutex lock because of someone bugs in the
	last version of the code, the property is already garanteed just by the global mutex */
	
	print_tecnicofs_tree(outputfile);

	fclose(outputfile);

	return SUCCESS;

}
