CC=gcc
CFLAGS=-Wall

.PHONY: clean

pcl:	pcl.c serio.o ihx.o dictionary.o iniparser.o error.o hanclient.o socket.o pid.o 
	$(CC) -Wall -o pcl pcl.c hanclient.o socket.o pid.o serio.o ihx.o iniparser.o dictionary.o error.o

dictonary.o:	dictionary.c dictionary.h

iniparser.o:	iniparser.c iniparser.h dictionary.h

serio.o:	serio.c serio.h

ihx.o:		ihx.c ihx.h

error.o:	error.c error.h	

hanclient.o:	hanclient.c hanclient.h pid.h socket.h error.h han.h

socket.o:	socket.c socket.h error.h

pid.o:		pid.c pid.h error.h

clean:
	-rm *.o pcl 


