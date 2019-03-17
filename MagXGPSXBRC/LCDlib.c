/* ************************************************************************** */
/** Descriptive File Name: LCDlib.c - Character LCD driver using the PIC32
						   PMP interface
  @ Author  
	 Richard Wall
  
  @ Date: 
	 Created:	August 12, 2013
	 Revised:	June 17, 2016 for Basys MX3
				 October 27, 2016 - relabeled defined constants macros, 
				 January 4, 2017 - Added graphical display
	 Verified:	 May 19, 2017
	 Revised:	June 17, 2017 - CHanged TAB control for LCDputs
	 
  @Company
	Digilent Inc

  @File Name
	LCDlib.c

  @Development Environment
	MPLAB X IDE x3.61 - http://www.microchip.com/mplab/mplab-x-ide 
	XC32 1.43 - http://www.microchip.com/mplab/compilers
	PLIB 3/7/20162 - http://www.microchip.com/SWLibraryWeb/product.aspx?product=PIC32%20Peripheral%20Library

  @Summary
	Character LCD control using the PIC32 parallel Master Port (PMP).

  @Description
	This program provides low level LCD IO control using the PIC32
	PMP bus. The initLCD function must be call before any ASCII
	characters can be written to the LCD. The readLCD and writeLCD
	functions handle access to both the LCD control registers
	(addr = 0) and the LCD DDRAM/CGRAM (addr = 1). The putsLCD function
	processes an ASCII string of characters that can contain ASCII
	control characters LF, CR, and TAB.  The TAB control moves the
	cursor 8 spaces to the right.
 
	Macros provided in LCDlib.h provide LCD cursor control and character
	based operations

  @ Remarks:
	The interface is implemented using the parallel Master Port (PMP)

**************************************************************************** */

/* System included files */
#include "hardware.h"
#include <plib.h>

/* Application included files */
#include "swDelay.h"		/* Required for DelayMs function */
#include "LCDlib.h"
#include <stdint.h>

// Graphical characters for generatting FFT display
unsigned char lcd_blks[64] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F,
							  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F,
							  0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F,
							  0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F,
							  0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F,
							  0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F,
							  0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F,
							  0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F};

/* initLCD Function Description *********************************************
 * SYNTAX:		  void initLCD( void);
 * KEYWORDS:		LCD, Character, PMP
 * DESCRIPTION:	 Sets up the PMP interface and Initializes a 16x2
 *				  character LCD
 * PARAMETERS:	  None
 * RETURN VALUE:	None
 * Notes:		   None
 * END DESCRIPTION **********************************************************/
void initLCD( void) {
	int config1 = PMP_ON|PMP_READ_WRITE_EN|PMP_READ_POL_HI|PMP_WRITE_POL_HI;
	int config2 = PMP_DATA_BUS_8 | PMP_MODE_MASTER1 |
				  PMP_WAIT_BEG_4 | PMP_WAIT_MID_15 | PMP_WAIT_END_4;
	int config3 = PMP_PEN_0;		/* only PMA0 enabled */
	int config4 = PMP_INT_OFF;	  /* no interrupts used */

	PORTSetPinsDigitalIn(IOPORT_E, LCD_DATAbits);		// RE0:7
	PORTSetPinsDigitalOut(IOPORT_D, ENpin);				// RD4
	PORTSetPinsDigitalOut(IOPORT_D, RWpin);				// RD5
	PORTSetPinsDigitalOut(IOPORT_B, RSpin);				// RB15

	mPMPOpen( config1, config2, config3, config4); // PMP initialization
	
	msDelay( 20);			   // wait for > 20ms
	
// initialize the HD44780 display 8-bit init sequence
	PMPSetAddress( LCDCMD);	 // select command register
	PMPMasterWrite( LCD_CFG );  // 8-bit int, 2 lines, 5x7
	msDelay( 39);			   //  > 48ms
	
	PMPMasterWrite( LCD_ON );   // ON, no cursor, no blink
	msDelay( 39);			   // > 37ms
	
	PMPMasterWrite( LCD_CLR );  // clear display
	msDelay( 2);				// > 1.6ms
	
	PMPMasterWrite( LCD_ENTRY ); /* ON, no cursor, no blink */
	msDelay( 50);			   /* Settling time - not required */
	putsLCD("PIC32MX370 \tDigilent Inc.");   /* Sign on message */
} /* End of initLCD */

/* readLCD Function Description *******************************************
 * SYNTAX:		  char readLCD( int addr);
 * KEYWORDS:		LCD, Character, PMP, read
 * DESCRIPTION:	 Writes a single character from the LCD.
 * PARAMETER1:	  addr - sets the RS pin
 * RETURN VALUE:	Byte read from LCD
 * Notes:		   The two read cycles are required as per peripheral library
 * END DESCRIPTION **********************************************************/
unsigned char readLCD( int addr)
{
unsigned char data; 
	PMPSetAddress( addr);		   // select register
	data = mPMPMasterReadByte();	// initiate read sequence
	data = mPMPMasterReadByte();	// read actual data
	return data;
} /* readLCD */

/* writeLCD Function Description *******************************************
 * SYNTAX:		  void writeLCD( int addr, char c);
 * KEYWORDS:		LCD, Character, PMP, write
 * DESCRIPTION:	 Writes a single character to the LCD.
 * PARAMETER1:	  addr - sets the RS pin
 * PARAMETER2:	  c - 8 bit data to write to LCD
 * RETURN VALUE:	None
 * Notes:		   This is a blocking function waiting for the LCD busy flag
					to be set clear.
END DESCRIPTION ************************************************************/
void writeLCD( int addr, char c)	
{
	PMPSetAddress( addr);	   // select LCD register 
	nsDelay(10);				// Wait 10 ns
	PMPMasterWrite( c);		 // initiate write sequence
} /* writeLCD */
	
/* putcLCD Function Description *******************************************
 * SYNTAX:		  void putcLCD( int rs, char c);
 * KEYWORDS:		LCD, Character, write
 * DESCRIPTION:	 Writes a single character to the LCD.
 * PARAMETER1:	  the RS pin
 * PARAMETER2:	  c - 8 bit data to write to LCD
 * RETURN VALUE:	None
 * NOTES:		   This is a blocking function waiting for the LCD busy flag
 *				  to be set clear.
 *				  
 *				  The LCD data pins must use bit set and bit clear 
 *				  instructions to prevent altering PORT E pins that are not 
 *				  connected to the LCD.
 *				  
 *				  Implements minimum delays for setup and hold times.
 * 
END DESCRIPTION ************************************************************/
void putcLCD( int rs, char c)	 
{
char bf;
	{
		LATBSET = BIT_3;		// LCD Busy Flag set
		do
		{
			bf = readLCD( LCDCMD) & LCD_BF;
		}while(bf);
		LATBCLR = BIT_3;		// LCD Busy Flag reset
	}
	
	writeLCD(rs, c);
} /* writeLCD */

/* putsLCD Function Description ********************************************
 * SYNTAX:		  void putsLCD( char *s);
 * KEYWORDS:		LCD, string, PMP, write
 * DESCRIPTION:	 Writes a NULL terminated ASCII string of characters to
 *				  the LCD.
 * PARAMETER1:	  character pointer to text string
 * RETURN VALUE:	None
 *
 * Notes:		   LF is converted to CLEAR DISPLAY.  CR is converted to
 *				  LINE RETURN, TAB inserts SPACE character to the end 
 *				  of line.
 * END DESCRIPTION *********************************************************/
void putsLCD( char *s)
{  
char c;
	
	while( *s) 
	{
		switch (*s)			 // Check for ASCII control characters
		{
		case '\n':			  // point to second line
			waitLCD();		  // Ensure cursor addr is valid
			setLCDC( 0x40);
			break;
		case '\r':			  // home, point to first line 
			waitLCD();		  // Ensure cursor addr is valid
			setLCDC( 0);
			break;
		case '\t':			  // advance next TAB (8) positions 
			waitLCD();		  // Ensure cursor address is valid
			c = addrLCD();	  // Read cursor address
			while( c % TABSIZE ) //Move to next TAB position - even if past EOL
			{
				putcLCD(LCDDATA, ' ');  // Insert space characters to EOL
				waitLCD();			  // Ensure cursor addr is valid
				c = addrLCD();		  // Read cursor address
			}

			if (( c >= LCD_LINE1) && (c < LCD_LINE2)) //if necessary reposition
			{
				setLCDC( LCD_LINE2 );   // Reposition cursor
			}
			break;				
		default:				// print character
			waitLCD();		  // Ensure cursor address is valid
			c = addrLCD();	  // Get cursor address
			if (( c >= LCD_LINE1) && (c < LCD_LINE2)) //if necessary reposition
			{
				waitLCD();			  // Ensure cursor address is valid
				setLCDC( LCD_LINE2 );   // Reposition cursor
			}
			putcLCD(LCDDATA, *s);	   // BF is checked before writing char
			break;
		} // end switch-case
		s++;
	} /* End while */
} /* putsLCD */

/* initLCDGr Function Description ********************************************
 * SYNTAX:		  void initLCDGr(void);
 * KEYWORDS:		LCD, string, PMP, write, variable CGRAM
 * DESCRIPTION:	 Programs the 8 CGRAM characters FFT Display
 *
 * PARAMETER1:	  None
 * RETURN VALUE:	None
 *
 * Notes:		   None
 * END DESCRIPTION *********************************************************/
void initLCDGr(void)
{
int i;
	clrLCD();
	for(i = 0; i < 8; i++)
	{
		writeLCD( LCDDATA, i);
	}

// Program graphical characters
	setLCDG(0); // Set CGRAM pointer to 0
	for(i=0; i<64; i++)
	{
		putcLCD( LCDDATA, lcd_blks[i]);
	}
	gotoLCD(0); // Set DDRAM to 0
	for(i = 0; i < 8; i++)
	{
		putcLCD( LCDDATA, i);
	}
}

/* FFT_Disp Function Description ********************************************
 * SYNTAX:		  void FFT_Disp(int16_t* mag, int N);
 * KEYWORDS:		LCD, string, PMP, write, variable CGRAM, graphic characters
 * DESCRIPTION:	 Display the 16 FFT bins using the 8 graphical characters
 *
 * PARAMETER1:	  Array containing the bin magnitudes
 * PARAMETER2:	  Number of bins to display
 * RETURN VALUE:	None
 *
 * Notes:		   None
 * END DESCRIPTION *********************************************************/
void FFT_Disp(int16_t* mag, int N)
{
int i;
uint8_t amp[32];
uint8_t a;

	clrLCD();
	for(i = 0; i< N; i++)
	{
		a = (uint8_t) (mag[i]);
		if(a > 7 )
		{
			amp[i+16] = 7;
			amp[i] =  a-8;
		}
		else
		{
			amp[i+16] = a;
			amp[i] = 0;		   
		}
	}
	for(i = 0; i< N; i++)
	{
		if(amp[i] == 0)
		{
			putcLCD( LCDDATA, ' ');
		}
		else
		{
			putcLCD( LCDDATA, amp[i]);			
		}
	}
	gotoLCD(NEW_LINE);
	for(i = 0; i< N; i++)
	{
		if(amp == 0)
		{
			putcLCD( LCDDATA, ' ');
		}
		else
		{
			putcLCD( LCDDATA, amp[i+16]);			
		}
	}
}

/* End of LCDlib.c */