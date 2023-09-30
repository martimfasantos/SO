# Makefile, versao 2
# Sistemas Operativos, DEI/IST/ULisboa 2020-21

CC   = gcc
LD   = gcc
CFLAGS =-Wall -std=gnu99 -I../
LDFLAGS=-lm -pthread

# A phony target is one that is not really the name of a file
# https://www.gnu.org/software/make/manual/html_node/Phony-Targets.html
.PHONY: all clean run

all: tecnicofs tecnicofs-client

tecnicofs: server/fs/state.o server/fs/operations.o server/tecnicofs-server.o
	$(LD) $(CFLAGS) $(LDFLAGS) -o tecnicofs server/fs/state.o server/fs/operations.o server/tecnicofs-server.o

tecnicofs-client: client/tecnicofs-client-api.o client/tecnicofs-client.o
	$(LD) $(CFLAGS) $(LDFLAGS) -o tecnicofs-client client/tecnicofs-client-api.o client/tecnicofs-client.o

server/fs/state.o: server/fs/state.c server/fs/state.h tecnicofs-api-constants.h
	$(CC) $(CFLAGS) -o server/fs/state.o -c server/fs/state.c

server/fs/operations.o: server/fs/operations.c server/fs/operations.h server/fs/state.h tecnicofs-api-constants.h
	$(CC) $(CFLAGS) -o server/fs/operations.o -c server/fs/operations.c

server/tecnicofs-server.o: server/tecnicofs-server.c server/fs/operations.h server/fs/state.h tecnicofs-api-constants.h
	$(CC) $(CFLAGS) -o server/tecnicofs-server.o -c server/tecnicofs-server.c

client/tecnicofs-client.o: client/tecnicofs-client.c tecnicofs-api-constants.h client/tecnicofs-client-api.h
	$(CC) $(CFLAGS) -o client/tecnicofs-client.o -c client/tecnicofs-client.c

client/tecnicofs-client-api.o: client/tecnicofs-client-api.c tecnicofs-api-constants.h client/tecnicofs-client-api.h
	$(CC) $(CFLAGS) -o client/tecnicofs-client-api.o -c client/tecnicofs-client-api.c

clean:
	@echo Cleaning...
	rm -f server/*.o server/fs/*.o client/*.o tecnicofs tecnicofs-client

run: tecnicofs tecnicofs-client
	./tecnicofs && ./tecnicofs-client
