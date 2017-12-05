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

void print_note(unsigned char note_len, char LCD);
void tsnum_inc(void);
void tsden_inc(void);
void tsbeat_inc(void);
void tssub_inc(void);

/* Macro definitions */
#define TIM_CONSTANT	250000
#define MAX_DEN			16		// Maximum Subdivision allowed
#define MIN_DEN			2		// Minimum Subdivision allowed
#define BPM_INC			4		// Resolution of bpm settings
#define BUF_SIZE		100		// PWM Output Function Buffer Size

/* Special LCD char definitions */
#define DOTTED_NOTE_CHAR	0xAA
#define EIGHTH_NOTE_CHAR	0x91
#define HALF_NOTE_CHAR		'd'

/* LCD INSTRUCTION CHARACTERS */
#define LCDON 0x0F		// LCD initialization command
#define LCDCLR 0x01		// LCD clear display command
#define TWOLINE 0x38	// LCD 2-line enable command
#define CURMOV 0xFE		// LCD cursor move instruction

/* LCD LOCATION CHARACTERS */
#define LEFT_LCD  0		// Left LCD select value
#define RIGHT_LCD 1		// Right LCD select value
#define LINE1 0x80		// LCD line 1 cursor position
#define LINE2 0xC0		// LCD line 2 cursor position
#define LINE1_END 0x8F	// LCD end of line 1 cursor position
#define LINE2_END 0xCF	// LCD end of line 2 cursor position

#define BEAT_SET			LINE1
#define TSNUM_SET			(LINE1_END - 1)
#define TSDEN_SET			(LINE2_END - 1)
#define SUBDIV_SET			(LINE1 + 7)
#define BEAT_ACCENT_SET		(LINE2 + 4)
#define MEASURE_ACCENT_SET	(LINE2 + 8)
#define SUBDIV_ACCENT_SET	(LINE2 + 12)

/* Mask definitions */
#define SET_INC_MASK_PTT	0x10
#define SET_TOG_MASK_PTT	0x20
#define START_STOP_MASK_PTT	0x40
#define TEMPO_TAP_MASK_PTT	0x80

/* Macro 'function' definitions */
#define HIDE_CURSOR(LCD)			(chgline(LINE2_END + 1, (LCD)))
#define CURSOR_TO_BEAT()			(chgline(BEAT_SET, LEFT_LCD))
#define CURSOR_TO_TSNUM()			(chgline(TSNUM_SET, LEFT_LCD))
#define CURSOR_TO_TSDEN()			(chgline(TSDEN_SET, LEFT_LCD))
#define CURSOR_TO_SUBDIV()			(chgline(SUBDIV_SET, RIGHT_LCD))
#define CURSOR_TO_BEAT_ACCENT()		(chgline(BEAT_ACCENT_SET, RIGHT_LCD))
#define CURSOR_TO_MEASURE_ACCENT()	(chgline(MEASURE_ACCENT_SET, RIGHT_LCD))
#define CURSOR_TO_SUBDIV_ACCENT()	(chgline(SUBDIV_ACCENT_SET, RIGHT_LCD))

#define UPDATE_BEAT()				print_note(tsbeat, LEFT_LCD)
#define UPDATE_TSNUM()				print2digits(tsnum, LEFT_LCD);
#define UPDATE_TSDEN()				print2digits(tsden, LEFT_LCD);
#define UPDATE_SUBDIV()				print_note(tssub, RIGHT_LCD);

#define PULSE_DISP()				{chgline(LINE2, LEFT_LCD);	print2digits(meascnt + 1, LEFT_LCD);	HIDE_CURSOR(LEFT_LCD);}

/* Variable declarations */
unsigned int  curcnt   = 0;		// Number of TIM interrupts since the last MAX_DEN note
unsigned int  metcnt   = 0;		// Number of TIM interrupts needed for each MAX_DEN note given control register settings, desired bpm, and desired beat length
unsigned int  pulsecnt = 0;		// Number of MAX_DEN notes since the last normal pulse
unsigned int  beatcnt  = 0;		// Number of MAX_DEN notes since the last beat pulse
unsigned int  meascnt  = 0;		// Number of pulse notes since the last measure pulse
unsigned int  subcnt   = 0;		// Number of MAX_DEN notes since the last subdivision pulse

unsigned char bpm      = 120;	// Desired tempo in beats per minute
unsigned char error    = 0;		// Rounding error: used to determine if metcnt should be rounded up or down

unsigned char beat_accent    = 0;		// Beat accent setting
unsigned char measure_accent = 0;		// Measure accent setting
unsigned char subdiv_accent  = 0;		// Subdivision accent setting

unsigned char set_inc    = 0;		// Setting Increment Flag
unsigned char set_tog    = 0;		// Setting Toggle Flag
unsigned char start_stop = 0;		// Start/Stop Flag
unsigned char tempo_tap  = 0;		// Pushbutton 7 flag: Tap in Tempo
unsigned char prevpb     = 0xF0;	// Previous pushbutton statuses
unsigned char pbstat     = 0xF0;	// Current pushbutton statuses

char tmpch = 0;
char runstp = 1;
char i = 0;

unsigned char tsnum   = 4;	// Time Signature Numerator
unsigned char tsden   = 4;	// Time Signature Denominator
unsigned char tsbeat  = 4;	// Length of "beat" in terms of MAX_DEN
unsigned char tssub   = 4;	// Length of subdivision in terms of MAX_DEN

char tapseq   = 0;
char tapindex = 0;
char numbeats = 0;
char beats    = 0;
unsigned int tmstmp[MAX_DEN];
unsigned int avg    = 0;
unsigned int ratio  = 0;
unsigned int iratio = 0;

unsigned int buffer[BUF_SIZE];
unsigned int buf_cnt = 0;
unsigned char pitch_val = 0;
unsigned char pitch_on  = 0;

unsigned char send_pulse  = 0;
unsigned char send_beat   = 0;
unsigned char send_meas   = 0;
unsigned char send_subdiv = 0;

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
	RTICTL  = 0x5F;
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

	/* Initialize PWM */
	PWME    = 0x02;		// Enable PWM Channel 1
	PWMPOL  = 0x02;		// Set Channel 1 as active high
	PWMCLK  = 0x00;		// Select CLK A for Channel 1
	PWMPRCLK = 0x01;	// Prescale CLK A by 2 (12 MHz)
	PWMDTY1 = 0;		// Initially Zero the Duty Cycle
	PWMPER1 = 200;		// Set OSF of Channel 1 to 60 kHz
	MODRR   = 0x02;		// Route PWM1 to PT1

	/* Initialize SPI for baud rate of 6 Mbs, MSB first */
	SPICR1 = 0x50;
	SPIBR = 0x01;
	DDRM  = 0x30;

	/* Initialize (other) digital I/O port pins */
	DDRT  = DDRT | 0xFC;	// Set PTT pins 3, 4, 5, 6, and 7 as outputs (LCDs)
	DDRT  = DDRT | 0x03;	// Set PTT pins 0 and 1 as an outputs (LEDs)

	/* Initialize the LEFT_LCD
		- pull LCDCLK high (idle)
		- pull R/W' low (write state)
		- turn on LCD (LCDON instruction)
		- enable two-line mode (TWOLINE instruction)
		- clear LCD (LCDCLR instruction)
		- wait for 2ms so that the LCD can wake up     
	*/
	PTT_PTT7 = 1;
	PTT_PTT6 = 0;
	send_i(LCDON, LEFT_LCD);
	send_i(TWOLINE, LEFT_LCD);
	send_i(LCDCLR, LEFT_LCD);
	lcdwait();

	/* Initialize the RIGHT_LCD
		- pull LCDCLK high (idle)
		- pull R/W' low (write state)
		- turn on LCD (LCDON instruction)
		- enable two-line mode (TWOLINE instruction)
		- clear LCD (LCDCLR instruction)
		- wait for 2ms so that the LCD can wake up     
	*/
	PTT_PTT4 = 1;
	PTT_PTT3 = 0;
	send_i(LCDON, RIGHT_LCD);
	send_i(TWOLINE, RIGHT_LCD);
	send_i(LCDCLR, RIGHT_LCD);
	lcdwait();

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
	metcnt_correct();
	bpmdisp();
	EnableInterrupts;
	TIE = TIE | 0x80; // Enable Ch 7 interrupts

	for(;;) {
		// If pushbutton 6 is pressed, start/stop metronome
		if(start_stop) {
			start_stop = 0;
			runstp = !runstp;
		}
		// If left pushbutton is pressed, enter setup mode
		// Flash necessary setting choices on LCD
		if(set_tog) {
			// Beat Length Select
			set_tog = 0;
			CURSOR_TO_BEAT();
			while(!set_tog) {
				if(set_inc) {
					set_inc = 0;
					tsbeat_inc();
				}
			}

			// Time Signature Numerator Select
			set_tog = 0;
			CURSOR_TO_TSNUM();
			while(!set_tog) {
				if(set_inc) {
					set_inc = 0;
					tsnum_inc();
				}
			}

			// Time Signature Denominator Select
			set_tog = 0;
			CURSOR_TO_TSDEN();
			while(!set_tog) {
				if(set_inc) {
					set_inc = 0;
					tsden_inc();
				}
			}
			HIDE_CURSOR(LEFT_LCD);

			// Beat Subdivision Select
			set_tog = 0;
			CURSOR_TO_SUBDIV();
			while(!set_tog) {
				if(set_inc) {
					set_inc = 0;
					tssub_inc();
				}
			}

			// Beat Accent Select
			set_tog = 0;
			CURSOR_TO_BEAT_ACCENT();
			while(!set_tog) {
				if(set_inc) {
					set_inc = 0;
					beat_accent = !beat_accent;
					if(beat_accent) {
						pmsglcd("Beat", RIGHT_LCD);
					}
					else {
						pmsglcd("    ", RIGHT_LCD);
					}
					CURSOR_TO_BEAT_ACCENT();
				}
			}

			// Measure Accent Select
			set_tog = 0;
			CURSOR_TO_MEASURE_ACCENT();
			while(!set_tog) {
				if(set_inc) {
					set_inc = 0;
					measure_accent = !measure_accent;
					if(measure_accent) {
						pmsglcd("Meas", RIGHT_LCD);
					}
					else {
						pmsglcd("    ", RIGHT_LCD);
					}
					CURSOR_TO_MEASURE_ACCENT();
				}
			}

			// Subdivision Accent Select
			set_tog = 0;
			CURSOR_TO_SUBDIV_ACCENT();
			while(!set_tog) {
				if(set_inc) {
					set_inc = 0;
					subdiv_accent = !subdiv_accent;
					if(subdiv_accent) {
						pmsglcd("Sub", RIGHT_LCD);
					}
					else {
						pmsglcd("   ", RIGHT_LCD);
					}
					CURSOR_TO_SUBDIV_ACCENT();
				}
			}

			set_tog = 0;
			HIDE_CURSOR(RIGHT_LCD);
			metcnt_correct();
			pulsecnt = 0;
			beatcnt = 0;
			subcnt = 0;
			meascnt = 0;
		}
		// If metronome on and metcnt reached, count subdivisions
		if(curcnt >= metcnt && runstp) {
			curcnt = 0;
			pulsecnt = (pulsecnt + 1) % (MAX_DEN / tsden);
			beatcnt  = (beatcnt + 1) % tsbeat;
			subcnt   = (subcnt + 1) % tssub;
			// If metronome on and denominator beat reached, pulse normal sound
			if(pulsecnt == 0) {
				send_pulse = 1;
				meascnt = (meascnt + 1) % tsnum;
				PULSE_DISP();
				// If metronome on and measure accent enabled, pulse at the front of each measure
				if (meascnt == 0 && measure_accent == 1) {
					send_meas = 1;
				}
			}
			// If metronome on and beat accent enabled, pulse at the front of each bpm beat
			if(beatcnt == 0 && beat_accent == 1) {
				send_beat = 1;
			}
			// If metronome on and subdivision accent enabled, pulse at the front of each subdivision
			if(subcnt == 0 && subdiv_accent == 1) {
				send_subdiv = 1;
			}
			if(send_meas) {
				LED_met(3);
				send_meas = 0;
				send_beat = 0;
				send_pulse = 0;
				send_subdiv = 0;
			}
			else if(send_beat) {
				LED_met(2);
				send_beat = 0;
				send_pulse = 0;
				send_subdiv = 0;
			}
			else if(send_pulse) {
				LED_met(1);
				send_pulse = 0;
				send_subdiv = 0;
			}
			else if(send_subdiv) {
				LED_met(0);
				send_subdiv = 0;
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
	pbstat = (PTAD & (SET_TOG_MASK_PTT | SET_INC_MASK_PTT));

	// Check PAD5 (Setting 'Activator')
	if ((prevpb & SET_TOG_MASK_PTT) && !(pbstat & SET_TOG_MASK_PTT)) {
		set_tog = 1;
	}
	// Check PAD4 (Setting 'Setter')
	if ((prevpb & SET_INC_MASK_PTT) && !(pbstat & SET_INC_MASK_PTT)) {
		set_inc = 1;
	}
	prevpb = (pbstat & ~(TEMPO_TAP_MASK_PTT)) | (prevpb & TEMPO_TAP_MASK_PTT);
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
	if(pitch_on) {
		buf_cnt = (buf_cnt + 10 * (pitch_val + 1)) % BUF_SIZE;
		PWMDTY1 = buffer[buf_cnt];
	}

	// Check PAD7 (Tap in Tempo)
	pbstat = (PTAD & TEMPO_TAP_MASK_PTT);
	if ((prevpb & TEMPO_TAP_MASK_PTT) && !(pbstat & TEMPO_TAP_MASK_PTT)) {
		tempo_tap = 1;
	}
	prevpb = (pbstat & TEMPO_TAP_MASK_PTT) | (prevpb & ~(TEMPO_TAP_MASK_PTT));

	if(tempo_tap) {
		runstp = 0;
		tempo_tap = 0;
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
			tmstmp[tapindex++] = curcnt;
			if(beats == 1) {
				if (tapindex > numbeats - 1) {
					tapseq = 0;
					runstp = 1;
					avg = 0;
					for (i = numbeats - 1; i > 0; --i) {
						avg += (tmstmp[i] - tmstmp[i - 1]) / (numbeats - 1);
					}
					bpm = TIM_CONSTANT / avg;
				}
			}
			else {
				if (tapindex > tsnum - 1) {
					tapseq = 0;
					runstp = 1;
					avg = 0;
					for (i = tsnum - 1; i > 0; --i) {
						avg += (tmstmp[i] - tmstmp[i - 1]) / (tsnum - 1);
					}
					// FIXME: Currently working here!!!
					bpm = TIM_CONSTANT / avg * MAX_DEN / (tsbeat * tsden);	// Modify for not using beats as tapping
				}
			}

			error = bpm % BPM_INC;	// Determine error from multiple of BPM_INC
			bpm = bpm / BPM_INC * BPM_INC;	// Truncate to multiple of BPM_INC
			if(error >= BPM_INC / 2) {	// Round to nearest multiple of BPM_INC
				bpm += BPM_INC;
			}
			metcnt_correct();
			bpmdisp();
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
	pitch_val = led;
	pitch_on  = 1;
	for(i = 0; i < 10; ++i) {
		lcdwait();
	}
	pitch_on = 0;
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
	metcnt = TIM_CONSTANT / (bpm * tsbeat);
	error  = TIM_CONSTANT % (bpm * tsbeat);
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
	while (tmpch != 0) {
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
	// Output current tempo in terms of the defined beat length
	chgline(LINE1, LEFT_LCD);
	print_note(tsbeat, LEFT_LCD);
	print_c('=', LEFT_LCD);
	print3digits(bpm, LEFT_LCD);

	// Output current time signature setting
	chgline(TSNUM_SET, LEFT_LCD);
	print2digits(tsnum, LEFT_LCD);
	chgline(TSDEN_SET, LEFT_LCD);
	print2digits(tsden, LEFT_LCD);

	// Hide LEFT_LCD Cursor
	HIDE_CURSOR(LEFT_LCD);

	// Output current subdivision length setting
	chgline(LINE1, RIGHT_LCD);
	pmsglcd("Subdiv:", RIGHT_LCD);
	print_note(tssub, RIGHT_LCD);

	// Output current accent settings
	chgline(LINE2, RIGHT_LCD);
	pmsglcd("Acc:", RIGHT_LCD);
	if(beat_accent) {
		pmsglcd("Beat", RIGHT_LCD);
	}
	else {
		pmsglcd("    ", RIGHT_LCD);
	}
	if(measure_accent) {
		pmsglcd("Meas", RIGHT_LCD);
	}
	else {
		pmsglcd("    ", RIGHT_LCD);
	}
	if(subdiv_accent) {
		pmsglcd("Sub", RIGHT_LCD);
	}
	else {
		pmsglcd("   ", RIGHT_LCD);
	}

	// Hide RIGHT_LCD Cursor
	HIDE_CURSOR(RIGHT_LCD);
}

void print_note(unsigned char note_len, char LCD) {
	if(MAX_DEN == 16) {
		switch(note_len) {
			case 1:
				pmsglcd("  16th", LCD);
				break;
			case 2:
				pmsglcd("   8th", LCD);
				break;
			case 3:
				pmsglcd("  D8th", LCD);
				break;
			case 4:
				pmsglcd(" Quart", LCD);
				break;
			case 6:
				pmsglcd("DQuart", LCD);
				break;
			case 8:
				pmsglcd("  Half", LCD);
		}
	}
}

void tsnum_inc(void) {
	tsnum++;
	if(tsnum > MAX_DEN || tsnum == 0) {
		tsnum = 1;
	}
	UPDATE_TSNUM();
	CURSOR_TO_TSNUM();
}

void tsden_inc(void) {
	tsden *= 2;
	if(tsden > MAX_DEN) {
		tsden = MIN_DEN;
	}
	UPDATE_TSDEN();
	CURSOR_TO_TSDEN();
}

void tsbeat_inc(void) {
	switch(tsbeat) {
		case 1:
		case 2:
		case 3:
			tsbeat += 1;
			break;
		case 4:
		case 6:
			tsbeat += 2;
			break;
		case 8:
			tsbeat = 1;
	}

	if(tsbeat > tsnum * MAX_DEN / tsden) {
		tsbeat = 1;
	}

	UPDATE_BEAT();
	CURSOR_TO_BEAT();
}

void tssub_inc(void) {
	tssub *= 2;
	if(tssub > tsbeat) {
		tssub = 1;
	}

	UPDATE_SUBDIV();
	CURSOR_TO_SUBDIV();
}