/*
* ihx.c
*
* Simple Intel Hex File parser
*
*
* Copyright (C) 2010 Stephen Rodgers, All rights reserved.
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
#include <stdlib.h>
#include "error.h"
#include "ihx.h"

static unsigned char hex2(char *p)
{
	unsigned c;
	sscanf(p, "%2x", &c);
	return (unsigned char) c;
}


void ihx_free(ihx_t *p)
{
	if(p){
		free(p);
	}
}


ihx_t *ihx_read(char *path, unsigned char *buffer, unsigned int max_bytes)
{
	unsigned char gotfirstaddr = 0;
	unsigned char rectype, checksum, bytecount;
	unsigned char addrh, addrl;
	unsigned int pos, addr, buffoffset, lineno;
	FILE *f;
	ihx_t *ihx;
	char line[HEXMAXLINE];

	debug(DEBUG_INCOMPLETE, "entering ihx_read()");

	if(!(ihx = malloc(sizeof(ihx_t)))){
		return NULL;
	}


	debug(DEBUG_INCOMPLETE, "ihx allocated in ihx_read");

	ihx->buf = buffer;
	ihx->size = 0;
	debug(DEBUG_INCOMPLETE, "Path to hex file: %s", path);

	if((f = fopen(path, "r"))){
		lineno = 1;
		while(!feof(f)){				
			checksum = 0;
			if(!(fgets(line, HEXMAXLINE, f))){
				ihx_free(ihx);
				debug(DEBUG_INCOMPLETE, "fgets() returned an error");
				return NULL;
			}
			if(line[0] != ':')
				continue;
			bytecount = hex2(line + 1);
			checksum += bytecount;
			addrh = hex2(line + 3);
			checksum += addrh;
			addrl = hex2(line + 5);
			checksum += addrl;
			addr = (((unsigned int) addrh) << 8) + addrl;
			if(!gotfirstaddr){
				ihx->load_address = addr;
				buffoffset = 0;
				gotfirstaddr = 1;
			}
			rectype = hex2(line + 7);
			if(rectype != 0)
				return ihx; /* Done */
			buffoffset = addr - ihx->load_address;
			if(buffoffset > ihx->size)
				ihx->size = buffoffset; /* update size */
			debug(DEBUG_INCOMPLETE, "Line %u, Load Address: %04X, Image Size: %d, Max Image Size: %d", lineno, addr, ihx->size, max_bytes);
			checksum += rectype;
			pos = 9;
			while(bytecount--){
				if(ihx->size > max_bytes){
					debug(DEBUG_INCOMPLETE, "Max bytes exceeded in ihx_read()");
					debug(DEBUG_INCOMPLETE, "Load Address: %04X, Image Size: %u, Max Image Size: %u", addr, ihx->size, max_bytes);
					debug(DEBUG_INCOMPLETE, "Offending line number and line in hex file: %u %s",lineno, line);
					ihx_free(ihx);
						return NULL;
				}
				buffer[buffoffset] = hex2(line + pos);
				checksum += buffer[buffoffset];
				pos += 2;
				buffoffset++;
				ihx->size++;
			}
			if(buffoffset > ihx->size) /* This needs to be here to ensure ihx->size is correct in all cases */
				ihx->size = buffoffset; /* update size */
			checksum ^= 0xFF;
			checksum++;
			if(checksum != hex2(line + pos)){
				ihx_free(ihx);
				debug(DEBUG_INCOMPLETE, "Checksum error in ihx_read()");
				return NULL;
			}
			lineno++;
		}
		debug(DEBUG_INCOMPLETE, "Premature EOF ihx_read()");
		free(ihx);
		return NULL;
	}
	debug(DEBUG_INCOMPLETE, "fopen() failed in ihx_read()");
	return NULL;
}			

void ihx_debug_dump(ihx_t *ihx)
{
	int i;

	for(i = 0 ; i < ihx->size ; i++){
		printf("%02X",ihx->buf[i]);
		if((i % 16) == 15)
			printf("\n");
		else
			printf(" ");
	}
	printf("\n\n");
	printf("Load Address: %04X\n", ihx->load_address);
	printf("Image Size: %04X\n", ihx->size);
}


