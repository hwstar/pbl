
/*
*
* error.c
*
* Copyright (C) 1999  Steven Brown
* Modifications Copyright (C) 2010 Stephen Rodgers, All rights reserved.
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
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include "error.h"

#define LOGOUT (output == NULL ? stderr : output)

/* Name of the program. */
extern char *progname;

/* Debugging level. */
extern int debuglvl;

FILE *output = NULL;


/*
* Set the error and debug output to the path specified instead of stderr
*/

void error_logpath(char *path)
{
  FILE *f;

  if(output != NULL)
    fclose(output);
    
  if((f = fopen(path,"w")) == NULL)
    fatal_with_reason(errno, "Can't open log file for writing");
  output = f;
}


/* Fatal error handler with strerror(errno) reason */

void fatal_with_reason(int error, char *message, ...)
{
    va_list ap;
    
    va_start(ap, message);

    fprintf(LOGOUT, "%s: ", progname);
    vfprintf(LOGOUT, message, ap);
    fprintf(LOGOUT, ": %s\n",strerror(error));

    va_end(ap);
    exit(1);
}


/* Fatal error handler. */
void fatal(char *message, ...) {
	va_list ap;
	va_start(ap, message);
	
	/* Print the error message. */
	fprintf(LOGOUT,"%s: ",progname);
	vfprintf(LOGOUT,message,ap);
	fprintf(LOGOUT,"\n");
	
	/* Exit with an error code. */
	va_end(ap);
	exit(1);
}


/* Normal error handler. */
void error(char *message, ...) {
	va_list ap;
	va_start(ap, message);
	
	/* Print the error message. */
	fprintf(LOGOUT,"%s: ",progname);
	vfprintf(LOGOUT,message,ap);
	fprintf(LOGOUT,"\n");
	
	va_end(ap);
	return;
}


/* Warning handler. */
void warn(char *message, ...) {
	va_list ap;
	va_start(ap, message);
	
	/* Print the warning message. */
	fprintf(LOGOUT,"%s: warning: ",progname);
	vfprintf(LOGOUT,message,ap);
	fprintf(LOGOUT,"\n");
	
	va_end(ap);
	return;
}


/* Panic error handler. */
void panic(char *message, ...) {
	va_list ap;
	va_start(ap, message);
	
	/* Print the error message. */
	fprintf(LOGOUT,"%s: PANIC: ",progname);
	vfprintf(LOGOUT,message,ap);
	fprintf(LOGOUT,"\n");
	
	/* Exit with an error code. */
	va_end(ap);
	exit(1);
}


/* Debugging error handler. */
void debug(int level, char *message, ...) {
	va_list ap;
	va_start(ap, message);
  char timenow[32];
  int l;
  time_t t;
	
	/* We only do this code if we are at or above the debug level. */
	if(debuglvl >= level) {
    t = time(NULL);
    strcpy(timenow,ctime(&t));
    l = strlen(timenow);
    if(l)
      timenow[l-1] = '\0';
      
 		/* Print the error message. */
		fprintf(LOGOUT,"%s [ %s ] (debug): ", progname, timenow);
		vfprintf(LOGOUT, message, ap);
		fprintf(LOGOUT,"\n");
    if(output != NULL)  // If we are writing to a log file, flush the debug output.
      fflush(output);
	}
	
	va_end(ap);
}

/* Print a debug string with a buffer of bytes to print */

void debug_hexdump(int level, void *buf, int buflen, char *message, ...){
	int i;
	va_list ap;
	va_start(ap, message);

	if(debuglvl >= level) {
		fprintf(LOGOUT,"%s: (debug): ",progname);
		vfprintf(LOGOUT,message,ap);
		for(i = 0 ; i < buflen ; i++)
			fprintf(LOGOUT,"%02X ",((int) ((unsigned char *)buf)[i]) & 0xFF);
		fprintf(LOGOUT,"\n");
	}
}

