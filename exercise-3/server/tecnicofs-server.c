#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <ctype.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <strings.h>

#include "fs/operations.h"

pthread_mutex_t mutexglobal = PTHREAD_MUTEX_INITIALIZER;
int numberThreads = 0;

extern int sockfd;
extern struct sockaddr_un server_addr;
socklen_t addrlen;

static void displayUsage (const char* appName) {
    printf("Usage: %s number_of_threads server_socket_name\n", appName);
    exit(EXIT_FAILURE);
}

static void parseArgs(long argc, char* const argv[]){

    /* verify the number of arguments */
    if (argc != 3){
        fprintf(stderr, "Invalid format:\n");
        displayUsage(argv[0]);
    }

    /* verify if the number of threads is an integer */
    int length = strlen(argv[1]);
    for (int i = 0; i < length; i++){
        if (!isdigit(argv[1][i])){
            printf("Number of threads must be an integer.\n");
            exit(EXIT_FAILURE);
        }
    }

    /* set the number of threads based on the input */
    numberThreads = atoi(argv[1]);

    if (numberThreads < 1){
        fprintf(stderr, "Error: invalid number of threads\n");
        exit(EXIT_FAILURE);
    }
}

char *receiveCommand(struct sockaddr_un *client_addr) {

    struct sockaddr_un thisclient_addr;
    char *command;
    char in_buffer[MAX_INPUT_SIZE];
    int c;

    addrlen = sizeof(struct sockaddr_un);
    c = recvfrom(sockfd, in_buffer, sizeof(in_buffer)-1, 0,
        (struct sockaddr *)&thisclient_addr, &addrlen);

    if (c <= 0) return NULL;
    //Preventivo, caso o cliente nao tenha terminado a mensagem em '\0', 
    in_buffer[c]='\0';

    *client_addr = thisclient_addr;
    command = in_buffer;

    return command;
}

void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
    exit(EXIT_FAILURE);
}

void *applyCommands(){
    while (1){
        
        struct sockaddr_un client_addr;
        char command_[MAX_INPUT_SIZE];

        const char *command = receiveCommand(&client_addr);

        if (command == NULL)
            continue;
        
        strcpy(command_, command);

        char token;
        char name[MAX_INPUT_SIZE], type[MAX_INPUT_SIZE];

        int numTokens = sscanf(command_, "%c %s %s", &token, name, type);

        if (numTokens < 2) {
            fprintf(stderr, "Error: invalid command in Queue\n");
            exit(EXIT_FAILURE);
        }

        int searchResult;
        int operationResult;
        
        switch (token) {
            case 'c':
                switch (type[0]) {
                    case 'f':
                        pthread_mutex_lock(&mutexglobal);
                        printf("Create file: %s\n", name);
                        operationResult = create(name, T_FILE);
                        pthread_mutex_unlock(&mutexglobal);
                        break;
                    case 'd':
                        pthread_mutex_lock(&mutexglobal);
                        printf("Create directory: %s\n", name);
                        operationResult = create(name, T_DIRECTORY);
                        pthread_mutex_unlock(&mutexglobal);
                        break;
                    default: {
                        fprintf(stderr, "Error: invalid node type\n");
                        exit(EXIT_FAILURE);
                    }
                }
                break;
            case 'l':
                pthread_mutex_lock(&mutexglobal);
                searchResult = lookup(name);
                operationResult = searchResult;
                if (searchResult >= 0)
                    printf("Search: %s found\n", name);
                else
                    printf("Search: %s not found\n", name);
                pthread_mutex_unlock(&mutexglobal);
                break;
            case 'd':
                pthread_mutex_lock(&mutexglobal);
                printf("Delete: %s\n", name);
                operationResult = delete(name);
                pthread_mutex_unlock(&mutexglobal);
                break;

            case 'm':
                pthread_mutex_lock(&mutexglobal);
                printf("Move: %s to %s\n", name, type);
                operationResult = move(name, type);
                pthread_mutex_unlock(&mutexglobal);
                break;

            case 'p':
                pthread_mutex_unlock(&mutexglobal);
                printf("Print to file: %s\n", name);
                operationResult = print(name);
                pthread_mutex_unlock(&mutexglobal);
                break;

            default: { /* error */
                fprintf(stderr, "Error: command to apply\n");
                exit(EXIT_FAILURE);
            }
        }

        if (sendto(sockfd, &operationResult, sizeof(int), 0, (struct sockaddr *)&client_addr, addrlen) < 0){
            perror("server: bind error");
            exit(EXIT_FAILURE);
        } 
    }
}

int main(int argc, char* argv[]){

    parseArgs(argc, argv);

    init_fs();

    if (tfsMount(argv[2]) == 0)
      printf("Mounted! (socket = %s)\n", argv[2]);
    else {
      fprintf(stderr, "Unable to mount socket: %s\n", argv[2]);
      destroy_fs();
      exit(EXIT_FAILURE);
    }

    pthread_t tid[numberThreads];

    /* create the slave threads (consumers) */
    for (int i = 0; i < numberThreads; i++){
        if (pthread_create(&tid[i], NULL, applyCommands, NULL) != SUCCESS){
            printf("Error: thread not created\n");
            exit(EXIT_FAILURE);
        }        
    }

    for (int i = 0; i < numberThreads ; i++){
        if (pthread_join(tid[i], NULL) != SUCCESS){
            printf("Error: thread failed to join\n");
            exit(EXIT_FAILURE);
        }    
    }

    //Fechar e apagar o nome do socket, apesar deste programa 
    //nunca chegar a este ponto
    close(sockfd);
    unlink(argv[1]);

    /* release allocated memory */
    destroy_fs();

    exit(EXIT_SUCCESS);
}
