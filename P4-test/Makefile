CC   = gcc
OPTS = -Wall -g

all: libmfs server client

server: server.o udp.o
	$(CC) -o server server.o udp.o 

client: client.o udp.o
	$(CC) -o client client.o udp.o  libmfs.so

libmfs: udp.o libmfs.o
	$(CC) -shared -g -Wall -o libmfs.so udp.o libmfs.o
udp.o:
	$(CC) $(OPTS) -c -fpic udp.c -o udp.o
libmfs.o:
	$(CC) $(OPTS) -c -fpic libmfs.c -o libmfs.o

%.o: %.c 
	$(CC) $(OPTS) -c $< -o $@

clean:
	rm -f server.o udp.o client.o server client libmfs.so libmfs.o
