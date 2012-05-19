/*
 * config.h.  Configuration header file for the han project
 *
 * Copyright (C) 1999,2002,2003 Stephen Rodgers
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
 * Stephen "Steve" Rodgers <hwstar@cox.net>
 *
 * $Id$
 */


// Global configs


#define CONF_FILE_PATH "/etc/han.conf"    // Path to config file

#define DAEMON_PID_FILE "hand.pid"        // PID file name
#define CONF_PID_PATH "/var/run/hand.pid" // Path to pid file

//  configs

#define DAEMON_SOCKET_FILE "hand.socket"// Unix domain socket name
#define CONF_MAX_RETRIES 3		// Maximum number of retries



/*
* Don't change anything below here unless you know what you are doing
*/

#define	POLL_MAX_COUNT 20		// Maximum number of fd's to POLL
#define MAX_CONFIG_STRING 128		// Maximum length of a config string
#define MAX_NODE_PARAMS 16              // Maximum number of parameter bytes in a packet
#define USER_READ_TIMEOUT 30000000      // Time to wait for socket reads
#define USER_WRITE_TIMEOUT 30000000     // Time to wait for socket writes





