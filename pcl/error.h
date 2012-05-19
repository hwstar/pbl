/*
 * Headers for the daemon and client's error handling code.
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
 * 
 */

#ifndef ERROR_H
#define ERROR_H

/* Error levels for debug. */
#define DEBUG_UNEXPECTED 1
#define DEBUG_EXPECTED 2
#define DEBUG_STATUS 3
#define DEBUG_ACTION 4
#define DEBUG_INCOMPLETE 5
#define DEBUG_MAX 5

// Call to redirect the error and log output to a different file (i.e. /tmp/logfile)
void error_logpath(char *path);

// Fatal error handler with strerror(errno);
void fatal_with_reason(int error, char *message, ...);

/* Extremely fatal error handler.  Things that shouldn't happen. */
void panic(char *message, ...);

/* Fatal error handler. */
void fatal(char *message, ...);

/* Debugging handler. */
void debug(int level, char *message, ...);

/* Debugging handler with hexdump feature */

void debug_hexdump(int level, void *buf, int buflen, char *message, ...);

/* Normal error handler. */
void error(char *message, ...);

/* Warning handler. */
void warn(char *message, ...);

#endif
