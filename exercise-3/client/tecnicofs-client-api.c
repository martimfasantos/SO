#include "tecnicofs-client-api.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <arpa/inet.h>

int sockfd;
socklen_t servlen;
struct sockaddr_un serv_addr, client_addr;
char clientSocketID[MAX_INPUT_SIZE];

int setSockAddrUn(char *path, struct sockaddr_un *addr) {

  if (addr == NULL)
    return 0;

  bzero((char *)addr, sizeof(struct sockaddr_un));
  addr->sun_family = AF_UNIX;
  strcpy(addr->sun_path, path);

  return SUN_LEN(addr);
}

int tfsCreate(char *filename, char nodeType) {

  char command[MAX_INPUT_SIZE];
  int result;

  sprintf(command, "c %s %c", filename, nodeType);

  if (sendto(sockfd, command, strlen(command)+1, 0, (struct sockaddr *) &serv_addr, servlen) < 0) {
    perror("client: sendto error");
    exit(EXIT_FAILURE);
  }

  if (recvfrom(sockfd, &result, sizeof(int), 0, 0, 0) < 0) {
    perror("client: recvfrom error");
    exit(EXIT_FAILURE);
  }

  return result;
}

int tfsDelete(char *path) {

  char command[MAX_INPUT_SIZE];
  int result;
  sprintf(command, "d %s", path);

  if (sendto(sockfd, command, strlen(command)+1, 0, (struct sockaddr *) &serv_addr, servlen) < 0) {
    perror("client: sendto error");
    exit(EXIT_FAILURE);
  }

  if (recvfrom(sockfd, &result, sizeof(int), 0, 0, 0) < 0){
    perror("client: recvfrom error");
    exit(EXIT_FAILURE);
  }

  return result;
}

int tfsMove(char *from, char *to) {

  char command[MAX_INPUT_SIZE];
  int result;
  sprintf(command, "m %s %s", from, to);

  if (sendto(sockfd, command, strlen(command)+1, 0, (struct sockaddr *) &serv_addr, servlen) < 0) {
    perror("client: sendto error");
    exit(EXIT_FAILURE);
  }

  if (recvfrom(sockfd, &result, sizeof(int), 0, 0, 0) < 0){
    perror("client: recvfrom error");
    exit(EXIT_FAILURE);
  }

  return result;
}

int tfsLookup(char *path) {

  char command[MAX_INPUT_SIZE] ;
  int result;
  sprintf(command, "l %s", path);

  if (sendto(sockfd, command, strlen(command)+1, 0, (struct sockaddr *) &serv_addr, servlen) < 0) {
    perror("client: sendto error");
    exit(EXIT_FAILURE);
  }

  if (recvfrom(sockfd, &result, sizeof(int), 0, 0, 0) < 0){
    perror("client: recvfrom error");
    exit(EXIT_FAILURE);
  }

  return result;
}

int tfsPrint(char *outputfile){

  char command[MAX_INPUT_SIZE] ;
  int result;
  sprintf(command, "p %s", outputfile);

  if (sendto(sockfd, command, strlen(command)+1, 0, (struct sockaddr *) &serv_addr, servlen) < 0) {
    perror("client: sendto error");
    exit(EXIT_FAILURE);
  }

  if (recvfrom(sockfd, &result, sizeof(int), 0, 0, 0) < 0){
    perror("client: recvfrom error");
    exit(EXIT_FAILURE);
  }

  return result;

}

int tfsMount(char * sockPath) {

  socklen_t clilen;

  if ((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0) ) < 0) {
    perror("client: can't open socket");
    exit(EXIT_FAILURE);
  }

  int clientID = getpid();

  sprintf(clientSocketID, "/tmp/client-%d", clientID);

  unlink(clientSocketID);

  clilen = setSockAddrUn(clientSocketID, &client_addr);

  if (bind(sockfd, (struct sockaddr *) &client_addr, clilen) < 0) {
    perror("client: bind error");
    exit(EXIT_FAILURE);
  }

  servlen = setSockAddrUn(sockPath, &serv_addr);

  return 0;
}

int tfsUnmount() {
  close(sockfd);

  unlink(clientSocketID);

  return 0;
}
