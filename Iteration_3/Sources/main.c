#include <hidef.h>
#include "derivative.h"
#include <mc9s12c32.h>

/* All functions after main should be initialized here */

void LED_met(char led);
void metcnt_correct(void);
void shiftout(char);
void lcdwait(void);
void send_byte(char);
void send_i(char);
void chgline(char);
void print_c(char);
void pmsglcd(char[]);
void bpmdisp(void);

/* Variable declarations */
unsigned int  curcnt   = 0;
unsigned int  metcnt   = 0;
unsigned int  pulsecnt = 0;
unsigned int  beatcnt  = 0;
unsigned int  meascnt  = 0;
unsigned int  subcnt   = 0;
unsigned char bpm      = 120;
unsigned char error    = 0;
unsigned char bacc     = 0;
unsigned char macc     = 0;
unsigned char sacc     = 0;

unsigned char leftpb  = 0x00;
unsigned char rghtpb  = 0x00;
unsigned char prevpb  = 0x11;
unsigned char lpbstat = 0x10;
unsigned char rpbstat = 0x01;

char tmpch = 0;
char runstp = 1;
char i = 0;

unsigned char       tsnum   = 4;	// Time Signature Numerator
unsigned char       tsden   = 4;	// Time Signature Denominator
unsigned char       tsbeat  = 4;	// Length of "beat" in terms of MAX_DEN
unsigned char       tssub   = 4;	// Length of subdivision in terms of MAX_DEN
const unsigned char MAX_DEN = 16;	// Maximum Subdivision allowed
const unsigned char MIN_DEN = 2;	// Minimum Subdivision allowed

/* Macro definitions */
#define TIM_CONSTANT 250000;

/* LCD INSTRUCTION CHARACTERS */
#define LCDON 0x0F	// LCD initialization command
#define LCDCLR 0x01	// LCD clear display command
#define TWOLINE 0x38	// LCD 2-line enable command
#define CURMOV 0xFE	// LCD cursor move instruction
#define LINE1 0x80	// LCD line 1 cursor position
#define LINE2 0xC0	// LCD line 2 cursor position

/*
***********************************************************************
Initializations
***********************************************************************
*/

void  initializations(void) {
	/* Set the PLL speed (bus clock = 24 MHz) */
	CLKSEL = CLKSEL & 0x80;	// disengage PLL from system
	PLLCTL = PLLCTL | 0x40;	// turn on PLL
	SYNR = 0x02;			// set PLL multiplier
	REFDV = 0;				// set PLL divider
	while (!(CRGFLG & 0x08)){}
	CLKSEL = CLKSEL | 0x80;	// engage PLL

	/* Disable watchdog timer (COPCTL register) */
	COPCTL = 0x40	  ; // COP off; RTI and COP stopped in BDM-mode

	/* Initialize asynchronous serial port (SCI) for 9600 baud, interrupts off initially */
	SCIBDH =	0x00; //set baud rate to 9600
	SCIBDL =	0x9C; //24,000,000 / 16 / 156 = 9600 (approx)
	SCICR1 =	0x00; //$9C = 156
	SCICR2 =	0x0C; //initialize SCI for program-driven operation
	DDRB	 =	0x10; //set PB4 for output mode
	PORTB	 =	0x10; //assert DTR pin on COM port

	/* Initialize RTI for a 2.048 ms interrupt rate */
	CRGINT  = CRGINT | 0x80;
	RTICTL  = 0x1F;
	DDRAD   = DDRAD & 0x3F;	// Set PAD6 and PAD7 as inputs
	ATDDIEN = ATDDIEN | 0xC0;

	/* Initialize ATD Ch 0 for input */
	ATDCTL2 = 0x80;
	ATDCTL3 = 0x08;
	ATDCTL4 = 0x85;

	/* Initialize TIM Ch 7 (TC7) for periodic interrupts every 240 microseconds
		- Enable timer subsystem
		- Set channel 7 for output compare
		- Set appropriate pre-scale factor and enable counter reset after OC7
		- Set up channel 7 to generate 240 microsecond interrupt rate
		- Initially disable TIM Ch 7 interrupts
	*/
	TSCR1 = TSCR1 | 0x80;	// Enable TIM subsystem
	TSCR2 = TSCR2 | 0x0C;	// Enable TCNT reset by Channel 7 and prescale bus clock by 16
	TIOS  = TIOS  | 0x80;	// Set Channel 7 for output compare
	TC7   = 360;			// Set Channel 7 to count to 360
	TIE   = TIE & 0x7F;     // Disable Ch 7 Interrupts

	/* Initialize SPI for baud rate of 6 Mbs, MSB first */
	SPICR1 = 0x50;
	SPIBR = 0x01;
	DDRM  = 0x30;

	/* Initialize (other) digital I/O port pins */
	DDRT  = DDRT | 0x70;	// Set PTT pins 4, 5, and 6 as outputs (LCD)
	DDRT  = DDRT | 0x0F;	// Set PTT pins 0, 1, 2, and 3 as an outputs (LEDs)

	/* Initialize the LCD
		- pull LCDCLK high (idle)
		- pull R/W' low (write state)
		- turn on LCD (LCDON instruction)
		- enable two-line mode (TWOLINE instruction)
		- clear LCD (LCDCLR instruction)
		- wait for 2ms so that the LCD can wake up     
	*/
	PTT_PTT6 = 1;
	PTT_PTT5 = 0;
	send_i(LCDON);
	send_i(TWOLINE);
	send_i(LCDCLR);
	lcdwait();
}

/*
***********************************************************************
Main
***********************************************************************
*/
void main(void) {
	DisableInterrupts
	initializations();
	metcnt_correct();
	bpmdisp();
	EnableInterrupts;
	TIE = TIE | 0x80; // Enable Ch 7 interrupts

	for(;;) {
		// If right pushbutton is pressed, start/stop metronome
		if(rghtpb) {
			rghtpb = 0;
			runstp = !runstp;
		}
		// If left pushbutton is pressed, enter setup mode
			// Flash necessary setting choices on LCD
			// Left pushbutton advances current setting
			// Right pushbutton advances selection
		if(leftpb) {
			leftpb = 0;
			send_i(CURMOV);
			send_i(LINE2 + 4);
			// Time Signature Numerator Select
			while(!leftpb) {
				if(rghtpb) {
					rghtpb = 0;
					tsnum++;
					if(tsnum > 32 || tsnum == 0) {
						tsnum = 1;
					}
					// Update LCD
					bpmdisp();
					send_i(CURMOV);
					send_i(LINE2 + 4);
				}
			}
			leftpb = 0;
			send_i(CURMOV);
			send_i(LINE2 + 7);
			// Time Signature Denominator Select
			while(!leftpb) {
				if(rghtpb) {
					rghtpb = 0;
					tsden *= 2;
					if(tsden > MAX_DEN) {
						tsden = MIN_DEN;
					}
					// Update LCD
					bpmdisp();
					send_i(CURMOV);
					send_i(LINE2 + 7);
				}
			}
			leftpb = 0;
			send_i(CURMOV);
			send_i(LINE2 + 11);
			// Beat Length Select
			while(!leftpb) {
				if(rghtpb) {
					rghtpb = 0;
					if(tsbeat < 4) {
						tsbeat++;
					}
					else {
						tsbeat += 2;
						if(tsbeat > 8) {
							tsbeat = 1;
						}
					}
					if(tsbeat > tsnum * MAX_DEN / tsden) {
						tsbeat = 1;
					}
					// Update LCD
					bpmdisp();
					send_i(CURMOV);
					send_i(LINE2 + 11);
				}
			}
			leftpb = 0;
			send_i(CURMOV);
			send_i(LINE2 + 15);
			// Beat Subdivision Select
			while(!leftpb) {
				if(rghtpb) {
					rghtpb = 0;
					tssub *= 2;
					if(tssub > tsbeat) {
						tssub = 1;
					}
					// Update LCD
					bpmdisp();
					send_i(CURMOV);
					send_i(LINE2 + 15);
				}
			}
			leftpb = 0;
			send_i(CURMOV);
			send_i(LINE2 + 16);
			metcnt_correct();
		}
		// If metronome on and metcnt reached, count subdivisions
		if(curcnt >= metcnt && runstp) {
			curcnt = 0;
			pulsecnt = (pulsecnt + 1) % (MAX_DEN / tsden);
			beatcnt  = (beatcnt + 1) % tsbeat;
			subcnt   = (subcnt + 1) % tssub;
			// If metronome on and denominator beat reached, pulse normal sound
			if(pulsecnt == 0) {
				LED_met(0);
				meascnt = (meascnt + 1) % tsnum;
				// If metronome on and measure accent enabled, pulse at the front of each measure
				if (meascnt == 0 && macc == 1) {
					LED_met(2);
				}
			}
			// If metronome on and beat accent enabled, pulse at the front of each bpm beat
			if(beatcnt == 0 && bacc == 1) {
				LED_met(1);
			}
			// If metronome on and subdivision accent enabled, pulse at the front of each subdivision
			if(subcnt == 0 && sacc == 1) {
				LED_met(3);
			}
		}
	} /* loop forever */
}	/* do not leave main */



/*
***********************************************************************
RTI interrupt service routine: RTI_ISR
***********************************************************************
*/

interrupt 7 void RTI_ISR(void) {
	// clear RTI interrupt flagt
	CRGFLG = CRGFLG | 0x80;
	rpbstat = (PTAD & 0x40) >> 6;
	lpbstat = (PTAD & 0x80) >> 3;

	if ((prevpb & 0x01) && !(rpbstat & 0x01)) {
		rghtpb = 1;
	}
	if ((prevpb & 0x10) && !(lpbstat & 0x10)) {
		leftpb = 1;
	}
	prevpb = lpbstat | rpbstat;
}

/*
***********************************************************************
TIM interrupt service routine
***********************************************************************
*/

interrupt 15 void TIM_ISR(void) {
	// clear TIM CH 7 interrupt flag
	TFLG1 = TFLG1 | 0x80;
	++curcnt;
}

/*
***********************************************************************
SCI (transmit section) interrupt service routine
***********************************************************************
*/

interrupt 20 void SCI_ISR(void) {
}

void LED_met(char led) {
	switch(led) {
		case 0:
			PTT_PTT0 = !PTT_PTT0;
			break;
		case 1:
			PTT_PTT1 = !PTT_PTT1;
			break;
		case 2:
			PTT_PTT2 = !PTT_PTT2;
			break;
		case 3:
			PTT_PTT3 = !PTT_PTT3;
	}
}

void metcnt_correct(void) {
	metcnt = 250000 / (bpm * tsbeat);
	error  = 250000 % (bpm * tsbeat);
	if(error >= bpm / 2) {
		++metcnt;
	}
}

/*
***********************************************************************
  shiftout: Transmits the character x to external shift 
            register using the SPI.  It should shift MSB first.  
            MISO = PM[4]
            SCK  = PM[5]
***********************************************************************
*/
void shiftout(char x) {
	// test the SPTEF bit: wait if 0; else, continue
	// write data x to SPI data register
	// wait for 30 cycles for SPI data to shift out
	while (!(SPISR & 0x20)) {}
	SPIDR = ~x;
	asm {
          pshx
          ldx   #9
  dloop:  dbne x,dloop
          pulx
	}
}

/*
***********************************************************************
  lcdwait: Delay for approx 2 ms
***********************************************************************
*/
void lcdwait() {
	asm {
          pshx
          ldx #16000
  wloop:  dbne x, wloop
          pulx
	}
}

/*
*********************************************************************** 
  send_byte: writes character x to the LCD
***********************************************************************
*/
void send_byte(char x) {
	// shift out character
	// pulse LCD clock line low->high->low
	// wait 2 ms for LCD to process data
	shiftout(x);
	PTT_PTT6 = 1;
	PTT_PTT6 = 0;
	PTT_PTT6 = 1;
	lcdwait();
}

/*
***********************************************************************
  send_i: Sends instruction byte x to LCD  
***********************************************************************
*/
void send_i(char x) {
	// set the register select line low (instruction data)
	// send byte
	PTT_PTT4 = 0;
	send_byte(x);
}

/*
***********************************************************************
  chgline: Move LCD cursor to position x
  NOTE: Cursor positions are encoded in the LINE1/LINE2 variables
***********************************************************************
*/
void chgline(char x) {
	send_i(CURMOV);
	send_i(x);
}

/*
***********************************************************************
  print_c: Print (single) character x on LCD            
***********************************************************************
*/
void print_c(char x) {
	PTT_PTT4 = 1;
	send_byte(x);
}

/*
***********************************************************************
  pmsglcd: print character string str[] on LCD
***********************************************************************
*/
void pmsglcd(char str[]) {
	tmpch = str[0];
	while (tmpch != 0)
	{
		print_c(tmpch);
		str++;
		tmpch = str[0];
	}
}

void bpmdisp(void) {
	chgline(LINE1);
	print_c(bpm / 100 + '0');
	print_c(bpm / 10 % 10 + '0');
	print_c(bpm % 10 + '0');
	pmsglcd(" BPM");

	chgline(LINE2);
	pmsglcd("TS:");

	if(tsnum >= 10) {
		print_c(tsnum / 10 + '0');
	}
	else {
		print_c(' ');
	}
	print_c(tsnum % 10 + '0');

	print_c('/');

	if(tsden >= 10) {
		print_c(tsden / 10 + '0');
	}
	else {
		print_c(' ');
	}
	print_c(tsden % 10 + '0');

	pmsglcd("B:");

	if(MAX_DEN / tsbeat >= 10) {
		print_c((MAX_DEN / tsbeat) / 10 + '0');
	}
	else {
		print_c(' ');
	}
	print_c((MAX_DEN / tsbeat) % 10 + '0');

	pmsglcd("S:");

	if(MAX_DEN / tssub >= 10) {
		print_c((MAX_DEN / tssub) / 10 + '0');
	}
	else {
		print_c(' ');
	}
	print_c((MAX_DEN / tssub) % 10 + '0');
}