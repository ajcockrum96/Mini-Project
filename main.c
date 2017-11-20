#include <hidef.h>
#include "derivative.h"
#include <mc9s12c32.h>

/* All functions after main should be initialized here */

void LED_met(void);
void metcnt_correct(void);

/* Variable declarations */
unsigned int  curcnt = 0;
unsigned int  metcnt = 0;
unsigned char bpm    = 120;
unsigned char error  = 0;

unsigned char leftpb  = 0x00;
unsigned char rghtpb  = 0x00;
unsigned char prevpb  = 0x11;
unsigned char lpbstat = 0x10;
unsigned char rpbstat = 0x01;

/* Macro definitions */
#define TIM_CONSTANT 250000;

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
	CRGINT = CRGINT | 0x80;
	RTICTL = 0x1F;
	DDRAD  = DDRAD | 0x3F;	// Set PAD6 and PAD7 as inputs

	/* Initialize TIM Ch 7 (TC7) for periodic interrupts every 240 microseconds
		- Enable timer subsystem
		- Set channel 7 for output compare
		- Set appropriate pre-scale factor and enable counter reset after OC7
		- Set up channel 7 to generate 240 microsecond interrupt rate
		- Initially disable TIM Ch 7 interrupts
	*/

	TSCR1 = TSCR1 | 0x80;	// Enable TIM subsystem
	TSCR2 = TSCR2 | 0xC0;	// Enable TCNT reset by Channel 7 and prescale bus clock by 16
	TIOS  = TIOS  | 0x80;	// Set Channel 7 for output compare
	TC7   = 360;			// Set Channel 7 to count to 360
	TIE   = TIE & 0x7F;     // Disable Interrupts

	/* Initialize (other) digital I/O port pins */
	DDRT  = DDRT | 0x01;	// Set PTT0 as an output
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
	EnableInterrupts;


	for(;;) {
		// If right pushbutton is pressed, start metronome
		if(rghtpb) {
			rghtpb = 0;
			TIE = TIE | 0x80;	// Enable TIM interrupts
		}
		// If left pushbutton is pressed, stop metronome
		if(leftpb) {
			leftpb = 0;
			TIE = TIE & 0x7F;	// Enable TIM interrupts
		}
		// If metronome on and metcnt reached, pulse metronome "beat"
		if(curcnt >= metcnt) {
			curcnt = 0;
			LED_met();
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
	lpbstat = (PTAD & 0x80) >> 3;
	rpbstat = (PTAD & 0x40) >> 6;

	if ((prevpb & 0x10) && !(lpbstat & 0x10)) {
		leftpb = 1;
	}
	if ((prevpb & 0x01) && !(rpbstat & 0x01)) {
		rghtpb = 1;
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

void LED_met(void) {
	PTT_PTT0 = !PTT_PTT0;
}

void metcnt_correct(void) {
	metcnt = 250000 / bpm;
	error  = 250000 % bpm;
	if(error >= bpm / 2) {
		++metcnt;
	}
}