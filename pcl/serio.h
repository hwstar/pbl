/*
 * serio definitions.
 *
 * Copyright (C) 2010 Stephen Rodgers 
 *
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef SERIO_H
#define SERIO_H

#include <time.h>
#include <unistd.h>

/* The maximum time to wait for an expected byte to be readable. */
#define SERIO_WAIT_READ_USEC_DELAY 5000000

/* The maximum time to wait to be able to write to the x10 hardware. */
#define SERIO_WAIT_WRITE_USEC_DELAY 5000000

/* Typedefs. */
typedef struct seriostuff serioStuff;

/* Structure to hold serio info. */
struct seriostuff {
	
	/* File descriptor to the serio tty. */
	int fd;
};

/* Prototypes. */
serioStuff *serio_open(char *tty_name, unsigned baudrate);
void serio_close(serioStuff *hanio);
int serio_flush_input(serioStuff *serio);
int serio_wait_read(serioStuff *hanio, int rx_timeout);
int serio_wait_write(serioStuff *hanio, int tx_timeout);
int serio_read(serioStuff *hanio, void *buf, size_t count, int rx_timeout);
int serio_write(serioStuff *hanio, void *buf, size_t count, int tx_timeout);

#endif
