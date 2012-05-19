/*
 * HAN headers and prototypes.
 *
 * Copyright (C) 1999  Stephen Rodgers
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
 * Stephen Rodgers <hwstar@rodgers.sdcoxmail.com>
 *
 * $Id$
 */

#ifndef HAN_H
#define HAN_H

/* Typedefs. */
typedef struct client_command Client_Command;
typedef struct han_packet Han_Packet;
typedef struct han_netscan Han_Netscan;
typedef struct han_watch_entry Han_Watch_Entry;
typedef struct err_stats Err_Stats;
typedef struct hand_info Hand_Info;

/* Generic node commands */

#define HAN_CMD_NOOP	0	// No operation, 0-MAX parms reqd
#define HAN_CMD_NODEID	1	// Return node id and other info, 4 parms reqd.
#define HAN_CMD_GCST	2	// Return error information, 3 parms reqd.
#define HAN_CMD_GEBL	0x0F	// Enter boot loader


/* Requests the client can make. */

#define HAN_CCMD_SENDPKT 0
#define HAN_CCMD_NETSCAN 1
#define HAN_CCMD_NETSTATS 2
#define HAN_CCMD_NETSTATSCLR 3
#define HAN_CCMD_DAEMON_INFO 4
#define	HAN_CCMD_RAW_PACKET 5
#define HAN_CCMD_PPOWER_COMMAND 0x1000

/* Communication status codes */

#define HAN_CSTS_OK 0
#define HAN_CSTS_CMD_UNKNOWN -1
#define HAN_CSTS_RX_TIMEOUT -2
#define HAN_CSTS_TX_TIMEOUT -3
#define HAN_CSTS_CRC_ERROR -4
#define HAN_CSTS_NAK_ERROR -5
#define HAN_CSTS_FORMAT_ERROR -6
#define HAN_CSTS_FRAMING_ERROR -7
#define HAN_CSTS_NO_SUCH_WATCH -8
#define HAN_CSTS_PPOWER_ERROR -9
#define HAN_CSTS_PPOWER_CONFIG_ERROR -10
#define HAN_CSTS_PPOWER_FORK_ERROR -11
#define HAN_CSTS_INVPARM -14


/* PPOWER definitions */

#define HAN_PPOWER_REQ_CMD 0


/* node info structure. This is used by the netscan command */

struct  node_info {
	unsigned char addr;
	unsigned type;
	unsigned fwlevel;
};


/* Error status structure. Used to track error rates */


struct err_stats {
	unsigned round_trips;
	unsigned spurious_packets;
	unsigned rx_timeouts;
	unsigned tx_timeouts;
	unsigned crc_errs;
};

struct hand_info {
	short int	handinfosize;
	short int	cmdpktsize;
	short int	netscanpktsize;
	short int	errstatssize;
	short int	ppowersize;
	short int	rawsize;
	short int	pad[42];
	char		version[32];

};
	

/* Netscan structure. Used by the netscan command */

struct  han_netscan {
	unsigned char numnodesfound;
	struct node_info nodelist[256];
};

/* Packet structure. Used by REQUEST_COMMAND */
 
struct  han_packet {
	unsigned char nodeaddress;
	unsigned char nodecommand;
	unsigned char nodeparams[MAX_NODE_PARAMS];
	unsigned char numnodeparams;
	unsigned char nodestatus[MAX_NODE_PARAMS];
};


/* PPower client packet structure. Used to send ppower commands over HAN TCP connections */

struct ppower_client_command {
  char command_string[64];
};

/* Raw packet structure */

struct han_raw {
	unsigned txtimeout;
	unsigned rxtimeout;
	unsigned char txlen;
	unsigned char rxexpectlen;
	unsigned char txbuffer[255];
	unsigned char rxbuffer[255];
};


/* Union to merge all of the command formats together */

union han_command {
	struct han_packet pkt;
	struct han_netscan scan;
	struct err_stats stats;
	struct hand_info info;
 	struct ppower_client_command ppower_cmd;
	struct han_raw raw;
};


/* Client command structure.  Sent to the daemon on a command socket. */

struct client_command {
	int request;
	int commstatus;
	union han_command cmd;
}; 



#endif
	
