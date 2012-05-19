/*
 * hanclient.c.  Client functions home automation network (HAN).
 *
 * Copyright (C) 1999,2002,2011 Stephen Rodgers
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
 * Stephen "Steve" Rodgers <hwstar@rodgers.sdcoxmail.com>
 *
 * $Id$
 */


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "options.h"
#include "error.h"
#include "socket.h"
#include "pid.h"
#include "han.h"
#include "hanclient.h"

/*
* Internal globals
*/
static char *inetService = NULL;
static char *inetHost = NULL;
static char *sockFilePath = NULL;
static char *networkErr = "HAN network error: ";

/*
* Determine the best method to connect and check to see if everything is in place
* to allow it to happen, then do a test connect to see we can really connect.
*
* Return 0 if successfull else 1 if fail.
*/

int hanclient_connect_setup(char *pidpath, char *sockfilepath, char *service, char *host){

	/* Always give preference to unix domain sockets, they are faster than inet sockets */

	if(sockfilepath[0]){
		/* Make sure we are running hand (.pid file check). */
       		if(pid_read(pidpath) == -1){
			debug(DEBUG_ACTION, "Cannot establish a unix domain connection, hand is not running, pid path = '%s'.", pidpath);
			return 1;
		}
                sockFilePath = sockfilepath;
        }
	else if(service[0]){
		/* Make sure we have a host defined */
		if(host[0] == 0){
			debug(DEBUG_ACTION, "Cannot establish an inet connection, host not defined in config file.");
			return 1;
		}
		inetService = service;
		inetHost = host;	
	}
	else{
		debug(DEBUG_ACTION, "Can't determine a valid connect method (unix or ip) in the config file to reach the han server.");
		return 1;
	}
	return 0;
}


int hanclient_send_command_return_res(Client_Command *client_command)
{
	int sock,i;

	/* Attempt to connect to the han daemon */
  
	if(sockFilePath != NULL)
		sock = socket_connect(sockFilePath);
	else
		sock = socket_connect_ip(inetHost, inetService, PF_UNSPEC, SOCK_STREAM );

	if(sock == -1){
		debug(DEBUG_ACTION, "Could not open socket to host: %s", inetHost);
		return 1;
	}

	/* Set the socket file mode to non-blocking */

	if(fcntl(sock, F_SETFL, O_NONBLOCK) == -1){
		debug(DEBUG_ACTION, "Could not set socket to nonblocking");
		socket_close(sock);
		return 1;
	}

	/* Write the client command block */

	i = socket_write(sock,
		(void *) client_command,
		sizeof(Client_Command),
		USER_WRITE_TIMEOUT);	

	if(i == 0){
		debug(DEBUG_ACTION, "Socket time out error, writing client command");
		socket_close(sock);
		return 1;
	}
  

	debug(DEBUG_ACTION,"packet sent, waiting for response");

	/* Wait for, and read back the response */

	i = socket_read(sock,
		(void *) client_command,
		sizeof(Client_Command),
		USER_READ_TIMEOUT);

	if(i == 0){
		debug(DEBUG_ACTION,"Socket time out error, waiting for response");
		socket_close(sock);
		return 1;
	}
	/* Close the socket */

	socket_close(sock);

	return 0;
}


/*
* Send a command to a node on a network, wait for the response, or time out
*/

void hanclient_send_command(Client_Command *client_command)
{

	hanclient_send_command_return_res(client_command);
	hanclient_error_check(client_command);
}

/* 
* Check the result code is good. If not, and post fatal error and exit
*/

void hanclient_error_check(Client_Command *client_command)
{
	switch(client_command->commstatus){
		case HAN_CSTS_OK:
			return;
			
		case HAN_CSTS_CMD_UNKNOWN:
			fatal("Unknown client command %d",client_command->request);
		
		case HAN_CSTS_RX_TIMEOUT:
		case HAN_CSTS_TX_TIMEOUT:
			fatal("%sCommunications time out", networkErr);

		case HAN_CSTS_CRC_ERROR:
			fatal("%sCommunications CRC error", networkErr);

		case HAN_CSTS_FORMAT_ERROR:
			fatal("%sUnknown header format", networkErr);
			
		case HAN_CSTS_NAK_ERROR:
			fatal("Node returned NAK response");

    		case HAN_CSTS_PPOWER_ERROR:
      			fatal("sending ppower command");

		case HAN_CSTS_PPOWER_CONFIG_ERROR:
			fatal("ppower path not defined in han.conf");

		case HAN_CSTS_PPOWER_FORK_ERROR:
			fatal("Could not fork an instance of ppower");   

		case HAN_CSTS_INVPARM:
			fatal("Invalid parameter passed");


		default:
			panic("%sUnknown error code %d", networkErr, client_command->commstatus);

		}
}
