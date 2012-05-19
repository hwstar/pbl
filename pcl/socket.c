/*
 * Code to handle the daemon and client's socket needs.
 *
 * Copyright (C) 2000,2002 Stephen Rodgers
 *
 * Original work as used in ppower
 * Copyright (C) 1999  Steven Brown
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
 * Steven Brown <swbrown@ucsd.edu>
 *
 * $Id$
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/fcntl.h>
#include "error.h"
#include "socket.h"

/*
* Attempt to close a socket and report an error if there was a problem
*/

void socket_close(int socket) {
	if(close(socket))
		fatal_with_reason(errno, "Could not close socket");
}

	
/* Create the daemon socket. */	
int socket_create(char *socket_name, int mode, int uid, int gid) {
	struct sockaddr_un sock_addr;
	int daemon_socket;
	
	/* 
	 * *** What mode does it create the socket with?  User only, or
	 * does it use the umask?  If so, that's bad, it will mean a
	 * permissions race.
	 */
	
	/* Create a socket for talking to the user program. */
	daemon_socket=socket (AF_UNIX, SOCK_STREAM, 0);
	if(daemon_socket == -1) {
		fatal_with_reason(errno, "Could not create daemon socket");
	}
	
	/* Bind the socket to the filesystem. */
	sock_addr.sun_family=AF_UNIX;
	strncpy(sock_addr.sun_path, socket_name, sizeof(sock_addr.sun_path) -1);
	sock_addr.sun_path[sizeof(sock_addr.sun_path) -1] = 0;
	if(bind(daemon_socket, (struct sockaddr *) &sock_addr, sizeof(sock_addr)) != 0) {
		fatal_with_reason(errno, "Could not bind socket '%s'", socket_name);
	}
	
	/* Clear the socket's permissions while we change the ownership. */
	if(chmod(socket_name, 0) != 0) {
		fatal_with_reason(errno, "Could not clear permissions on socket '%s'", socket_name);
	}
	
	/* 
	 * Change the ownership of the socket to the ones specified.  I'd
	 * rather use fchown, but it fails quietly on bound sockets?  If we
	 * got here, we are guaranteed that the socket exists by socket_name
	 * anyway, so it should be safe.
	 */
	if(chown(socket_name, uid, gid) != 0) {
		fatal_with_reason(errno, "Could not chown socket '%s'", socket_name);
	}
	
	/* Set the socket's real permissions. */
	if(chmod(socket_name, mode) != 0) {
		fatal_with_reason(errno, "Could not set permissions on socket '%s'", socket_name);
	}
	
	/* Set the socket to listen for connections. */
	if(listen(daemon_socket, SOMAXCONN) != 0) {
		fatal_with_reason(errno, "Could not set socket to listen.");
	}
	
	/* Return this socket. */
	return(daemon_socket);
}


/* Create a listening socket list. */
	
int socket_create_listen(char *bindaddr, char *service, int family, int socktype, int (*addsock)(int sock, void *addr, int family, int socktype)) {
	struct addrinfo hints, *list, *p;
	int sock = -1, res;
	int sockcount = 0;

	if(!addsock)
		return -1;


	memset(&hints, 0, sizeof hints);


	// Init the hints struct for getaddrinfo

	hints.ai_family = family;
	hints.ai_socktype = socktype;
	if(bindaddr == NULL)	
		hints.ai_flags = AI_PASSIVE;

	// Get the address list
	if((res = getaddrinfo(bindaddr, service, &hints, &list)) == -1){
		debug(DEBUG_ACTION, "socket_create_listen(): getaddrinfo failed: %s", gai_strerror(res));
		return -1;
	}

	for(p = list; p != NULL; p = p->ai_next){ // Traverse the list
		int sockopt = 1;
	
		if((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
			debug(DEBUG_ACTION,"socket_create_listen(): Call to socket failed with %s, continuing...",strerror(errno));
			continue;
		}		

		// If IPV6 socket, set IPV6 only option so port space does not clash with an IPV4 socket
		// This is necessary in order to prevent the ipv6 bind from failing when an IPV4 socket was previously bound.

		if(p->ai_family == PF_INET6){
			setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &sockopt, sizeof(sockopt ));
			debug(DEBUG_ACTION,"socket_create_listen(): Setting IPV6_V6ONLY socket option");
		}
			
		/* Set to reuse socket address when program exits */

		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt));


		if(bind(sock, p->ai_addr, p->ai_addrlen) == -1){
			debug(DEBUG_ACTION,"socket_create_listen(): Bind failed with %s, continuing...", strerror(errno));
			close(sock);
			continue;
		}

		if(listen(sock, SOMAXCONN) == -1){
			debug(DEBUG_ACTION, "socket_create_listen(): Listen failed with %s, continuing...", strerror(errno));
			close(sock);
			continue;
			}

		/* Callback to have caller do something with the socket */

		sockcount++;

		if((*addsock)(sock, p->ai_addr, p->ai_family, p->ai_socktype))
			break;
	}

	freeaddrinfo(list);

	if(!sockcount){
		debug(DEBUG_ACTION, "socket_create_listen(): could not create, bind or listen on a socket");
		return -1;
	}

	return 0;
}






/* 
 * Connect to the daemon socket.
 *
 * Returns the fd of the socket or -1 if error
 */
int socket_connect(char *socket_name) {
	struct sockaddr_un sock_addr;
	int daemon_socket;
	
	/* Create a socket for talking to the daemon program. */
	daemon_socket=socket (AF_UNIX, SOCK_STREAM, 0);
	if(daemon_socket == -1) {
		debug(DEBUG_ACTION, "Could not create socket: %s", strerror(errno));
		return -1;
	}
	
	/* Connect the socket to the daemon's socket. */
	sock_addr.sun_family=AF_UNIX;
	strncpy(sock_addr.sun_path, socket_name, sizeof(sock_addr.sun_path) -1);
	sock_addr.sun_path[sizeof(sock_addr.sun_path) -1] = 0;
	if(connect(daemon_socket, (struct sockaddr *) &sock_addr, sizeof(sock_addr)) != 0) {
		debug(DEBUG_ACTION, "Could not connect to socket '%s'.", socket_name);
		return -1;
	}
	
	/* Return this socket. */
	return(daemon_socket);
}
/*
 * Connect to the daemon socket.
 *
 * Returns the fd of the socket or -1 if error
 */
int socket_connect_ip(char *host, char *service, int family, int socktype) {

	struct addrinfo hints, *list, *p, *ipv6 = NULL, *ipv4 = NULL;
	int sock, res;


	if((!host) || (!service))
		return -1;

  	memset(&hints, 0, sizeof hints);
	
	hints.ai_family = family;
	hints.ai_socktype = socktype;
	
	// Get the address list
	if((res = getaddrinfo(host, service, &hints, &list)) == -1){
		debug(DEBUG_ACTION, "socket_connect_ip(): getaddrinfo failed: %s", gai_strerror(res));
		return -1;
	}
	for(p = list; p ; p = p->ai_next){
		if((!ipv6) && (p->ai_family == PF_INET6))
			ipv6 = p;
		if((!ipv4) && (p->ai_family == PF_INET))
			ipv4 = p;
	}

	if(!ipv4 && !ipv6){
		debug(DEBUG_ACTION,"socket_connect_ip(): Could not find a suitable IP address to connect to");
		return -1;
	}
	
	p = (ipv6) ? ipv6 : ipv4; // Prefer IPV6 over IPV4

	/* Create a socket for talking to the daemon program. */

	sock = socket(p->ai_family, p->ai_socktype,p->ai_protocol );
	if(sock == -1) {
		freeaddrinfo(list);
		debug(DEBUG_ACTION, "socket_connect_ip(): Could not create ip socket: %s", strerror(errno));
		return -1;
	}


	/* Connect the socket */

	if(connect(sock, (struct sockaddr *) p->ai_addr, p->ai_addrlen)) {
		freeaddrinfo(list);
		debug(DEBUG_ACTION, "socket_connect_ip(): Could not connect to inet host:port '%s:%s'.", host, service);
		return -1;
	}
	
	freeaddrinfo(list);

	/* Return this socket. */
	return(sock);
}


/*
 * Read from a socket.
 *
 * The socket must be non-blocking, or the timeout is useless.  We return
 * true if we worked, and false if we timed out.
 */
int socket_read(int socket, void *buffer, int size, int timeout) {
	int received;
	int readval;
	
	/* Keep reading until we have the whole message. */
	for(received=0; received < size;) {
		
		/* Wait until data becomes available. */
		if(!socket_wait_read(socket, timeout)) {
			debug(DEBUG_ACTION, "Socket read timed out.");
			return(0);
		}
		
		/* Read as much as we can. */
		readval=read(socket, ((char *) buffer) + received, size - received);

		if(readval == EINTR)
			continue;

		if(readval <= 0) {
			debug(DEBUG_ACTION, "Read in socket read failed.");
			return(0);
		}
		
		/* Update the number of bytes received. */
		received += readval;
	}
	
	/* We got it. */
	return(1);
}


/*
 * Read a line of text from a socket.
 *
 * The socket must be non-blocking, or the timeout is useless.  We return
 * number of bytes received if successfull, 0 if we timed out, or the socket was closed on us,
 * or negative if an error occured.
 */
int socket_read_line(int socket, char *buffer, int size, int timeout) {
	int received;
	int readval;
	char lf, ret;
	
	/* Keep reading until we have the whole message or we see a line feed or CR */
	for(received=0, lf = 0, ret = 0; received < size;) {
		
		/* Wait until data becomes available. */
		if(!socket_wait_read(socket, timeout)) {
			debug(DEBUG_ACTION, "Socket read timed out.");
			return(received);
		}
		
		/* Read as much as we can. */
		readval=read(socket, ((char *) buffer) + received, 1);

		if(readval == EINTR)
			continue;

		if(readval != 1) {
			return(readval);
		}
		
		if(buffer[received] == 0x0d){
			lf= 1;
		} 
		else if(buffer[received] == 0x0a){
			ret = 1;
		}
		else
			lf = ret = 0;

		if(lf & ret){
			buffer[received - 1] = 0;
			received -= 1;
			break;
		}

		/* Update the number of bytes received. */
		received++;


	}
	
	/* We got it. */
	return(received);
}




/*
 * Write to a socket.
 *
 * The socket must be non-blocking, or the timeout is useless.  We return
 * true if we worked, and false if we timed out.
 */
int socket_write(int socket, void *buffer, int size, int timeout) {
	int sent;
	int writeval;
	
	/* Keep writing until we have sent the whole message. */
	for(sent=0; sent < size;) {
		
		/* Wait until data becomes available. */
		if(!socket_wait_write(socket, timeout)) {
			debug(DEBUG_ACTION, "Socket write timed out.");
			return(0);
		}
		
		/* Write as much as we can. */
		writeval=write(socket, ((char *) buffer) + sent, size - sent);
		if(writeval <= 0) {
			debug(DEBUG_ACTION, "Write in socket write failed.");
			return(0);
		}
		
		/* Update the number of bytes sent. */
		sent += writeval;
	}
	
	/* We got it. */
	return(1);
}


/* 
 * Wait for a socket to become readable.
 *
 * If it didn't become readable in the ammount of time given, return false,
 * otherwise true.
 *
 * The timeout given is in milliseconds.  If it is -1, it is infinite.
 */
int socket_wait_read(int socket, int timeout) {
	fd_set read_fd_set;
	struct timeval tv;
	struct timeval *tvp;
	
	/* If the timeout is -1, we have no timeout. */
	if(timeout == -1) {
		tvp=NULL;
	}
	else {
		tvp=&tv;
	}
	
	/* Wait for data to be readable. */
	FD_ZERO(&read_fd_set);
	FD_SET(socket, &read_fd_set);
	tv.tv_sec=0;
	tv.tv_usec=timeout;
	if(select(socket+1, &read_fd_set, NULL, NULL, tvp)) {
		
		/* We got some data, return ok. */
		return(1);
	}
	
	else {
		
		/* We didn't get any data, this is a fail. */
		return(0);
	}
}

/* 
 * Wait for a socket to become writable.
 *
 * If it didn't become writable in the ammount of time given, return false,
 * otherwise true.
 *
 * The timeout given is in milliseconds.  If it is -1, it is infinite.
 */
 
int socket_wait_write(int socket, int timeout) {
	fd_set write_fd_set;
	struct timeval tv;
	struct timeval *tvp;
	
	/* If the timeout is -1, we have no timeout. */
	if(timeout == -1) {
		tvp=NULL;
	}
	else {
		tvp=&tv;
	}
	
	/* Wait for data to be readable. */
	FD_ZERO(&write_fd_set);
	FD_SET(socket, &write_fd_set);
	tv.tv_sec=0;
	tv.tv_usec=timeout;
	if(select(socket+1, NULL, &write_fd_set, NULL, tvp)) {
		
		/* We got the go ahead to write, return ok. */
		return(1);
	}
	
	else {
		
		/* We didn't get the go ahead to write, this is a fail. */
		return(0);
	}
}
