TARGETS = client server

CC     = gcc
CFLAGS = -Wall -O2
LFLAGS = -Wall

all: $(TARGETS)

client.o server.o err.o: err.h

client: client.o err.o

server: server.o err.o

clean:
	rm -f $(TARGETS) *.o *~ *.bak
