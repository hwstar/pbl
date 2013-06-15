#include <xc.h>
#include <stdint.h>
#include "defs.h"
#include "flash.h"


//#define FORCE_ERR			// Test error recovery
#define	WITH_LED			// Allow LED usage
#define WITH_BUTTON			// Allow button usage


/* Oscillator frequency */
#define _XTAL_FREQ 32000000

// Fuses

#pragma config WDTE=OFF, LVP=OFF, FOSC=INTOSC, PWRTE=OFF, CP=ON, CPD=ON, \
        BOREN=ON, CLKOUTEN=ON, IESO=ON, FCMEN=OFF, \
        WRT=3, PLLEN=ON, STVREN=ON, BORV=LO, LVP=OFF



// Set these to indicate different versions and protocol changes to the PC loader app (pcl).
#define PRODUCTID	0x3FFF			// Product ID (unique for each product). Do not use the default 0x3FFF as that is reserved for testing.
#define BOOTVERSION	0			// Boot loader version (change when additional functionallity is added to boot loader, and pcl needs to know about it)
#define PROTOCOL	0			// Protocol in use (change when boot loader expects a deviation in the protocol different from what
						// is documented here and pcl needs to know about it)
#define LOADER_SIZE	0x800			// Loader size in words

#define	TXENA		LATCbits.LATC3		// RS-485 Transmit enable
#ifdef WITH_LED
#define	LED		LATAbits.LATA5		// LED pin (optional)
#endif
#ifdef WITH_BUTTON
#define BUTTON		PORTAbits.RA2       	// Button pin (optional)
#endif

#define EEBOOTSIG	0xFE			// Allows app to force entry into boot loader after reset
#define	EEADDR		0xFF			// Address of node address cell in EEPROM
/* 
* End of user tweakable parameters
*
* Do not change anything below unless you know what you're doing.
*/

/*
 * PIC specific EEPROM registers
 */

#define CONFIG_BLOCK_SIZE 16			// Number of words of config memory to transfer
#define LOADER_START	0			// Loader starts at byte 0	
#define APP_START	LOADER_SIZE
#define APP_SIZE (_ROMSIZE - LOADER_SIZE)


#if (APP_START >= _ROMSIZE)
#error(Part is too small!)
#endif

// Commands

#define BC_QUERY	0x00			// Query command
#define BC_CHECK_APP	0x08			// Check app space for integrity
#define BC_WRITE_EN	0x10			// Write enable
#define BC_WRITE_PM	0x40			// Write program memory
#define BC_WRITE_EEPROM	0xA5			// Write config memory
#define BC_EXEC_APP	0x55			// Execute APP
#define BC_RESET	0xAA			// Reset CPU


// control characters used

#define NUL		0x00
#define SOH		0x01
#define STX 		0x02
#define ETX		0x03
#define SUBST		0x04
#define	HDC		0xFF	
#define ACK		0xC1
#define NAK 		0x81

#define MAX_CF          32

#define POLY16          0x1021



/* Macros */

#define SET_BAUD(B) (((_XTAL_FREQ/B)/64) - 1)

/*
 * Structs
 */

typedef struct {
    uint8_t rfu[48];
    uint8_t siglo;
    uint8_t useless1;
    uint8_t sighi;
    uint8_t useless2;
    uint8_t rulo;
    uint8_t useless3;
    uint8_t ruhi;
    uint8_t useless4;
    uint8_t rfu2[4];
    uint8_t crclo;
    uint8_t useless7;
    uint8_t crchi;
    uint8_t useless8;
} info_t;

typedef struct {
	uint16_t lsize;
	uint16_t appsize;
	uint16_t prodid;
	uint8_t  bootvers;
	uint8_t  proto;
	uint8_t  config[MAX_CF];
} response_t;

typedef union	{
	uint8_t payload[ROW_BYTES];
	response_t resp;
        info_t i;
} pl_t;


typedef struct {
	uint8_t pkttype;
	uint8_t address;
	uint8_t cmd;
	uint16_t param;
	uint16_t seq;
	pl_t pl;
	uint8_t pad[7];
	uint16_t crc16;
} ps_t;

typedef union {
	uint8_t buffer[sizeof(ps_t)];
	ps_t s;
} packet_t;


/*
* Global Variables
*/


packet_t pkt;
static uint8_t cmd, myaddress;

/*
* CODE
*/

/*
 * Jump to start of application
 */

static void start_app(void)
{
#asm
    LJMP    APP_START;
#endasm
}

/*
 * Calculate 16 bit CRC over buffer using polynomial
*/

static uint16_t calc_crc16(uint16_t crcin, uint8_t *buf, uint8_t len)
{
    uint8_t i,j;
    static bit dogen;
    union{
        uint16_t word;
        struct{
            uint8_t low;
            uint8_t high;
        };
    } crc;

    crc.word = crcin;
    for(i = 0; i != len; i++){
        CLRWDT();
        crc.high ^= buf[i];
        for(j = 0; j != 8; j++){
            dogen = ((crc.high & 0x80) != 0);
            crc.word <<= 1;
            if(dogen){
                crc.word ^= POLY16;
            }
        }
    }
    return crc.word;
}

/*
 * Read a word from confog memory
 */

static uint16_t read_configmem_word(uint8_t wordaddr)
{
	uint16_t retval;

        EEADRL = wordaddr;
        EEADRH = 0;
        EECON1bits.CFGS = TRUE;
        EECON1bits.RD = TRUE;
        NOP();
        NOP();
        retval = EEDATH << 8;
        retval |= EEDATL;
        EECON1bits.CFGS = FALSE;
        return retval;
}


/*
* Return true if transmitter and holding register are both empty
*/

static void tx_wait_empty()
{

	for(;;){
		if(PIR1bits.TXIF){
			if(TXSTAbits.TRMT)
				break;
		}
	}
}

/*
 * Output a character to the UART
 */

static void putc(uint8_t c)
{
    tx_wait_empty();
    TXREG = c;
}

/*
 * Wait for a character to be received, then return it
 */

static uint8_t getc(void)
{
    while(FALSE == PIR1bits.RCIF)
        if(RCSTAbits.OERR){
            RCSTAbits.SPEN = FALSE;
            NOP();
            RCSTAbits.SPEN = TRUE;
        }

    return RCREG;

}


/* Send query response packet */

static void send_query_response(void)
{
	uint8_t *q;
	uint8_t i;
	uint16_t v;

	for(i = 0; i != sizeof(packet_t); i++) // Zero out packet buffer
		pkt.buffer[i] = 0;

	// Build the response packet
	pkt.s.pl.resp.lsize = LOADER_SIZE;
	pkt.s.pl.resp.appsize = APP_SIZE;
	pkt.s.pl.resp.prodid = PRODUCTID;
	pkt.s.pl.resp.bootvers = BOOTVERSION;
	pkt.s.pl.resp.proto = PROTOCOL;
	for(i = 0, q = &pkt.s.pl.resp.config ; i != CONFIG_BLOCK_SIZE ; i++){
		v = read_configmem_word(i);
		*q++ = (uint8_t) v;
		*q++ = (uint8_t)(v >> 8);
	}
	// Calculate CRC for above
	pkt.s.crc16 = calc_crc16(0, pkt.buffer, sizeof(packet_t) - sizeof(uint16_t));

	putc(STX);
	// Send packet
	for( i = 0 ; i != sizeof(packet_t) ; i++){
		if(pkt.buffer[i] <= SUBST)
			putc(SUBST);
		putc(pkt.buffer[i]);
	}
	putc(ETX);
}


/* Test app in ROM for integrity */
	
static uint8_t check_appspace(void)
{
	uint16_t appcrc16, crc16, pmaddr, rowsused, i;

	flash_read_row((_ROMSIZE - ROW_WORDS) , &pkt.s.pl); // Read the top row
	if((pkt.s.pl.i.sighi != 0x55) || (pkt.s.pl.i.siglo != 0xAA))
		return 1;

	
	appcrc16 =  (((uint16_t) pkt.s.pl.i.crchi << 8)) | pkt.s.pl.i.crclo;
	rowsused = (((uint16_t) pkt.s.pl.i.ruhi << 8)) | pkt.s.pl.i.rulo;

	// Signature is good, read all rows except the last one and calculate a CRC
	crc16 = 0;
	pmaddr = APP_START;
	for(i = 0 ; i != rowsused ; i++){ // Calculate the CRC over the rows the app used.
		flash_read_row(pmaddr, pkt.buffer);
		crc16 = calc_crc16(crc16, pkt.buffer, ROW_BYTES) ;
		pmaddr += ROW_WORDS;
		CLRWDT();

	}


	return (appcrc16 == crc16) ? 0 : 1;

}


/* Process received packet */

static uint8_t process_packet(void)
{
	uint8_t i;
	uint8_t acknak;
	uint8_t eeaddress;
	uint16_t crc16;
	static uint16_t seqno,pseq;
	uint16_t param;
	static bit write_en;

	#ifdef FORCE_ERR
	static int errctr; 
	#endif

	// check packet
	crc16 = calc_crc16(0, pkt.buffer, sizeof(packet_t) - sizeof(uint16_t));
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
			if(1 == errctr){ // Force an error
				acknak = NAK;
				output_high(LED); // DEBUG
				errctr--;
				break;
			}
			if(errctr)
				errctr--;
			#endif

			if((write_en) && (seqno == pseq) && (param >= APP_START)){
				flash_write_row(param, pkt.s.pl.payload);
			}
			else
				acknak = NAK;
			break;

		case	BC_WRITE_EEPROM: // Write EEPROM in 64 byte chunks
			if(write_en && (seqno == pseq)){
				eeaddress = ((uint8_t) param);
                                for(i = 0; i != ROW_BYTES; i++)
                                    eeprom_write(eeaddress+i, pkt.s.pl.payload[i]);
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

/* Handle download from PC */

static void do_bootloader(void)
{
	uint8_t c;
	uint8_t i;

	for(;;){
		c = getc();
		// wait for STX;
		if(c != STX)
			continue;


		// Get packet bytes

		for(i = 0;;){
			c = getc();
			if(ETX == c)
				break;
			if(SUBST == c)
				c = getc();
			if(i < sizeof(pkt))
				pkt.buffer[i++] = c;
		}

		/* Process packet */

		i = process_packet();

                /* Send a response */

		TXENA = TRUE;
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
                                tx_wait_empty();
                                TXENA = FALSE;
				if(BC_RESET == cmd)
                                    asm("ljmp 0");
				else{
                                    if(eeprom_read(EEBOOTSIG) != 0xFF)
					eeprom_write(EEBOOTSIG, 0xFF); // Clear boot signature
                                     // Jump to App.
                                     start_app();
				}
			
	
			case	NAK:
				putc(NAK);
				break;
			
			default:
				break;
			
		}
		tx_wait_empty();
                TXENA = FALSE;

	}
}


// Top level

int main(void)
{

     /*
     * Init
     */
    OSCCON = 0x70; // Select 8MHz source for PLL

    /* Port A */
    TRISA = 0x07;
    PORTA = 0x00;

    /* Port C */
    TRISC = 0x20;
    PORTC = 0x00;
    

    /* UART */
    SPBRGL = SET_BAUD(9600);
    RCSTA = 0x10;
    TXSTA = 0x20;
    RCSTAbits.SPEN = TRUE;

    /* Get RS-485 address */
    myaddress = eeprom_read(EEADDR);
    if(0xFF == myaddress) // If EEPROM erased
        myaddress = 0x1F; // Use test address 0x1F

    __delay_ms(100);
	
    #ifdef WITH_BUTTON

    /* If boot loader button pressed/ jumper inserted */

    if(BUTTON == 0){
            do_bootloader();
    }
    #endif

    /* Test for boot loader signature in EEPROM */

    if(0x55 == eeprom_read(EEBOOTSIG)){		// If signature in EEPROM, do bootloader
            do_bootloader();
    }

    /* Test app for correct CRC */

    if(check_appspace()){
            do_bootloader(); // App space corrupt!
    }

    /* Jump to app entry point */

    start_app();
    for(;;);
}

/* Interrupt service redirection */

interrupt void isr_redirect(void)
{
#asm
    LJMP (APP_START + 4);
#endasm
}


/*
* END
*/




