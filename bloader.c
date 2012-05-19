/*
* bloader.c
*
* Copyright (C) 2011 Stephen A. Rodgers, All Rights Reserved
*
*/

/*
* This file is part of the HAN (HOME AUTOMATION NETWORK) Project
*
*   HAN is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 2 of the License, or
*   (at your option) any later version.

*   HAN is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with HAN.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
*
* Boot loader program for the PIC16F193x series of MPU's. Works in concert with the pcl linux PC command line
* application, and the pclw Windows GUI and Linux GUI applications.
*
* The current size of the boot loader is just under 1024 words. If additional functionallity needs to be added
* the boot loader will get bigger and the LOADER_SIZE variable will need to be increased. Note: there are statements
* to force an error if LOADER_SIZE is exceeded, so if you get an "out of rom" error you will need to increase the
* LOADER_SIZE variable to the next larger integer multiple of 256. If the loader size or total program memory
* values are changed, they need to be changed in both bloader.c and bootapp.h, and both files must match.
*
* bloader runs first at boot time, and checks to see if there is a valid application (app.) loaded in the application area.
* If there is, then control is transferred to the app. If not, then control will remain with the boot loader.
*
* Packets sent by the PC contain an STX character in the first byte, followed by an address, command,parameter, sequence number,
* a byte array of size LOADER_PAYLOAD, and finally a 16 bit CRC of the entire packet. LOADER_PAYLOAD has been chosen to
* be equal to the flash row size of a 16F19XX series part. Other parts may need a different value for LOADER_PAYLOAD. If
* LOADER_PAYLOAD has to be increased, then LOADER_BUFSIZE has to be adjusted by a like amount.
*
* If the packet has a good CRC, the command is valid, and the sequence numbers match, then a response packet will be sent.
* For every response packet sent, the sequence number will be incremented by the boot loader. The next packet sent must send a
* sequence number which matches. If there is  a problem with the packet, a NAK will be sent.
*
* The response to a BC_QUERY command is a packet similar to the structure of a command packet, except it is sent from the target to the pc.
* Please note that the BC query command resets the sequence number to 0 so the next command sent should use sequence number 0.
* If there is a problem with the BC_QUERY command packet, NAK will be sent.
*
*
*/

/*
* Set your processor part number here
*/
#ifdef __PCM__
#include <16F1938.h>
//#include <16F1936.h>
//#include <16F1933.h>
#endif

//#define FORCE_ERR			// Test error recovery
#define	WITH_LED			// Allow LED usage
#define WITH_BUTTON			// Allow button usage

/*
* Leave these alone unless you know what you are doing
*/
#include "tnd.h"

#pragma nolist
#include <stdlib.h>
#pragma list
#pragma use fast_io(ALL)

/* 
* More User tweakable parameters
*/

// Fuses
#pragma fuses HS, PLL, WDT, PUT, NOLVP
// Clock speed
#pragma use delay(internal=32Mhz, restart_wdt)
// RS-232 baud rate
#pragma use rs232(baud=9600, UART1, ERRORS, RESTART_WDT)

#define TOTAL_PROGRAM_MEMORY getenv("PROGRAM_MEMORY")		// Number of words of program memory PIC16F1938
#define LOADER_SIZE	0x400			// Max Loader size in words

// Set these to indicate different versions and protocol changes to the PC loader app (pcl).
#define PRODUCTID	0x3FFF			// Product ID (unique for each product). Do not use the default 0x3FFF as that is reserved for testing.
#define BOOTVERSION	0			// Boot loader version (change when additional functionallity is added to boot loader, and pcl needs to know about it)
#define PROTOCOL	0			// Protocol in use (change when boot loader expects a deviation in the protocol different from what
						// is documented here and pcl needs to know about it)

// Initial states for I/O pins. Set these to suit your app.
#define TRISA		0x0F 			// Initial tris A value
#define TRISB 		0xFF			// Initial tris B value
#define TRISC 		0xBF			// Initial tris C value
#define	AINIT		0x00			// PORT A initial value
#define	BINIT		0x00			// PORT B initial value
#define CINIT		0x00			// PORT C initial value
#define TOTAL_PROGRAM_MEMORY getenv("PROGRAM_MEMORY")		// Number of words of program memory PIC16F1938
#define LOADER_SIZE	0x400			// Loader size in words

#define	TXEN		PIN_A4			// RS-485 Transmit enable
#ifdef WITH_LED
#define	LED		PIN_A5			// LED pin (optional)
#endif
#ifdef WITH_BUTTON
#define BUTTON		PIN_B6			// Button pin (optional)
#endif

#define EEBOOTSIG	0xFE			// Allows app to force entry into boot loader after reset
#define	EEADDR		0xFF			// Address of node address cell in EEPROM
/* 
* End of user tweakable parameters
*
* Do not change anything below unless you know what you're doing.
*/


#define CONFIG_MEMBASE	0x8000			// Base of config memory
#define CONFIG_BLOCK_SIZE 16			// Number of words of config memory to transfer
#define LOADER_START	0			// Loader starts at byte 0	
#define LOADER_LASTADDR	(LOADER_SIZE - 1)	// Last byte address of loader	
#define LOADER_BUFSIZE	80			// Buffer size in bytes for getting data from PC
#define LOADER_PAYLOAD	64			// Loader payload in Bytes
#define APP_START	LOADER_SIZE
#define APP_ENTRY	LOADER_SIZE
#define APP_ISR_ENTRY	LOADER_SIZE + 4
#define APP_SIZE (TOTAL_PROGRAM_MEMORY - LOADER_SIZE)



#define BC_QUERY	0x00			// Query command
#define BC_CHECK_APP	0x08			// Check app space for integrity
#define BC_WRITE_EN	0x10			// Write enable
#define BC_WRITE_PM	0x40			// Write program memory
#define BC_WRITE_EEPROM	0xA5			// Write config memory
#define BC_EXEC_APP	0x55			// Execute APP
#define BC_RESET	0xAA			// Reset CPU


// Buffer locations for info bytes in last row of app.

#define SIGLO 0x30
#define SIGHI 0x32
#define RULO  0x34
#define RUHI  0x36
#define RFULO 0x38
#define RFUHI 0x3A
#define CRCLO 0x3C
#define CRCHI 0x3E


// control characters used

#define NUL		0x00
#define SOH		0x01
#define STX 		0x02
#define ETX		0x03
#define SUBST		0x04
#define	HDC		0xFF	
#define ACK		0xC1
#define NAK 		0x81

#define MAX_CF 32


typedef struct {
	u16 lsize;
	u16 appsize;
	u16 prodid;
	u8  bootvers;
	u8  proto;
	u8  config[MAX_CF];
} response_t;

typedef union	{
	u8 payload[LOADER_PAYLOAD];
	response_t resp;
} pl_t;


typedef struct {
	u8 pkttype;
	u8 address;
	u8 cmd;
	u16 param;
	u16 seq;
	pl_t pl;
	u8 pad[7];
	u16 crc16;
} ps_t;

typedef union {
	u8 buffer[LOADER_BUFSIZE];
	ps_t s;
} packet_t;

// PIC16F193x specific EEPROM registers
// For reading a word from config memory
#define CFGS 6
#define RD 0

#pragma byte EEADRL=getenv("SFR:EEADRL")
#pragma byte EEADRH=getenv("SFR:EEADRH")
#pragma byte EEDATL=getenv("SFR:EEDATL")
#pragma byte EEDATH=getenv("SFR:EEDATH")
#pragma byte EECON1=getenv("SFR:EECON1")

// For CRC generator

#define STATUS getenv("SFR:STATUS")
#define CARRY 0
#define ZERO 2

/*
* Global Variables
*/


packet_t pkt;
static u8 cmd, myaddress;


// Default product ID at top of boot loader image

#pragma rom LOADER_LASTADDR = {PRODUCTID}

// Prevent the compiler from using any ROM above the first 1K.

#ifdef __PCM__
#org LOADER_SIZE, 0x07FF {}  // Rom page 0 (upper half only)
#org 0x0800, 0x0FFF {}  // Rom page 1 (2K words)

#if TOTAL_PROGRAM_MEMORY > 4096
#org 0x1000, 0x17FF {}  // Rom page 2 (2K words)
#org 0x1800, 0x1FFF {}  // Rom page 3 (2K words)
#endif
#if TOTAL_PROGRAM_MEMORY > 8192
#org 0x2000, 0x27FF {}  // Rom page 4 (2K words)
#org 0x2800, 0x2FFF {}  // Rom page 5 (2K words)
#org 0x3000, 0x37FF {}  // Rom page 6 (2K words)
#org 0x3800, 0x3FFF {}  // Rom page 7 (2K words) 
#endif
#endif


/*
* CODE
*/



// Calculate CRC over buffer using polynomial: X^16 + X^12 + X^5 + 1
// 42 words

u16 do_crc(u16 crcin, u8 *buf, u8 len)
{
	u8 i,crcl, crch, b, bitc;

	crcl = make8(crcin, 0);		// Split CRC word into bytes
	crch = make8(crcin, 1);

	
	for(i = 0; i < len; i++){
		b = buf[i];		// Fetch byte from buffer
		restart_wdt();
		#asm
		MOVF	b,W
		XORWF	crch,F		; XOR buffer byte with crc high byte
		MOVLW	8
		MOVWF	bitc		; Set bit count
		bitloop:
		BCF	STATUS,CARRY	; Clear carry flag
		RLF	crcl,F
		RLF	crch,F
		BTFSS	STATUS,CARRY
		GOTO	nogen		; Skip XOR with Generator
		MOVLW	0x10		; XOR with Generator
		XORWF	crch,F
		MOVLW	0x21
		XORWF	crcl,F
		nogen:
		DECFSZ	bitc,F
		GOTO	bitloop		; Next bit
		#endasm
	}
	return make16(crch, crcl);	// Return 16 bit CRC value
}



// ccs C 4.108 does not have a function to read config memory.
// So we wrote our own. This is specific to Midrange parts
// and has been only tested on the PIC 16F1938

u16 read_configmem_word(u8 wordaddr)
{
	u8 retval_l;
	u8 retval_h;	
	
	
	#asm
	MOVF    wordaddr,W
	MOVWF   EEADRL
	CLRF    EEADRH
	BSF     EECON1,CFGS
	BSF     EECON1,RD
	NOP          
	NOP
	MOVF    EEDATL,W
	MOVWF   retval_l
	MOVF    EEDATH,W
	MOVWF   retval_h
	BCF     EECON1,CFGS
	#endasm

	return make16(retval_h, retval_l);
}
/*
* Return true if transmitter and holding register are both empty
*/

static void tx_wait_empty()
{

	// Register bits

	const u8 TRMT=1;			// Transmit shift register empty
	const u8 TXIF=4;			// Transmit interrupt flag

	#pragma	byte pir1=getenv("SFR:PIR1");
	#pragma	byte txsta=getenv("SFR:TXSTA");

	for(;;){
		if(bit_test(pir1, TXIF)){
			if(bit_test(txsta, TRMT))
				break;
		}
	}
}



// Send query response packet

void send_query_response(void)
{
	u8 *q;
	u8 i;
	u16 v;

	for(i = 0; i < sizeof(packet_t); i++) // Zero out packet buffer
		pkt.buffer[i] = 0;

	// Build the response packet
	pkt.s.pl.resp.lsize = LOADER_SIZE;
	pkt.s.pl.resp.appsize = APP_SIZE;
	pkt.s.pl.resp.prodid = PRODUCTID;
	pkt.s.pl.resp.bootvers = BOOTVERSION;
	pkt.s.pl.resp.proto = PROTOCOL;
	for(i = 0, q = &pkt.s.pl.resp.config ; i < CONFIG_BLOCK_SIZE ; i++){
		v = read_configmem_word(i);
		*q++ = make8(v, 0);
		*q++ = make8(v, 1);
	}
	// Calculate CRC for above
	pkt.s.crc16 = do_crc(0, pkt.buffer, sizeof(packet_t) - sizeof(u16));

	putc(STX);
	// Send packet
	for( i = 0 ; i < sizeof(packet_t) ; i++){
		if(pkt.buffer[i] <= SUBST)
			putc(SUBST);
		putc(pkt.buffer[i]);
	}
	putc(ETX);
}


// Test ROM for integrity
	
bool check_appspace(void)
{
	u16 appcrc16, crc16, pmaddr, rowsused, i;

	read_program_memory(TOTAL_PROGRAM_MEMORY - (LOADER_PAYLOAD >> 1) , pkt.buffer, LOADER_PAYLOAD); // Read the top row
	if((pkt.buffer[SIGHI] != 0x55) || (pkt.buffer[SIGLO] != 0xAA))
		return 1;

	
	appcrc16 = make16(pkt.buffer[CRCHI],pkt.buffer[CRCLO]);
	rowsused = make16(pkt.buffer[RUHI], pkt.buffer[RULO]);

	// Signature is good, read all rows except the last one and calculate a CRC
	crc16 = 0;
	pmaddr = APP_START;
	for(i = 0 ; i < rowsused ; i++){ // Calculate the CRC over the rows the app used.
		read_program_memory(pmaddr, pkt.buffer, LOADER_PAYLOAD);
		crc16 = do_crc(crc16, pkt.buffer, 64) ;
		pmaddr += (LOADER_PAYLOAD >> 1);
		restart_wdt();

	}


	return (appcrc16 == crc16) ? 0 : 1;

}


// Process packet

u8 process_packet(void)
{
	u8 i;
	u8 acknak;
	u8 eeaddress;
	u16 crc16;
	static u16 seqno,pseq;
	u16 param;
	static u1 write_en;

	#ifdef FORCE_ERR
	static int errctr; 
	#endif

	// check packet
	crc16 = do_crc(0, pkt.buffer, sizeof(packet_t) - sizeof(u16));
	if(crc16 != pkt.s.crc16){
		return NUL; // no good, ignore
	}


	// check packet type
	if(pkt.s.pkttype != HDC)
		return NUL; // wrong packet type, ignore

	// check packet address
	if(pkt.s.address != myaddress)
		return NUL; // not for us, ignore


	pseq = pkt.s.seq;
	param = pkt.s.param;
	cmd = pkt.s.cmd;
	acknak = ACK;

	switch(cmd){
		case	BC_QUERY:
			seqno = 0; // Zero out sequence number when we get this command. This provides a way to sync.
			write_en = 0;
			#ifdef FORCE_ERR
			errctr = 11; // Force an error on the tenth block. 
			#endif
			acknak = SOH;
			break;

		case	BC_WRITE_EN:
			write_en = 1;
			break;

		case	BC_WRITE_PM:

			#ifdef FORCE_ERR
			if(errctr == 1){ // Force an error 
				acknak = NAK;
				output_high(LED); // DEBUG
				errctr--;
				break;
			}
			if(errctr)
				errctr--;
			#endif

			if((write_en) && (seqno == pseq) && (param >= APP_START)){
				write_program_memory(param, pkt.s.pl.payload, LOADER_PAYLOAD);
			}
			else
				acknak = NAK;
			break;

		case	BC_WRITE_EEPROM: // Write EEPROM in 64 byte chunks
			if(write_en && (seqno == pseq)){
				eeaddress = ((u8) param);				
				for(i = 0; i < LOADER_PAYLOAD; i++)
					write_eeprom(eeaddress, pkt.s.pl.payload[i]);
			}
			else
				acknak = NAK;
			break;

		case	BC_CHECK_APP:
			if(check_appspace())
				acknak = STX; // Bad
			else
				acknak = ACK; // Good
			break;

		case	BC_RESET:
		case	BC_EXEC_APP:
			acknak = ETX;
			break;

		default:
			acknak = NAK;
			break;
	}
	if(acknak == ACK){
		seqno++;
	}
	return acknak;
}

// Handle download from PC

void do_bootloader(void)
{
	u8 c;
	u8 i;

	for(;;){
		c = getc();
		// wait for STX;
		if(c != STX)
			continue;


		// Get packet bytes

		for(i = 0;;){
			c = getc();
			if(c == ETX)
				break;
			if(c == SUBST)
				c = getc();
			if(i < LOADER_BUFSIZE)
				pkt.buffer[i++] = c;
		}

		// Process packet
		i = process_packet();
		output_bit(TXEN, TRUE);
		switch(i){
			case	ACK:
				putc(ACK);
				break;

			case	STX:
				putc(STX);
				break;

			case	SOH:
				send_query_response();
				break;

			case	ETX:
				putc(ACK);
				delay_ms(1000);
				if(cmd == BC_RESET)
					reset_cpu();
				else{
					write_eeprom(EEBOOTSIG, 0xFF); // Clear boot signature
					goto_address(APP_ENTRY); // Outta here...
				}
				break;
	
			case	NAK:
				putc(NAK);
				break;
			
			default:
				break;
			
		}
		tx_wait_empty();
		output_bit(TXEN, FALSE);

	}
}


// Top level

main()
{

	setup_wdt(WDT_2S);
	set_tris_a(TRISA);
	set_tris_b(TRISB);
	set_tris_c(TRISC);
	output_A(AINIT);
	output_B(BINIT);
	output_C(CINIT);
	port_b_pullups(0xC0);

	delay_ms(100);
	
	myaddress = read_eeprom(EEADDR);
	
	if(myaddress == 0xFF) // If EEPROM erased
		myaddress = 0x1F; // Use test address 0x1F

	#ifdef WITH_BUTTON
	if(input(BUTTON) == 0){
		do_bootloader();
	}
	#endif

	if(read_eeprom(EEBOOTSIG) == 0x55){		// If signature in EEPROM, do bootloader
		do_bootloader();
	}

	if(check_appspace())
		do_bootloader(); // App space corrupt!
	goto_address(APP_ENTRY); // CRC is good, start app	

}

// Interrupt service referral

#int_global
void isr(void) {
   jump_to_isr(APP_ISR_ENTRY); // Note: CCS Skips over first two bytes of app isr for efficiency
}

/*
* END
*/




