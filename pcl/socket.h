/*
 * Socket handling headers.
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
 * Steven Brown <swbrown@ucsd.edu>
 *
 * $Id: socket.h,v 1.2 1999/05/19 08:22:16 kefka Exp $
 */

#ifndef SOCKET_H
#define SOCKET_H


/* Prototypes. */
void socket_close(int socket);
int socket_create(char *socket_name, int mode, int uid, int gid);
int socket_create_listen(char *bindaddr, char *service, int protovers, int socktype, int (*addsock)(int sock, void *addr, int family, int socktype));
int socket_connect(char *socket_name);
int socket_connect_ip(char *host, char *service, int family, int socktype);
int socket_read(int socket, void *buffer, int size, int timeout);
int socket_read_line(int socket, char *line, int maxbuffer, int timeout);
int socket_write(int socket, void *buffer, int size, int timeout);
int socket_wait_read(int socket, int timeout);
int socket_wait_write(int socket, int timeout);

#endif
