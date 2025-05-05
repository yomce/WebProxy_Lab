CC = gcc
CFLAGS = -O0 -Wall -I . -g
# CFLAGS = -O2 -Wall -I . -g

# This flag includes the Pthreads library on a Linux box.
# Others systems will probably require something different.
LIB = -lpthread

all: echoclient echoserveri

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

echoclient: echoclient.c csapp.o
	$(CC) $(CFLAGS) -o echoclient echoclient.c csapp.o $(LIB)

echoserveri: echoserveri.c csapp.o
	$(CC) $(CFLAGS) -o echoserveri echoserveri.c csapp.o $(LIB)

clean:
	rm -f *.o echoclient echoserveri *~