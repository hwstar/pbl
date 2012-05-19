/*
* pcl.c
*
* Copyright (C) 2010 Stephen A. Rodgers All Rights Reserved.
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

#define PCL_VERSION "1.0.0"


#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <assert.h>
#include "options.h"
#include "han.h"
#include "hanclient.h"
#include "serio.h"
#include "ihx.h"
#include "iniparser.h"
#include "error.h"


#define BOOT_VERSION_SUPPORTED 0		/* Boot version supported (must be greater or equal to boot loader version) */
#define MAX_PACKET 80				/* Maximum packet size */
#define LOADER_PAYLOAD 64			/* Loader payload in bytes (must match loader) */


/* Ascii Control Characters */
#define STX 		0x02
#define ETX		0x03
#define SUBST		0x04
#define	HDC		0xFF
#define	HDC_ACK		0xC1
#define	HDC_NAK		0x81
#define ENQ 		0x05
#define ACK 		0x06
#define NAK 		0x15


/* Boot Loader Commands */
#define BC_QUERY	0x00			/* Query command */
#define BC_CHECK_APP	0x08			/* Check app integrity on target */
#define BC_WRITE_EN	0x10			/* Write enable */
#define BC_WRITE_PM	0x40			/* Write program memory */
#define BC_EXEC_APP	0x55			/* Execute App */
#define BC_RESET	0xAA			/* Reset CPU */
#define BC_WRITE_EEPROM	0xA5			/* Write config memory */


#define PRODUCTID	0x2B36			/* Default Product ID */
#define	PROTOCOL	0			/* Protocol */

#define	PACKET_SIZE 80				/* Size of a packet */
#define MAX_PATH 128				/* Maximum path name length + 1 */
#define	MAX_CF 32				/* Config memory size in bytes */
#define PACKET_RETRIES 5			/* Number of retries to do when NAK is received on a packet */

// Buffer offsets for CRC and Signature in last row

#define SIGLO 0x30
#define SIGHI 0x32
#define RULO  0x34
#define RUHI  0x36
#define RFULO 0x38
#define RFUHI 0x3A
#define CRCLO 0x3C
#define CRCHI 0x3E

#define FAIL -1
#define PASS 0

/*
* Data Types
*/

typedef unsigned short u16;
typedef unsigned char u8;
typedef unsigned int u32;


struct packet_s_pbl {
	u8 id;
	u8 cmd;
	u16 param;
	u16 seq;
	u8 payload[LOADER_PAYLOAD];
	u16 crc16;
} __attribute__((__packed__));

typedef struct packet_s_pbl packet_t_pbl;

struct packet_s_han {
	u8 pkttype;
	u8 addr;
	u8 cmd;
	u16 param;
	u16 seq;
	u8 payload[LOADER_PAYLOAD];
	u8 pad[7];
	u16 crc16;
} __attribute__((__packed__));

typedef struct packet_s_han packet_t_han;

typedef union packet_u {
	u8 buffer[PACKET_SIZE];
	packet_t_pbl pbl;
	packet_t_han han;
} packet_t;

struct response_s {
	u16 lsize;
	u16 appsize;
	u16 prodid;
	u8  bootvers;
	u8  proto;
	u8  config[MAX_CF];
}__attribute__((__packed__)); 

typedef struct response_s response_t;

struct config_area_s {
	u16	user1;
	u16	user2;
	u16	user3;
	u16	user4;
	u16	resv1;
	u16	resv2;
	u16	deviceid;
	u16	config1;
	u16	config2;
	u16	cal1;
	u16	cal2;
}__attribute__((__packed__)); 


typedef struct config_area_s config_area_t;

typedef struct {
	int checkapp : 1;
	int execute : 1;
	int eeprom : 1;
	int verbose : 1;
	int reset : 1;
	int interrogateonly : 1;
	int hanmode : 1;
	int handisrunning : 1;
	int configfileoverride : 1;
} flags_t;

/*
* Global variables
*/

// For error.c's benefit
char *progname;
int debuglvl = DEBUG_UNEXPECTED;
static u16 hannodeaddr;
static flags_t flags;
static Client_Command client_command;
static packet_t packet;
static u16 productid = PRODUCTID;
static u8 packet_size;

/* Commandline options. */

#define SHORT_OPTIONS "a:cd:ef:hio:p:rvVxz:"

static struct option long_options[] = {
  {"address", 1, 0, 'a'},
  {"check-after-programming", 0, 0, 'c'},
  {"debug", 1, 0, 'd'},
  {"eeprom", 0, 0, 'e'},
  {"file", 1, 0, 'f'},
  {"help", 0, 0, 'h'},
  {"interrogate-only", 0, 0, 'i'},
  {"product-id", 1, 0, 'o'},
  {"port", 1, 0, 'p'},
  {"reset", 0, 0, 'r'},
  {"verbose", 0, 0, 'v'},
  {"version", 0, 0, 'V'},
  {"execute-after-programming", 0, 0, 'x'},
  {"config-file", 1, 0, 'z'},
  {0, 0, 0, 0}
};
 
static char file[MAX_PATH];
static char port[MAX_PATH];
static char service[MAX_PATH] = "1128";
static char host[MAX_PATH] = "::1";
static char config_file[MAX_PATH] = "pcl.conf";

static char *not_comp = "Hand not compatible with pcl";

/*
* Start of code
*/


/* Hex dump */
void hex_dump(void *buf, u32 size, u8 printaddr)
{
	u32 i;
	if(printaddr)
		printf("0000: ");
	for(i = 0 ; i < size ; i++){
		printf("%02X", ((u8 *)buf)[i]);
		if((i % 16) == 15){
			printf("\n");
			if(printaddr && (i != (size - 1)))
				printf("%04X: ", i + 1);
		}
		else
			printf(" ");
	}
}


/* Calculate CRC over buffer using polynomial: X^16 + X^12 + X^5 + 1 */

static u16  do_crc(u16 crcin, void *buf, u16 len)
{
	u8 i;
	u16 crc = crcin;
	u8 *b = (u8 *) buf;

	while(len--){
		crc ^= (((u16) *b++) << 8);
		for ( i = 0 ; i < 8 ; ++i ){
			if (crc & 0x8000)
				crc = (crc << 1) ^ 0x1021;
			else
				crc <<= 1;
          	}
	}
	return crc;
}


/* Transmit a packet */

static int packet_tx(serioStuff *s, void *p, size_t size, int timeout)
{
	if(!flags.hanmode){
		return serio_write(s, p, size, timeout);
	}
	else{
		int res;
		int xfrcount;
		static u8 ctrlchars[3] = {SUBST,STX,ETX};

		res = serio_write(s, ctrlchars + 1, 1, timeout);

		for(xfrcount = 0; (res == 1) && (xfrcount < size); xfrcount++){
			if(((u8 *)p)[xfrcount] <= SUBST){
				res = serio_write(s, ctrlchars, 1, timeout); /* Write SUBST */
			}
			if(res == 1){
				res = serio_write(s, ((u8 *) p) + xfrcount, 1, timeout);
			}
		}
		if(res == 1){
			res = serio_write(s, ctrlchars + 2, 1, timeout);
		}	
		return (res < 0) ? res : xfrcount;
	}

}

/* Receive a packet */

static int packet_rx(serioStuff *s, void *p, size_t size, int timeout)
{

	if(!flags.hanmode){
		return  serio_read(s, p, size, timeout);

	}
	else{
		int res;
		u8 b[2];
		int xfrcount = 0;

		do{
			res = serio_read(s, b, 1, timeout);
		}
		while((res == 1) && (b[0] != STX));


		for(xfrcount = 0; (res == 1); xfrcount++){
			res = serio_read(s, b, 1, timeout);

			if(res != 1){
				break;
			}

			if(b[0] == ETX){
				break;
			}

			else if(b[0] == SUBST){
				res = serio_read(s, b, 1, timeout);

				if(res != 1){
					break;
				}
			}
			if(xfrcount < size)
				((u8 *) p)[xfrcount] = b[0];
		}
		return (res < 0) ? res : xfrcount;
	}

}


/* Zero out the packet buffer */

static void packet_init(void)
{
	memset(packet.buffer, 0, sizeof(packet_t));
	if(!flags.hanmode){
		packet.pbl.id = STX;
	}
}

/* Calculate CRC over the packet buffer minus the size of an u16 */

static void packet_finalize(void)
{	
	u16 crc16;
	crc16 = do_crc(0, packet.buffer, packet_size - sizeof(u16));
	if(flags.hanmode){
		packet.han.crc16 = crc16;
	}
	else{
		packet.pbl.crc16 = crc16;
	}
	
}


/*
* Expand a packet by inserting STX, ETX and SUBST chars where necessary
*/

static int packet_format(void *dest, void *src, int count)
{
	int i;
	int res = 0;

	((u8 *)dest)[res++] = STX;

	for(i = 0; i < count; i++){
		if(((u8 *)src)[i] <= SUBST)
			((u8 *)dest)[res++] = SUBST;
		((u8 *)dest)[res++] = ((u8 *)src)[i];
	}

	((u8 * )dest)[res++] = ETX;

	return res;
}

/*
* Unformat the packet return the length of the unformatted packet
*/

static int packet_unformat(void *dest, void *src, int count)
{
	int i,len;
	char subst,gotstx;

	for(len = 0, i = 0, subst = 0, gotstx = 0; i < count; i++){
		if(!gotstx){
			if(((u8 *)src)[i] == STX)
				gotstx = 1;
			continue;
		}
		if(!subst){
			if(((u8 *)src)[i] == ETX)
				break;
			else if( ((u8 *)src)[i] == SUBST)
				subst = 1;
			else
				((u8 *)dest)[len++] =  ((u8 *)src)[i];
		}
		else{
			subst = 0;
			((u8 *)dest)[len++] = ((u8 *)src)[i];
		}
	}
	if((gotstx) && (((u8 *)src)[i] == ETX))
		return len;
	else
		return 0;
}




/* Send a command packet */
/* Note: Payload can be NULL if there is no payload to transmit */

static int send_command(serioStuff *s, u8 cmd, u16 param, void *payload)
{
	int retries, res;
	int bytes_sent, bytes_received;
	u8 ack,nak;
	unsigned char resp = 0x55; 
	static u16 seqno = 0;


	packet_init();

	if(flags.hanmode){
		packet.han.pkttype = HDC;
		packet.han.addr = (u8) hannodeaddr; 
		packet.han.cmd = cmd;
		packet.han.param = param;
		packet.han.seq = seqno;
		ack = HDC_ACK;
		nak = HDC_NAK;
		
	}
	else{
		packet.pbl.cmd = cmd;
		packet.pbl.param = param;
		packet.pbl.seq = seqno;
		ack = ACK;
		nak = NAK;
	}
	if(payload)
		memcpy((flags.hanmode) ? packet.han.payload : packet.pbl.payload, payload, LOADER_PAYLOAD);
	packet_finalize();
	debug(DEBUG_ACTION,"Command: 0x%02X Sequence Number: %d, CRC: 0x%04X", cmd, seqno, (flags.hanmode)? packet.han.crc16 : packet.pbl.crc16);

	if(!flags.handisrunning){ // Hand not running?
		serio_flush_input(s);
		for(retries = PACKET_RETRIES;;){
			if((bytes_sent = packet_tx(s, packet.buffer, packet_size, 5000000)) < 0){
				debug(DEBUG_ACTION, "Command Packet Write Error");
				return FAIL;
			}
			debug(DEBUG_ACTION, "Bytes Sent: %d", bytes_sent);
			if(bytes_sent != packet_size){
				debug(DEBUG_ACTION, "Command Packet Write Incomplete");
				return FAIL;
			}

			for(;;){
				bytes_received = serio_read(s, &resp, 1, 5000000);
				if(bytes_received < 0){
					if(errno != EAGAIN){
						debug(DEBUG_UNEXPECTED, "Response Read Error: %s", strerror(errno));
						return FAIL;
					}
					else{
						continue;
					}
				}
				else{
					break;
				}
			}
	
			if(bytes_received != 1){
				debug(DEBUG_ACTION, "Read Timeout Error");
				return FAIL;
			}

			if((resp == ack)||(resp == nak)){
				debug(DEBUG_ACTION, "Response: %s",(resp == ack)?"ACK":"NAK");
			}
			else{
				debug(DEBUG_ACTION, "Response: 0x%02X", (unsigned int) resp);
			}

			if((cmd == BC_CHECK_APP) && (resp == STX)){
				debug(DEBUG_UNEXPECTED, "Check App Failed");
				return FAIL;
			}

			if((resp != ack)){				
				debug(DEBUG_ACTION, "Did not get ACK");
				if((resp == nak) && (retries--)){
					debug(DEBUG_UNEXPECTED, "***Retrying packet***");
					usleep(100000);
					continue;
				}
				return FAIL; // Retries used up or didn't get ACK or NAK. Fail.
			}
			break;
		}
	}
	else{ // Hand is running
		for(retries = 0; retries < PACKET_RETRIES; retries++){
			client_command.cmd.raw.txlen = packet_format(client_command.cmd.raw.txbuffer, &packet.han, packet_size);
			client_command.cmd.raw.rxexpectlen = 1;
			client_command.cmd.raw.txtimeout = 100000;
			client_command.cmd.raw.rxtimeout = 1000000;
			client_command.request = HAN_CCMD_RAW_PACKET;
			res = hanclient_send_command_return_res(&client_command);
			bytes_received = 0;
			if(!res)
				bytes_received = client_command.cmd.raw.rxexpectlen;
			if(bytes_received == 1){
				resp = client_command.cmd.raw.rxbuffer[0];
				if((resp == ack)||(resp == nak)){
					debug(DEBUG_ACTION, "Response: %s",(resp == ack)?"ACK":"NAK");
				}
				else{
					debug(DEBUG_ACTION, "Response: 0x%02X", (unsigned int) resp);
				}

				if((cmd == BC_CHECK_APP) && (resp == STX)){
					debug(DEBUG_UNEXPECTED, "Check App Failed");
					return FAIL;
				}

				if((resp != ack)){				
					debug(DEBUG_ACTION, "Did not get ACK");
					if(resp == nak){
						debug(DEBUG_UNEXPECTED, "***Retrying packet***, try = %d", retries);
						usleep(100000);
						continue;
					}
				}
				else
					break; // Success
			}
			debug(DEBUG_UNEXPECTED,"Received invalid or no response, try = %d", retries);
			usleep(100000);
		}
		if(retries == PACKET_RETRIES){
			debug(DEBUG_EXPECTED, "Too many packet retries!");
			return FAIL;
		}
	}

	seqno++;
	return PASS;
}
	
/* Calculate and check the packet CRC */

static int packet_check()
{
	u16	rcrc16, crc16 =  do_crc(0, (u8 *) packet.buffer, packet_size - sizeof(u16));

	rcrc16 = (flags.hanmode) ? packet.han.crc16 : packet.pbl.crc16;

	debug(DEBUG_ACTION, "Rx CRC: 0x%04X, Calc CRC 0x%04X", rcrc16, crc16);

	if(crc16 == rcrc16)
		return PASS;
	else
		return FAIL;
		
}

static void show_help(void)
{
	printf("\n");
	printf("--address, -a                          : Specify han node address\n");
	printf("--check_after_programming, -c          : Check CRC of app on target after programming\n");
	printf("--debug, -d                            : Set debug level (0-5). Used to to find bugs\n");
	printf("--eeprom, -e                           : Write to eeprom instead of program memory\n");
	printf("--file, -f path/to/file.hex            : Specify .hex or .bin file name\n");
	printf("--help, -h                             : Prints this text\n");
	printf("--interrogate-only, -i                 : Interrograte boot loader on target and exit\n");
	printf("--product-id, -o                       : Specify 16 bit product ID in hexadecimal\n");
	printf("--port, -p pathtoport                  : Specify path name to port node\n");
	printf("--reset, -r                            : Reset target after programming\n");
	printf("--verbose, -v                          : Print out additional info during use\n");
	printf("--version, -V                          : Print version and exit\n");
	printf("--execute, -x			       : Check app for integrity then execute it\n");
	printf("--config-file, -z                      : Specify config file\n");
	printf("\n");
	printf("Examples:\n");
	printf("pcl -p /dev/ttyUSB1 -f app.hex         : Basic programming command\n");
	printf("pcl -x -p /dev/ttyUSB1 -f app.hex      : Program and then execute\n");
	printf("pcl -c -p /dev/ttyUSB1 -f app.bin      : Program and then check\n");
	printf("pcl -e -p /dev/ttyUSB1 -f eeprom.hex   : Program eeprom\n");
	printf("pcl -e -p /dev/ttyUSB1 -f eeprom.bin   : Program eeprom\n");
	printf("pcl -i -p /dev/ttyUSB1                 : Interrogate only\n");
	printf("pcl -x -p /dev/ttyUSB1                 : Check app and start it\n");
	printf("pcl -a 1 -x -z pclr.conf               : Program HAN node at address 1 using config file\n");
	printf("\n");
}



/*
* Top level
*/

int main(int argc, char *argv[])
{
	char optchar;
	int longindex, i,res, crcerr;
	u8 writecmd;
	u32 bufbytepos;
	u8 *buffer,*lastrow;
	u16 wordaddr;
	u16 rowsexceptlast, toprow;
	u16 crc16;
	u16 load_size, load_size_bytes, load_address;
	u16 bootloader_size;
	u32 max_app_size;
	u32 bytes_received, bytes_sent;
	ihx_t *ihx;
	response_t *r;
	config_area_t *cf;
	serioStuff *s;
	dictionary *dict = NULL;
	static char exten[20];
	char *q;

	/* Die if packet structures screwed up */
	assert(sizeof(packet_t) == PACKET_SIZE);
		
	progname = argv[0];

	// Argument pre scan for config file name change first

	while((optchar=getopt_long(argc, argv, SHORT_OPTIONS, long_options, &longindex)) != EOF) {
		
		//Handle each argument. 
		switch(optchar) {
			
			// Was it a long option? 
			case 0:	
				// Hrmm, something we don't know about? 
				fatal("Unhandled long getopt option '%s'", long_options[longindex].name);
			
			// If it was an error, exit right here. 
			case '?':
				exit(1);

			// Was it a config file name? 
			case 'z':
				memset(config_file, 0, MAX_PATH);
				strncpy(config_file, optarg, MAX_PATH);
				flags.configfileoverride = 1;
				break;

			default: // Don't care about the rest of the args right now
				break;	
		}
	}
	optind = 1; // Reset option index for second scan later


	if((dict = iniparser_load(config_file))){
		char *s;
		s = iniparser_getstring(dict, "general:file", NULL);
		if(s)
			strncpy(file, s, MAX_PATH - 1);
		s = iniparser_getstring(dict, "general:port", NULL);
		if(s)
			strncpy(port, s, MAX_PATH - 1);
		s = iniparser_getstring(dict, "general:han-host",NULL);
		if(s)
			strncpy(host, s, MAX_PATH - 1);
		s = iniparser_getstring(dict, "general:han-service",NULL);
		if(s)
			strncpy(service, s, MAX_PATH - 1);
		s = iniparser_getstring(dict, "general:product-id", NULL);
		if(s){
			if(sscanf(s, "%X", &i) != 1)
				fatal("In pcl.conf, product ID needs to be a hexadecimal value");
			productid = (u16) i;
		}
		iniparser_freedict(dict);
	}
	else if(flags.configfileoverride){
		fatal("Cannot open config file: %s\n", config_file);
	}


	/* Parse the arguments. */
	while((optchar=getopt_long(argc, argv, SHORT_OPTIONS, long_options, &longindex)) != EOF) {
		
		/* Handle each argument. */
		switch(optchar) {
			
			/* Was it a long option? */
			case 0:
				
				/* Hrmm, something we don't know about? */
				fatal("Unhandled long getopt option '%s'", long_options[longindex].name);
			
			/* If it was an error, exit right here. */
			case '?':
				exit(1);

			/* Was it han node address request? */
			case 'a':
				if(sscanf(optarg, "%hx", &hannodeaddr) != 1)
					fatal("Invalid node address");
				if(hannodeaddr > 32)
					fatal("Node address out of range");
				flags.hanmode = 1;
				break;	


			/* Was it a check app request? */
			case 'c':
				flags.checkapp = 1;
				break;	

			/* Was it a debug level set? */
			case 'd':

				/* Save the value. */
				debuglvl=strtol(optarg, NULL, 10);
				if((debuglvl < 0 || debuglvl > DEBUG_MAX)) {
					fatal("Invalid debug level");
				}
				break;

			case 'e':
				flags.eeprom = 1;
				break;


			/* Was it a file request? */
			case 'f': 
				memset(file, 0, MAX_PATH);
				strncpy(file, optarg, MAX_PATH);
				break;
			
			/* Was it a help request? */
			case 'h':
				show_help();
				exit(0);

			case 'i':
				flags.interrogateonly = 1;
				break;

			case 'o':
				if(sscanf(optarg, "%X", &i) != 1)
					fatal("Product ID needs hexadecimal value");
				productid = (u16) i;
				break;

			/* Was it a port request? */
			case 'p': 
				memset(port, 0, MAX_PATH);
				strncpy(port, optarg, MAX_PATH);
				break;

			/* Was it a reset request */
			case 'r':
				flags.reset = 1;
				break;	

			/* Was it a verbose request? */
			case 'v':
				flags.verbose = 1;
				break;	

			case 'V':
				printf("PCL version %s\n", PCL_VERSION);
				exit(0);

			/* Was it Execute App ? */
			case 'x':
				flags.execute = 1;
				break;

			case 'z': /* handled previously */
				break;

			/* It was something weird.. */
			default:
				panic("Unhandled getopt return value %c", optchar);
		}
	}
	
	/* If there were any extra arguments, we should complain. */

	if(optind < argc) {
		fatal("Extra argument on commandline, '%s'", argv[optind]);
	}

	if(flags.eeprom && (flags.execute | flags.checkapp))
		fatal("-e is not valid with -x or -c");

	if(!(flags.interrogateonly | flags.execute | flags.checkapp | flags.eeprom))
		fatal("What do you want me to do, anyhow? Must specify -e, -c, -i, or -x");

	r = (response_t *) ((flags.hanmode) ? packet.han.payload : packet.pbl.payload);
	cf = (config_area_t *) r->config;


	// Send a query packet
	packet_init();

	if(flags.hanmode){ // If HAN address specified
		res  = hanclient_connect_setup("","", service, host);
		if(!res){ // if connect setup OK
			printf("Test for hand running\n"); 
			client_command.request = HAN_CCMD_DAEMON_INFO;
			res =  hanclient_send_command_return_res(&client_command);
			printf("Hand %s running\n", (res) ? "is not" : "is");

			if(!res){ // If hand is loaded, send boot loader entry command 
 		               	debug(DEBUG_EXPECTED,"Hand version: %s", client_command.cmd.info.version);
				// Check for structure size match between us and han daemon
                		if(client_command.cmd.info.cmdpktsize != sizeof(struct han_packet)){
                        		debug(DEBUG_UNEXPECTED, "Hand command structure incompatible");
                        		fatal(not_comp);
                		}
            		    	if(client_command.cmd.info.rawsize != sizeof(struct han_raw)){
                        		debug(DEBUG_UNEXPECTED, "Hand raw command structure incompatible");
                        		fatal(not_comp);
                		}

				flags.handisrunning = 1; // Set flag indicating comm is going to go through hand
				memset(&client_command,0,sizeof(Client_Command)); // Send boot loader entry command
				client_command.request = HAN_CCMD_SENDPKT;
				client_command.cmd.pkt.nodecommand = HAN_CMD_GEBL; 
				client_command.cmd.pkt.numnodeparams = 2;
				client_command.cmd.pkt.nodeparams[0] = 0x55;
				client_command.cmd.pkt.nodeparams[1] = 0xAA;
				client_command.cmd.pkt.nodeaddress = hannodeaddr;
				printf("Sending boot loader entry request\n");
				res = hanclient_send_command_return_res(&client_command);
				res = res | client_command.commstatus;
				printf("Boot loader %s entered via HAN command\n", (res) ? "not" : "was");
				if(!res){
					printf("Waiting for boot loader to initialize...");
					fflush(stdout);
					usleep(3000000); // Wait for boot loader to activate 
					printf("OK\n");
				}
			}
		}
		// Set up packet parameters for addressable mode
		packet.han.param = 0x55AA; // Not required by protocol
		packet.han.pkttype = HDC;
		packet.han.addr = (u8) hannodeaddr;
		packet_size = sizeof(packet_t);
	}
	else{ // Non-addressable operating mode
		packet.pbl.param = 0x55AA; // Not required by protocol
		packet_size = sizeof(packet_t_pbl);
	}


	if(!flags.handisrunning){ // If not going through hand
		if(!port[0])
			fatal("Missing port (-p) option on command line or config file");

		if(!(s = serio_open(port, (flags.hanmode) ? 9600 : 57600)))
			fatal("Can't open serial port %s\n", port);

		serio_flush_input(s);
		packet_finalize();
		debug(DEBUG_ACTION, "Transmit Packet CRC: 0x%04X", (flags.hanmode) ? packet.han.crc16 : packet.pbl.crc16);
		if((bytes_sent = packet_tx(s, packet.buffer, packet_size, 5000000)) < 0)
			fatal("Packet write error");
		debug(DEBUG_ACTION, "Bytes Sent: %d", bytes_sent);

		if(bytes_sent != packet_size)
			fatal("Packet write incomplete");


		packet_init(); // Just to be sure we get something


		// Wait for response
		bytes_received = packet_rx(s, packet.buffer, packet_size, 5000000);
		debug(DEBUG_ACTION, "Bytes Received: %d", bytes_received);
		if(bytes_received < 0)
			fatal("Packet read error");
		if((bytes_received != packet_size)) // Must see a packet, not just an ACK
			fatal("Packet read incomplete, received %d bytes", bytes_received);
		if(packet_check())
			fatal("Packet CRC error");


	}
	else{ // Send packets through hand */

		debug(DEBUG_ACTION,"Sending packets through hand");
		packet_finalize();
		debug(DEBUG_ACTION, "Transmit Packet CRC: 0x%04X", (flags.hanmode) ? packet.han.crc16 : packet.pbl.crc16);
		for(i = 0; i < PACKET_RETRIES; i++){
			client_command.cmd.raw.txlen = packet_format(client_command.cmd.raw.txbuffer, &packet.han, packet_size);
			client_command.cmd.raw.rxexpectlen = 255;
			client_command.cmd.raw.txtimeout = 100000;
			client_command.cmd.raw.rxtimeout = 500000;
			client_command.request = HAN_CCMD_RAW_PACKET;
			res = hanclient_send_command_return_res(&client_command);
			if((!res) && (client_command.cmd.raw.rxexpectlen))
				bytes_received = packet_unformat(packet.buffer, client_command.cmd.raw.rxbuffer, 255);
			else
				bytes_received = 0;
			if(bytes_received)
				crcerr = packet_check();
			else
				crcerr = 0;
			if((bytes_received == packet_size) && (!crcerr)) // Must see a packet with a good CRC, not just an ACK
				break;
			debug(DEBUG_UNEXPECTED,"Query packet receive error, try = %d", i);
			debug(DEBUG_UNEXPECTED,"bytes_received = %d crcerr = %d", bytes_received, crcerr);
		}
		if(i == PACKET_RETRIES)
			fatal("Too many packet retries!");
	}

	if(flags.verbose || flags.interrogateonly){
		printf("Loader Size in Words: 0x%04X\n", r->lsize);
		printf("App. Size In Words  : 0x%04X\n", r->appsize);
		printf("Product ID          : 0x%04X\n", r->prodid); 
		printf("Boot Program Version: 0x%02X\n", r->bootvers);
		printf("Protocol Number     : 0x%02X\n", r->proto);
		printf("Device User 1       : 0x%04X\n", cf->user1);
		printf("Device User 2       : 0x%04X\n", cf->user2);
		printf("Device User 3       : 0x%04X\n", cf->user3);
		printf("Device User 4       : 0x%04X\n", cf->user4);
		printf("Device ID           : 0x%04X\n", cf->deviceid);
		printf("Device Config 1     : 0x%04X\n", cf->config1);
		printf("Device Config 2     : 0x%04X\n", cf->config2);
	}

	if((flags.execute) && (!file[0])){ /* Special case for execute without load */
		printf("Check App: ");	
		if(send_command(s, BC_CHECK_APP, 0, NULL)){
			printf("FAILED\n");
			exit(1);
		}
		else
			printf("PASSED\n");
		printf("Executing App... ");
		if(send_command(s, BC_EXEC_APP, 0, NULL)){
			printf("FAILED\n");
			exit(1);
		}
		printf("\n");
		exit(0);
	}
	
	if(flags.interrogateonly) /* If interrogate only, exit now */
		exit(0);

	if(r->prodid != productid)
		fatal("Wrong product ID: specified: %04X, device reports: %04X", productid, r->prodid); 


	if(r->bootvers > BOOT_VERSION_SUPPORTED)
		fatal("Do not know how to deal with bootversion %d\n", r->bootvers);

	if(r->proto)
		fatal("Does not support protocol version %d\n", r->proto);

	max_app_size = r->appsize;
	bootloader_size = r->lsize;

	if(!file[0])
		fatal("Missing file (-f) option on command line");

	// Allocate buffer
	if(!(buffer = malloc(max_app_size << 1)))
		fatal("No memory for buffer");

	if(!flags.eeprom){
		/* Fill buffer with erase pattern */	
		for(i = 0 ; i < max_app_size << 1; i++)
			buffer[i] = (i & 1) ? 0x3F : 0xFF;
	}

	/* Locate the extension if it exists */
	for(i = strlen(file), q = NULL; i >= 0 ; i--){
		if(file[i] == '/'){
			break;
		}
		if(file[i] == '.'){
			q = file + i + 1;
			break;
		}
	}

	if(q)
		strncpy(exten, q, 18);

	if(!strcmp(exten, "hex")){
		/* Hex files */
		if(!(ihx = ihx_read( file, buffer, max_app_size << 1)))
			fatal("Could not open and/or read hex file");

		load_address = ihx->load_address >> 1;
		load_size = ihx->size >> 1;
		load_size_bytes = ihx->size;
		ihx_free(ihx);
	}
	else if(!strcmp(exten, "bin")){
		/* Binary files */
		int fh, br;

		if((fh = open(file, O_RDONLY, 0)) < 0)
			fatal("Could not open bin file: %s", file);

		if((br = read(fh, buffer, max_app_size << 1)) < 0)
			fatal("Could not read bin file: %s", file);

		if(close(fh) < 0)
			fatal("Could not close bin file");
		load_size = br >> 1;
		load_size_bytes = br;
		load_address = bootloader_size;
	}
	else{
		if(strlen(exten))
			fatal("Unrecognizable file format %s", exten);
		else
			fatal("Missing file extension");
	}

	if(flags.eeprom){
		writecmd = BC_WRITE_EEPROM;
		load_address = 0;
		rowsexceptlast = 4; // For pic 16F193X
		if(load_size_bytes != 256)
			fatal("EEPROM hex file must contain exactly 256 bytes");
	}
	else{
		writecmd = BC_WRITE_PM;
		rowsexceptlast = load_size_bytes / LOADER_PAYLOAD;
		if(load_size_bytes % LOADER_PAYLOAD)
			rowsexceptlast++;
		toprow = ((max_app_size << 1) - LOADER_PAYLOAD) / LOADER_PAYLOAD;

		debug(DEBUG_ACTION,"Actual App Size in Words: %u", load_size);
		debug(DEBUG_ACTION,"App Load Word Address: 0x%04X", load_address);
		debug(DEBUG_ACTION, "rowsexceptlast = %u", rowsexceptlast);
		debug(DEBUG_ACTION, "toprow = %u\n", toprow);
	}
	

	if(!flags.eeprom){
		// Calculate CRC on all rows in buffer except the last one
		crc16 = do_crc(0, buffer, rowsexceptlast * LOADER_PAYLOAD);

		debug(DEBUG_ACTION,"App CRC: 0x%04X", crc16);

		if(load_address != bootloader_size)
			fatal("Wrong App Load Address");


		// Write the info bytes into the last 16 bits of the top row.
		lastrow = buffer + ((max_app_size << 1) - LOADER_PAYLOAD);
		lastrow[SIGLO] = 0xAA;
		lastrow[SIGLO + 1] = 0; // 6 bit locations are useless, zero them out for readability.
		lastrow[SIGHI] = 0x55;
		lastrow[SIGHI + 1] = 0;
		lastrow[RULO] = (u8) rowsexceptlast;
		lastrow[RULO + 1] = 0;
		lastrow[RUHI] = (u8) (rowsexceptlast >> 8);
		lastrow[RUHI + 1] = 0;
		lastrow[RFULO] = 0;
		lastrow[RFULO + 1] = 0;
		lastrow[RFUHI] = 0;
		lastrow[RFUHI + 1] = 0;
		lastrow[CRCLO] = (u8) crc16;
		lastrow[CRCLO + 1] = 0;
		lastrow[CRCHI] = (u8) (crc16 >> 8);
		lastrow[CRCHI + 1] = 0;
	}

	debug(DEBUG_ACTION, "Sending Write Enable Command");


	if(send_command(s, BC_WRITE_EN, 0, NULL))
		fatal("Write Enable Command Failed");


	if(debuglvl > DEBUG_ACTION)
		hex_dump(buffer, load_size_bytes, 1);


	/* Write program memory or eeprom */

	for(i = 0 ; i < rowsexceptlast; i++){
		bufbytepos = i * LOADER_PAYLOAD;
		wordaddr = (bufbytepos >> 1) + load_address;
		debug(DEBUG_ACTION, "wordaddr: 0x%04X, bufbytepos: 0x%04X", wordaddr, bufbytepos);
		if(send_command(s, writecmd, wordaddr, buffer + bufbytepos))
			fatal("\nWrite Program Memory Failed");
		if(debuglvl == DEBUG_UNEXPECTED){
			printf(".");
			if((i & 63) == 63)
				printf("\n");
			fflush(stdout);
		}


	}
	if(debuglvl == DEBUG_UNEXPECTED)
		printf("\n");

	/* Write the top row if not eeprom and the top row stands alone from the app */
	if((!flags.eeprom) && (rowsexceptlast < toprow)){
		debug(DEBUG_ACTION, "Gap between app. end and top row, writing top row ");
		bufbytepos = toprow * LOADER_PAYLOAD;
		wordaddr = (bufbytepos >> 1) + load_address;
		debug(DEBUG_ACTION, "wordaddr: 0x%04X, bufbytepos: 0x%04X", wordaddr, bufbytepos);
		if(send_command(s, BC_WRITE_PM, wordaddr, buffer + bufbytepos))
			fatal("Write Program Memory Failed");
	}

	if((flags.checkapp)||(flags.execute)){
		printf("Check App: ");	
		if(send_command(s, BC_CHECK_APP, 0, NULL)){
			printf("FAILED\n");
			exit(1);
		}
		else
			printf("PASSED\n");
	}
	if(flags.execute){
		printf("Executing App... ");
		if(send_command(s, BC_EXEC_APP, 0, NULL)){
			printf("FAILED\n");
			exit(1);
		}
		printf("\n");
	}		
	else if(flags.reset){
		printf("Resetting MPU... ");
		if(send_command(s, BC_RESET, 0, NULL)){
			printf("FAILED\n");
			exit(1);
		}
		printf("\n");
	}		
	printf("DONE\n");
	free(buffer);		
	exit(0);
}

