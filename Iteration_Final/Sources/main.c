#include <hidef.h>
#include "derivative.h"
#include <mc9s12c32.h>

/* All functions after main should be initialized here */
void LED_met(char led);
void metcnt_correct(void);
void shiftout(char x);
void lcdwait(void);
void send_byte(char x, char LCD);
void send_i(char x, char LCD);
void chgline(char x, char LCD);
void print_c(char x, char LCD);
void pmsglcd(char str[], char LCD);
void print2digits(unsigned char x, char LCD);
void print3digits(unsigned char x, char LCD);
void bpmdisp(void);

/* Variable declarations */
unsigned int  curcnt   = 0;		// Number of TIM interrupts since the last MAX_DEN note
unsigned int  metcnt   = 0;		// Number of TIM interrupts needed for each MAX_DEN note given control register settings, desired bpm, and desired beat length
unsigned int  pulsecnt = 0;		// Number of MAX_DEN notes since the last normal pulse
unsigned int  beatcnt  = 0;		// Number of MAX_DEN notes since the last beat pulse
unsigned int  meascnt  = 0;		// Number of MAX_DEN notes since the last measure pulse
unsigned int  subcnt   = 0;		// Number of MAX_DEN notes since the last subdivision pulse

unsigned char bpm      = 120;	// Desired tempo in beats per minute
unsigned char error    = 0;		// Rounding error: used to determine if metcnt should be rounded up or down

unsigned char bacc     = 0;		// Beat accent setting
unsigned char macc     = 0;		// Measure accent setting
unsigned char sacc     = 0;		// Subdivision accent setting

unsigned char pb4    = 0;		// Pushbutton 4 flag: Setting 'Setter'
unsigned char pb5    = 0;		// Pushbutton 5 flag: Setting 'Activator'
unsigned char pb6    = 0;		// Pushbutton 6 flag: Start/Stop
unsigned char pb7    = 0;		// Pushbutton 7 flag: Tap in Tempo
unsigned char prevpb = 0xF0;	// Previous pushbutton statuses
unsigned char pbstat = 0xF0;	// Current pushbutton statuses

char tmpch = 0;
char runstp = 1;
char i = 0;

unsigned char       tsnum   = 4;	// Time Signature Numerator
unsigned char       tsden   = 4;	// Time Signature Denominator
unsigned char       tsbeat  = 4;	// Length of "beat" in terms of MAX_DEN
unsigned char       tssub   = 4;	// Length of subdivision in terms of MAX_DEN
const unsigned char MAX_DEN = 16;	// Maximum Subdivision allowed
const unsigned char MIN_DEN = 2;	// Minimum Subdivision allowed

char tapseq = 0;
char tapindex = 0;
char numbeats = 0;
char beats    = 0;
unsigned int tmstmp[16];
unsigned int avg    = 0;
unsigned int ratio  = 0;
unsigned int iratio = 0;

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
	DDRAD   = DDRAD & 0x0F;	// Set PAD pins 4, 5, 6, and 7 as inputs
	ATDDIEN = ATDDIEN | 0xF0;

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
	DDRT  = DDRT | 0xFC;	// Set PTT pins 3, 4, 5, 6, and 7 as outputs (LCDs)
	DDRT  = DDRT | 0x03;	// Set PTT pins 0 and 1 as an outputs (LEDs)

	/* Initialize the LCD 1 (L)
		- pull LCDCLK high (idle)
		- pull R/W' low (write state)
		- turn on LCD (LCDON instruction)
		- enable two-line mode (TWOLINE instruction)
		- clear LCD (LCDCLR instruction)
		- wait for 2ms so that the LCD can wake up     
	*/
	PTT_PTT7 = 1;
	PTT_PTT6 = 0;
	send_i(LCDON, 0);
	send_i(TWOLINE, 0);
	send_i(LCDCLR, 0);
	lcdwait();

	/* Initialize the LCD 2 (R)
		- pull LCDCLK high (idle)
		- pull R/W' low (write state)
		- turn on LCD (LCDON instruction)
		- enable two-line mode (TWOLINE instruction)
		- clear LCD (LCDCLR instruction)
		- wait for 2ms so that the LCD can wake up     
	*/
	PTT_PTT4 = 1;
	PTT_PTT3 = 0;
	send_i(LCDON, 1);
	send_i(TWOLINE, 1);
	send_i(LCDCLR, 1);
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
		// If pushbutton 6 is pressed, start/stop metronome
		if(pb6) {
			pb6 = 0;
			runstp = !runstp;
		}
		// If left pushbutton is pressed, enter setup mode
			// Flash necessary setting choices on LCD
			// Left pushbutton advances current setting
			// Right pushbutton advances selection
		if(pb5) {
			pb5 = 0;
			send_i(CURMOV, 0);
			send_i(LINE2 + 4, 0);
			// Time Signature Numerator Select
			while(!pb5) {
				if(pb4) {
					pb4 = 0;
					tsnum++;
					if(tsnum > 32 || tsnum == 0) {
						tsnum = 1;
					}
					// Update LCD
					bpmdisp();
					send_i(CURMOV, 0);
					send_i(LINE2 + 4, 0);
				}
			}
			pb5 = 0;
			send_i(CURMOV, 0);
			send_i(LINE2 + 7, 0);
			// Time Signature Denominator Select
			while(!pb5) {
				if(pb4) {
					pb4 = 0;
					tsden *= 2;
					if(tsden > MAX_DEN) {
						tsden = MIN_DEN;
					}
					// Update LCD
					bpmdisp();
					send_i(CURMOV, 0);
					send_i(LINE2 + 7, 0);
				}
			}
			pb5 = 0;
			send_i(CURMOV, 0);
			send_i(LINE2 + 11, 0);
			// Beat Length Select
			while(!pb5) {
				if(pb4) {
					pb4 = 0;
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
					send_i(CURMOV, 0);
					send_i(LINE2 + 11, 0);
				}
			}
			pb5 = 0;
			send_i(CURMOV, 0);
			send_i(LINE2 + 15, 0);
			// Beat Subdivision Select
			while(!pb5) {
				if(pb4) {
					pb4 = 0;
					tssub *= 2;
					if(tssub > tsbeat) {
						tssub = 1;
					}
					// Update LCD
					bpmdisp();
					send_i(CURMOV, 0);
					send_i(LINE2 + 15, 0);
				}
			}
			pb5 = 0;
			send_i(CURMOV, 0);
			send_i(LINE2 + 16, 0);
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
	Samples pushbuttons for setting controls
***********************************************************************
*/
interrupt 7 void RTI_ISR(void) {
	// clear RTI interrupt flagt
	CRGFLG = CRGFLG | 0x80;
	pbstat = (PTAD & 0xF0);

	// Check PAD6 (Start Stop)
	if ((prevpb & 0x40) && !(pbstat & 0x40)) {
		pb6 = 1;
	}
	// Check PAD5 (Setting 'Activator')
	if ((prevpb & 0x20) && !(pbstat & 0x20)) {
		pb5 = 1;
	}
	// Check PAD4 (Setting 'Setter')
	if ((prevpb & 0x10) && !(pbstat & 0x10)) {
		pb4 = 1;
	}
	prevpb = (pbstat & 0x70) | (prevpb & 0x80);
}

/*
***********************************************************************
TIM interrupt service routine
	Updates metronome curcnt (which counts up to metcnt, the length of
	a MAX_DEN note)
***********************************************************************
*/
interrupt 15 void TIM_ISR(void) {
	// clear TIM CH 7 interrupt flag
	TFLG1 = TFLG1 | 0x80;
	++curcnt;

	// Check PAD7 (Tap in Tempo)
	pbstat = (PTAD & 0xF0);
	if ((prevpb & 0x80) && !(pbstat & 0x80)) {
		pb7 = 1;
	}
	prevpb = (pbstat & 0x80) | (prevpb & 0x70);

	if(pb7) {
		runstp = 0;
		pb7 = 0;
		if (!tapseq) {
			curcnt = 0;
			tmstmp[0] = curcnt;
			tapseq = 1;
			tapindex = 1;

			// Determine if beats or pulses should be tapped out
			// Beats are preferred, but are only used if can cleanly (using integers)
			// be defined in relationship to pulses
			iratio = (tsbeat * tsden) / MAX_DEN; // pulses to beats
			ratio  = MAX_DEN / (tsbeat * tsden); // beats to pulses
			if(iratio != 0 && (tsnum % iratio == 0)) {
				beats    = 1;
				numbeats = tsnum / iratio;
			}
			else if(ratio != 0 && (ratio * tsbeat == MAX_DEN / tsden)) {
				beats    = 1;
				numbeats = tsnum * ratio;
			}
			else {
				beats = 0;
			}
		}
		else {
			if(beats == 1) {
				if (tapindex < numbeats - 1) {
					tmstmp[tapindex++] = curcnt;
				}
				else {
					tmstmp[tapindex++] = curcnt;
					tapseq = 0;
					runstp = 1;
					avg = 0;
					for (i = numbeats - 1; i > 0; --i) {
						avg += (tmstmp[i] - tmstmp[i - 1]) / (numbeats - 1);
					}
					// FIXME: Currently working here!!!
					bpm = 250000 / avg;	// I think this is okay?  Plug into metcnt equation to check

					error = bpm % 4;	// Determine error from multiple of 4
					bpm = bpm / 4 * 4;	// Truncate to multiple of 4
					if(error >= 2) {	// Round to nearest multiple of 4
						bpm += 4;
					}
					metcnt_correct();
					bpmdisp();
				}
			}
			else {
				if (tapindex < tsnum - 1) {
					tmstmp[tapindex++] = curcnt;
				}
				else {
					tmstmp[tapindex++] = curcnt;
					tapseq = 0;
					pb6 = 1;
					avg = 0;
					for (i = tsnum - 1; i > 0; --i) {
						avg += (tmstmp[i] - tmstmp[i - 1]) / (tsnum - 1);
					}
					// FIXME: Currently working here!!!
					bpm = 250000 / avg * MAX_DEN / (tsbeat * tsden);	// Modify for not using beats as tapping

					error = bpm % 4;	// Determine error from multiple of 4
					bpm = bpm / 4 * 4;	// Truncate to multiple of 4
					if(error >= 2) {	// Round to nearest multiple of 4
						bpm += 4;
					}
					metcnt_correct();
					bpmdisp();
				}
			}
		}
	}
}

/*
***********************************************************************
SCI (transmit section) interrupt service routine
***********************************************************************
*/
interrupt 20 void SCI_ISR(void) {
}

/*
***********************************************************************
  LED_met: Controls led outputs for different metronome pulses
***********************************************************************
*/
void LED_met(char led) {
	switch(led % 2) {
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

/*
***********************************************************************
  metcnt_correct: Calculates TIM interrupt counts that accumulate for
	the length of a MAX_DEN length note given a desired bpm and beat
	length in terms of MAX_DEN notes.  Constant value calculated from
	control register settings.
***********************************************************************
*/
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
void send_byte(char x, char LCD) {
	// shift out character
	// pulse LCD clock line low->high->low
	// wait 2 ms for LCD to process data
	shiftout(x);
	if(LCD == 0) {
		PTT_PTT7 = 1;
		PTT_PTT7 = 0;
		PTT_PTT7 = 1;
	}
	else {
		PTT_PTT4 = 1;
		PTT_PTT4 = 0;
		PTT_PTT4 = 1;
	}
	lcdwait();
}

/*
***********************************************************************
  send_i: Sends instruction byte x to LCD  
***********************************************************************
*/
void send_i(char x, char LCD) {
	// set the register select line low (instruction data)
	// send byte
	if(LCD == 0) {
		PTT_PTT5 = 0;
	}
	else {
		PTT_PTT2 = 0;
	}
	send_byte(x, LCD);
}

/*
***********************************************************************
  chgline: Move LCD cursor to position x
  NOTE: Cursor positions are encoded in the LINE1/LINE2 variables
***********************************************************************
*/
void chgline(char x, char LCD) {
	send_i(CURMOV, LCD);
	send_i(x, LCD);
}

/*
***********************************************************************
  print_c: Print (single) character x on LCD            
***********************************************************************
*/
void print_c(char x, char LCD) {
	if(LCD == 0) {
		PTT_PTT5 = 1;
	}
	else {
		PTT_PTT2 = 1;
	}
	send_byte(x, LCD);
}

/*
***********************************************************************
  pmsglcd: print character string str[] on LCD
***********************************************************************
*/
void pmsglcd(char str[], char LCD) {
	tmpch = str[0];
	while (tmpch != 0)
	{
		print_c(tmpch, LCD);
		str++;
		tmpch = str[0];
	}
}

/*
*********************************************************************** 
  print2digits: outputs a formatted 2 digit value to the LCD
***********************************************************************
*/
void print2digits(unsigned char x, char LCD) {
	if(x >= 10) {
		print_c(x / 10 + '0', LCD);
	}
	else {
		print_c(' ', LCD);
	}
	print_c(x % 10 + '0', LCD);
}

/*
*********************************************************************** 
  print3digits: outputs a formatted 3 digit value to the LCD
***********************************************************************
*/
void print3digits(unsigned char x, char LCD) {
	if(x >= 100) {
		print_c(x / 100 + '0', LCD);
	}
	else {
		print_c(' ', LCD);
	}

	if(x % 100 >= 10) {
		print_c(x / 10 % 10 + '0', LCD);
	}
	else {
		print_c(' ', LCD);
	}

	print_c(x % 10 + '0', LCD);
}

/*
*********************************************************************** 
  bpmdisp: updates LCD display with desired metronome settings
***********************************************************************
*/
void bpmdisp(void) {
	chgline(LINE1, 0);
	print3digits(bpm, 0);
	pmsglcd(" BPM", 0);

	chgline(LINE2, 0);
	pmsglcd("TS:", 0);
	print2digits(tsnum, 0);
	print_c('/', 0);
	print2digits(tsden, 0);

	pmsglcd("B:", 0);
	print2digits(MAX_DEN / tsbeat, 0);

	pmsglcd("S:", 0);
	print2digits(MAX_DEN / tssub, 0);

	chgline(LINE1, 1);
	pmsglcd("TEST SCREEN 2", 1);
	pmsglcd("                ", 1);
}