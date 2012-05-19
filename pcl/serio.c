
/*
* serio.c
*
* Modifications Copyright (C) 2010 Stephen Rodgers, All rights reserved.
*
*
*
*/

/*
* This file is part of the PBL (PIC Boot Loader) Project
*
*   PBL is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 2 of the License, or
*   (at your option) any later version.

*   PBL is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with PBL.  If not, see <http://www.gnu.org/licenses/>.
*/



#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include "serio.h"

/* 
 * Open the serial device. 
 *
 * Description of how to do the serial handling came from some mini serial
 * port programming howto.
 */

serioStuff *serio_open(char *tty_name, unsigned baudrate) {
	struct termios termios;
	serioStuff *serio;
	speed_t brc;
	

	if((serio = malloc(sizeof(serioStuff))) == NULL)
		return NULL;

	/* 
	 * Open the serio tty device.
	 */
	serio->fd=open(tty_name, O_RDWR | O_NOCTTY | O_NDELAY);
	if(serio->fd == -1) {
		return NULL;
	}
	
	
	/* Set the options on the port. */
	
	/* We don't want to block reads. */
	if(fcntl(serio->fd, F_SETFL, O_NONBLOCK) == -1) {
		return NULL;
	}
	
	/* Get the current tty settings. */
	if(tcgetattr(serio->fd, &termios) != 0) {
		return NULL;
	}
	
	/* Enable receiver. */
	termios.c_cflag |= CLOCAL | CREAD;
	
	/* Set to 8N1. */
	termios.c_cflag &= ~PARENB;
	termios.c_cflag &= ~CSTOPB;
	termios.c_cflag &= ~CSIZE;
	termios.c_cflag |=  CS8;
	
	/* Accept raw data. */
	termios.c_lflag &= ~(ICANON | ECHO | ISIG);
	termios.c_oflag &= ~(OPOST | ONLCR | OCRNL | ONLRET | OFILL);
	termios.c_iflag &= ~(ICRNL | IXON | IXOFF | IMAXBEL);
	
	switch(baudrate){
		case 9600:
			brc = B9600;
			break;

		case 57600:
			brc = B57600;
			break;

		default:
			return NULL;
	}


	/* Set the speed of the port. */
	if(cfsetospeed(&termios, brc) != 0) {
		return NULL;
	}
	if(cfsetispeed(&termios, brc) != 0) {
		return NULL;
	}
	
	/* Save our modified settings back to the tty. */
	if(tcsetattr(serio->fd, TCSANOW, &termios) != 0) {
		return NULL;
	}
	
	return(serio);
}

/* Flush the input buffer */

int serio_flush_input(serioStuff *serio)
{
	return tcflush(serio->fd, TCIFLUSH);
}



/* Close the TTY port, and free the serio structure */

void serio_close(serioStuff *serio){
	close(serio->fd);
	free(serio);
}

/* 
 * Wait for the serio hardware to provide us with some data.
 *
 * This function should only be called when we know the serio should have sent
 * us something.  We don't wait long in here, if it isn't screwed up, it
 * should be sending quite quickly.  We return true if we got a byte and
 * false if we timed out waiting for one.
 */
int serio_wait_read(serioStuff *serio, int rx_timeout) {
	fd_set read_fd_set;
	struct timeval tv;
	int retval;
	
	/* Wait for data to be readable. */
	for(;;) {
		
		/* Make the call to select to wait for reading. */
		FD_ZERO(&read_fd_set);
		FD_SET(serio->fd, &read_fd_set);
		tv.tv_sec=0;
		tv.tv_usec=rx_timeout;
		retval=select(serio->fd+1, &read_fd_set, NULL, NULL, &tv);
		
		/* Did select error? */
		if(retval == -1) {
			
			/* If it's an EINTR, go try again. */
			if(errno == EINTR) {
//			debug(DEBUG_EXPECTED, "Signal recieved in read select, restarting.");
				continue;
			}
			
			/* It was something weird. */
			return(-1);
		}
		
		/* Was data available? */
		if(retval) {	
			
			/* We got some data, return ok. */
			return(1);
		}
		
		/* No data available. */
		else {
			
			/* We didn't get any data. This is a time out */
			return(0);
		}
	}
}


/* 
 * Wait for the serio hardware to be writable.
 */

int serio_wait_write(serioStuff *serio, int tx_timeout) {
	fd_set write_fd_set;
	struct timeval tv;
	int retval;
	
	/* Wait for data to be writable. */
	for(;;) {
		
		/* Make the call to select to wait for writing. */
		FD_ZERO(&write_fd_set);
		FD_SET(serio->fd, &write_fd_set);
		tv.tv_sec=0;
		tv.tv_usec=tx_timeout;
		retval=select(serio->fd+1, NULL, &write_fd_set, NULL, &tv);
		
		/* Did select error? */
		if(retval == -1) {
			
			/* If it's an EINTR, go try again. */
			if(errno == EINTR) {
				continue;
			}
			
			/* It was something weird. */
			return(-1); 
		}
		
		/* Can we write data? */
		if(retval) {	
			
			/* We can write some data, return ok. */
			return(1);
		}
		
		/* No data writable. */
		else {
			
			/* We can't write any data. this is a time out. */
			return(0);
		}
	}
}


/* 
 * Read data from the serio hardware.
 *
 * Basically works like read(), but with a select-provided readable check
 * and timeout.
 * 
 * Returns the number of bytes read.  This might be less than what was given
 * if we ran out of time.
 */
int serio_read(serioStuff *serio, void *buf, size_t count, int rx_timeout) {
	int bytes_read;
	ssize_t retval;
	
	/* Read the request into the buffer. */
	for(bytes_read=0; bytes_read < count;) {
		
		/* Wait for data to be available. */
		if(!serio_wait_read(serio, rx_timeout)) {
			return(bytes_read);
		}
		
		/* Get as much of it as we can.  Loop for the rest. */
		retval=read(serio->fd, (char *) buf + bytes_read, count - bytes_read);
		if(retval == -1) {
			return -1;
		}
		bytes_read += retval;
	}
	
	/* We're all done. */
	return(bytes_read);
}


/* 
 * Write data to the serio hardware.
 *
 * Basically works like write(), but with a select-provided writeable check
 * and timeout.
 * 
 * Returns the number of bytes written.  This might be less than what was
 * given if we ran out of time.
 */
int serio_write(serioStuff *serio, void *buf, size_t count, int tx_timeout) {
	int bytes_written;
	ssize_t retval;
	
	/* Write the buffer to the serio hardware. */
	for(bytes_written=0; bytes_written < count;) {
		
		/* Wait for data to be writeable. */
		if(!serio_wait_write(serio, tx_timeout)) {
//			debug(DEBUG_UNEXPECTED, "Gave up waiting for serio to be writeable.");
			return(bytes_written);
		}
		
		/* Get as much of it as we can.  Loop for the rest. */
		retval=write(serio->fd, (char *) buf + bytes_written, count - bytes_written);
		if(retval == -1) {
			return -1;
		}
		bytes_written += retval;
//		debug(DEBUG_ACTION, "Wrote %i bytes, %i remaining.", retval, count - bytes_written);
	}
	
	/* We're all done. */
	return(bytes_written);
}

