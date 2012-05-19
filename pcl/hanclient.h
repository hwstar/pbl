/*
 * hanclient.h.  Client functions home automation network (HAN).
 *
 * Copyright (C) 1999 Stephen Rodgers
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
 * Stephen Rodgers <hwstar@cox.net>
 *
 * $Id$
 */
#ifndef HANCLIENT_H
#define HANCLIENT_H

void hanclient_send_command(Client_Command *client_command);
int hanclient_send_command_return_res(Client_Command *client_command);
void hanclient_error_check(Client_Command *client_command);
int hanclient_connect_setup(char *pidpath, char *sockfilepath, char *service, char *host);


#endif
