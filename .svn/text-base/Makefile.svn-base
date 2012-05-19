.PHONY: clean pgm 

CC=ccsc
CFLAGS=-D


all:	bloader.hex 

bloader.hex: bloader.c 
	$(CC) $(CFLAGS) $<

clean:
	-rm *.hex *.o ./-d-debug.txt ccsc_log.txt *.sym *.esym *.lst *.err 



