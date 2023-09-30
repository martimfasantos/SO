#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include "fs/operations.h"

#define MAX_COMMANDS 150000
#define MAX_INPUT_SIZE 100

int numberThreads = 0;
extern int synchstrategy;

extern pthread_mutex_t mutex_vetor;

extern pthread_mutex_t mutex;
extern pthread_rwlock_t rwlock;

char inputCommands[MAX_COMMANDS][MAX_INPUT_SIZE];
int numberCommands = 0;
int headQueue = 0;

int insertCommand(char* data) {
    if(numberCommands != MAX_COMMANDS) {
        strcpy(inputCommands[numberCommands++], data);
        return 1;
    }
    return 0;
}

char* removeCommand() {
    if(numberCommands > 0){
        numberCommands--;
        return inputCommands[headQueue++];  
    }
    return NULL;
}

void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
    exit(EXIT_FAILURE);
}

void processInput( FILE *input ){
    char line[MAX_INPUT_SIZE];

    /* break loop with ^Z or ^D */
    while (fgets(line, sizeof(line)/sizeof(char), input)) {
        char token, type;
        char name[MAX_INPUT_SIZE];

        int numTokens = sscanf(line, "%c %s %c", &token, name, &type);

        /* perform minimal validation */
        if (numTokens < 1) {
            continue;
        }
        switch (token) {
            case 'c':
                if(numTokens != 3)
                    errorParse();
                if(insertCommand(line))
                    break;
                return;
            
            case 'l':
                if(numTokens != 2)
                    errorParse();
                if(insertCommand(line))
                    break;
                return;
            
            case 'd':
                if(numTokens != 2)
                    errorParse();
                if(insertCommand(line))
                    break;
                return;
            
            case '#':
                break;
            
            default: { /* error */
                errorParse();
            }
        }
    }
}

void *applyCommands(){

    while (numberCommands > 0){
        if (synchstrategy != NOSYNC){
            pthread_mutex_lock(&mutex_vetor);
        }
        
        const char* command = removeCommand();

        if (synchstrategy != NOSYNC){
            pthread_mutex_unlock(&mutex_vetor);
        }

        if (command == NULL){
            continue;
        }

        char token, type;
        char name[MAX_INPUT_SIZE];
        int numTokens = sscanf(command, "%c %s %c", &token, name, &type);
        if (numTokens < 2) {
            fprintf(stderr, "Error: invalid command in Queue\n");
            exit(EXIT_FAILURE);
        }

        int searchResult;
        switch (token) {
            case 'c':
                switch (type) {
                    case 'f':
                        lock(synchstrategy, READWRITE);
                        printf("Create file: %s\n", name);
                        create(name, T_FILE);
                        unlock(synchstrategy);
                        break;
                    case 'd':
                        lock(synchstrategy, READWRITE);
                        printf("Create directory: %s\n", name);
                        create(name, T_DIRECTORY);
                        unlock(synchstrategy);
                        break;
                    default: {
                        fprintf(stderr, "Error: invalid node type\n");
                        exit(EXIT_FAILURE);
                    }
                }
                break;
            case 'l': 
                lock(synchstrategy, READONLY);
                searchResult = lookup(name);
                if (searchResult >= 0)
                    printf("Search: %s found\n", name);
                else
                    printf("Search: %s not found\n", name);
                unlock(synchstrategy);
                break;
            case 'd':
                lock(synchstrategy, READWRITE);
                printf("Delete: %s\n", name);
                delete(name);
                unlock(synchstrategy);
                break;
            default: { /* error */
                fprintf(stderr, "Error: command to apply\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    return NULL;
}

int main(int argc, char* argv[]){

    /* start the clock */
    struct timeval start, stop;
    gettimeofday(&start, NULL);

    /* verify the number of arguments */
    if (argc != 5){
        printf("Number of arguments must be 5.\n");
        exit(EXIT_FAILURE);
    }

    int i;

    /* set the synchstrategy */
    getSynch(argv[4]);

    /* set the number of threads based on the input */
    numberThreads = atoi(argv[3]);

    /* nosync can only have one thread */
    if (synchstrategy == NOSYNC && numberThreads != 1){
        fprintf(stderr, "Error: more than one thread with nosync\n");
        exit(EXIT_FAILURE);
    }

    pthread_t tid[numberThreads];

    /* init filesystem */
    init_fs();

    /* open input file, process input and close the file*/
    FILE *inputfile, *outputfile;
    inputfile = fopen(argv[1], "r");
    processInput(inputfile);

    /* create the threads */
    for (i = 0; i < numberThreads; i++){
        if (pthread_create(&tid[i], NULL, applyCommands, NULL) == 0){
            continue;
        }
        else {
            printf("Error: thread not created\n");
            exit(EXIT_FAILURE);
        }        
    }

    for ( i = 0; i < numberThreads; i++){
        pthread_join(tid[i], NULL);
    }

    /* close inputfile */
    fclose(inputfile);


    /* destroy all the locks created */
    destroylocks(synchstrategy);

    /* open output file, print the tree to it and close the file*/
    outputfile = fopen(argv[2], "w");
    print_tecnicofs_tree(outputfile);
    fclose(outputfile);

    /* release allocated memory */
    destroy_fs();

    /* stop the clock */
    gettimeofday(&stop, NULL);
    printf("TecnicoFS completed in %.6f seconds.\n", (double) (stop.tv_sec - start.tv_sec)+
                                                    (double) (stop.tv_usec - start.tv_usec)/1000000);

    exit(EXIT_SUCCESS);
}
