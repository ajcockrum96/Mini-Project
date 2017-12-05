#include <hidef.h>
#include "derivative.h"
#include <mc9s12c32.h>

/* All functions after main should be initialized here */

/* Macro definitions */
#define BUF_SIZE		100		// PWM Output Function Buffer Size

/* Variable declarations */
unsigned int buffer[BUF_SIZE];
unsigned int buf_cnt = 0;

unsigned int i = 0;
unsigned char pitch_val = 0;

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

	/* Initialize PWM */
	PWME    = 0x02;		// Enable PWM Channel 1
	PWMPOL  = 0x02;		// Set Channel 1 as active high
	PWMCLK  = 0x00;		// Select CLK A for Channel 1
	PWMPRCLK = 0x01;	// Prescale CLK A by 2 (12 MHz)
	PWMCTL   = 0x00;
	PWMCAE   = 0x00;
	PWMDTY1  = 0;		// Initially Zero the Duty Cycle
	PWMPER1  = 200;		// Set OSF of Channel 1 to 60 kHz
	MODRR    = 0x02;	// Route PWM1 to PT1

	/* Initialize (other) digital I/O port pins */
	// DDRT  = DDRT | 0x03;	// Set PTT pins 0 and 1 as an outputs (LEDs)

	/* Fill PWM Function output buffer */
	// 100 * sin(2 * pi * x / BUF_SIZE) + 100
	// Generates a 5 Vpp sin wave output
	buffer[0]  = buffer[50] = 100;
	buffer[1]  = buffer[49] = 106;
	buffer[2]  = buffer[48] = 113;
	buffer[3]  = buffer[47] = 119;
	buffer[4]  = buffer[46] = 125;
	buffer[5]  = buffer[45] = 131;
	buffer[6]  = buffer[44] = 137;
	buffer[7]  = buffer[43] = 143;
	buffer[8]  = buffer[42] = 148;
	buffer[9]  = buffer[41] = 154;
	buffer[10] = buffer[40] = 159;
	buffer[11] = buffer[39] = 164;
	buffer[12] = buffer[38] = 168;
	buffer[13] = buffer[37] = 173;
	buffer[14] = buffer[36] = 177;
	buffer[15] = buffer[35] = 181;
	buffer[16] = buffer[34] = 184;
	buffer[17] = buffer[33] = 188;
	buffer[18] = buffer[32] = 190;
	buffer[19] = buffer[31] = 193;
	buffer[20] = buffer[30] = 195;
	buffer[21] = buffer[29] = 197;
	buffer[22] = buffer[28] = 198;
	buffer[23] = buffer[27] = 199;
	buffer[24] = buffer[26] = 200;
	buffer[25] = 200;

	buffer[51] = buffer[99] = 94;
	buffer[52] = buffer[98] = 87;
	buffer[53] = buffer[97] = 81;
	buffer[54] = buffer[96] = 75;
	buffer[55] = buffer[95] = 69;
	buffer[56] = buffer[94] = 63;
	buffer[57] = buffer[93] = 57;
	buffer[58] = buffer[92] = 52;
	buffer[59] = buffer[91] = 46;
	buffer[60] = buffer[90] = 41;
	buffer[61] = buffer[89] = 36;
	buffer[62] = buffer[88] = 32;
	buffer[63] = buffer[87] = 27;
	buffer[64] = buffer[86] = 23;
	buffer[65] = buffer[85] = 19;
	buffer[66] = buffer[84] = 16;
	buffer[67] = buffer[83] = 12;
	buffer[68] = buffer[82] = 10;
	buffer[69] = buffer[81] =  7;
	buffer[70] = buffer[80] =  5;
	buffer[71] = buffer[79] =  3;
	buffer[72] = buffer[78] =  2;
	buffer[73] = buffer[77] =  1;
	buffer[74] = buffer[76] =  0;
	buffer[75] = 0;
}

/*
***********************************************************************
Main
***********************************************************************
*/
void main(void) {
	DisableInterrupts
	initializations();
	EnableInterrupts;
	TIE = TIE | 0x80; // Enable Ch 7 interrupts

	for(;;) {
		//pitch_val = 0;
		//for(i = 0; i < 10; ++i) {
			//asm {
				////pshx
				//ldx #16000
		//wloop:  dbne x, wloop
				//pulx
			//}
		//}
		//PWMPER1 = 0;
		//for(i = 0; i < 105; ++i) {
			//asm {
				//pshx
				//ldx #16000
		//w2loop:  dbne x, w2loop
				//pulx
			//}
		//}
		//PWMPER1 = 200;
		//pitch_val = 1;
		//for(i = 0; i < 10; ++i) {
			//asm {
				//pshx
				//ldx #16000
		//w3loop:  dbne x, w3loop
				//pulx
			//}
		//}
		//PWMPER1 = 0;
		//for(i = 0; i < 105; ++i) {
			//asm {
				//pshx
				//ldx #16000
		//w4loop:  dbne x, w4loop
				//pulx
			//}
		//}
		//PWMPER1 = 200;
		//pitch_val = 2;
		//for(i = 0; i < 10; ++i) {
			//asm {
				//pshx
				//ldx #16000
		//w5loop:  dbne x, w5loop
				//pulx
			//}
		//}
		PWMPER1 = 0;
		//for(i = 0; i < 105; ++i) {
			//asm {
				//pshx
				//ldx #16000
		//w6loop:  dbne x, w6loop
				//pulx
			//}
		//}
		//PWMPER1 = 200;
		//pitch_val = 3;
		//for(i = 0; i < 10; ++i) {
			//asm {
				//pshx
				//ldx #16000
		//w7loop:  dbne x, w7loop
				//pulx
			//}
		//}
		//PWMPER1 = 0;
		//for(i = 0; i < 105; ++i) {
			//asm {
				//pshx
				//ldx #16000
		//w8loop:  dbne x, w8loop
				//pulx
			//}
		//}
		//PWMPER1 = 200;
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
	buf_cnt = (buf_cnt + 10 * (pitch_val + 1)) % BUF_SIZE;
	PWMDTY1 = buffer[buf_cnt];
}

/*
***********************************************************************
SCI (transmit section) interrupt service routine
***********************************************************************
*/
interrupt 20 void SCI_ISR(void) {
}