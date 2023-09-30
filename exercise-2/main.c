#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <ctype.h>
#include "fs/operations.h"

#define MAX_COMMANDS 10
#define MAX_INPUT_SIZE 100

pthread_mutex_t mutex;
pthread_cond_t canProcess = PTHREAD_COND_INITIALIZER;
pthread_cond_t canApply = PTHREAD_COND_INITIALIZER;

int numberThreads = 0;
extern int synchstrategy;

char inputCommands[MAX_COMMANDS][MAX_INPUT_SIZE];
int numberCommands = 0;
int processptr = 0;
int applyptr = 0;

int STOP = 0;

int insertCommand(char* data) {
    numberCommands++;
    strcpy(inputCommands[processptr++], data);
    processptr %= MAX_COMMANDS;

    return 1;
}

char* removeCommand() {
    if(numberCommands > 0){
        numberCommands--;
        char* command = inputCommands[applyptr++];
        applyptr %= MAX_COMMANDS;
        return command;
    }
    return NULL;
}

void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
    exit(EXIT_FAILURE);
}

void *processInput( void *arg ){

    char line[MAX_INPUT_SIZE];
    FILE* input = (FILE*)arg;

    /* break loop with ^Z or ^D */
    while (fgets(line, sizeof(line)/sizeof(char), input)) {
        pthread_mutex_lock(&mutex);

        while (numberCommands == MAX_COMMANDS)
            pthread_cond_wait(&canProcess, &mutex);
        
        char token;
        char name[MAX_INPUT_SIZE], type[MAX_INPUT_SIZE];
        
        int numTokens = sscanf(line, "%c %s %s", &token, name, type);

        if (numTokens < 1) {
            continue;
        }

        switch (token) {
            case 'c':
                if(numTokens != 3)
                    errorParse();
                if(insertCommand(line))
                    break;
            
            case 'l':
                if(numTokens != 2)
                    errorParse();
                if(insertCommand(line))
                    break;
            
            case 'd':
                if(numTokens != 2)
                    errorParse();
                if(insertCommand(line))
                    break;
            
            case 'm':
                if (numTokens != 3)
                    errorParse();
                if (insertCommand(line)){
                    break;
                }

            case '#':
                break;
            
            default: { /* error */
                errorParse();
            }
        }

        pthread_cond_signal(&canApply);
        pthread_mutex_unlock(&mutex);
    }
    
    STOP = 1;
    pthread_cond_broadcast(&canApply);
        
    return NULL;
}



void *applyCommands(){
    while (1){

        pthread_mutex_lock(&mutex);

        while (numberCommands == 0 && !STOP)
            pthread_cond_wait(&canApply, &mutex);
        
        int locks_vector[LOCKSVECTOR_SIZE];

        if (numberCommands > 0){
            
            char command_[MAX_INPUT_SIZE];
            const char* command = removeCommand();

            strcpy(command_, command);

            /* Send a signal to the producer */
            pthread_cond_signal(&canProcess);

            pthread_mutex_unlock(&mutex);

            if (command_ == NULL){
                continue;
            }

            char token;
            char name[MAX_INPUT_SIZE], type[MAX_INPUT_SIZE];


            int numTokens = sscanf(command_, "%c %s %s", &token, name, type);

            if (numTokens < 2) {
                fprintf(stderr, "Error: invalid command in Queue\n");
                exit(EXIT_FAILURE);
            }

            int searchResult;
            switch (token) {
                case 'c':
                    switch (type[0]) {
                        case 'f':
                            printf("Create file: %s\n", name);
                            create(name, T_FILE);
                            break;
                        case 'd':
                            printf("Create directory: %s\n", name);
                            create(name, T_DIRECTORY);
                            break;
                        default: {
                            fprintf(stderr, "Error: invalid node type\n");
                            exit(EXIT_FAILURE);
                        }
                    }
                    break;
                case 'l':
                    init_locks_vector(locks_vector);
                    searchResult = lookup(name, locks_vector, FIND);
                    if (searchResult >= 0)
                        printf("Search: %s found\n", name);
                    else
                        printf("Search: %s not found\n", name);
                    break;
                case 'd':
                    printf("Delete: %s\n", name);
                    delete(name);
                    break;

                case 'm':
                    printf("Move: %s to %s\n", name, type);
                    move(name, type);
                    break;

                default: { /* error */
                    fprintf(stderr, "Error: command to apply\n");
                    exit(EXIT_FAILURE);
                }
            }
    
        }

        else if (STOP){
            pthread_mutex_unlock(&mutex);
            return NULL;
        }
    }
}

int main(int argc, char* argv[]){

    struct timeval start, stop;

    /* verify the number of arguments */
    if (argc != 4){
        printf("Number of arguments must be 4.\n");
        exit(EXIT_FAILURE);
    }

    /* verify if the number of threads is an integer */
    int length = strlen(argv[3]);
    for (int i = 0; i < length; i++){
        if (!isdigit(argv[3][i])){
            printf("Number of threads must be an integer.\n");
            exit(EXIT_FAILURE);
        }
    }

    /* set the number of threads based on the input */
    numberThreads = atoi(argv[3]);

    if (numberThreads < 1){
        fprintf(stderr, "Error: invalid number of threads\n");
        exit(EXIT_FAILURE);
    }

    pthread_t tid[numberThreads];

    /* init filesystem */
    init_fs();

    /* open input file, process input and close the file*/
    FILE *inputfile, *outputfile;
    inputfile = fopen(argv[1], "r");

    if (inputfile == NULL){
        fprintf(stderr, "Error: Input file not found\n");
        exit(EXIT_FAILURE);
    }

    /* start the clock */
    gettimeofday(&start, NULL);

    /* create the main thread (producer) */
    pthread_create(&tid[0], NULL, processInput, inputfile);


    /* create the slave threads (consumers) */
    for (int i = 1; i <= numberThreads; i++){
        if (pthread_create(&tid[i], NULL, applyCommands, NULL) != SUCCESS){
            printf("Error: thread not created\n");
            exit(EXIT_FAILURE);
        }        
    }


    for (int i = 0; i <= numberThreads ; i++){
        if (pthread_join(tid[i], NULL) != SUCCESS){
            printf("Error: thread failed to join\n");
            exit(EXIT_FAILURE);
        }    
    }

    /* stop the clock */
    gettimeofday(&stop, NULL);

    /* close inputfile */
    fclose(inputfile);

    /* open output file, print the tree to it and close the file*/
    outputfile = fopen(argv[2], "w");

    print_tecnicofs_tree(outputfile);
    fclose(outputfile);

    /* release allocated memory */
    destroy_fs();

    printf("TecnicoFS completed in %.4f seconds.\n", (double) (stop.tv_sec - start.tv_sec)+
                                                    (double) (stop.tv_usec - start.tv_usec)/1000000);

    exit(EXIT_SUCCESS);
}
