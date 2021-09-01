/*H************************************************************
*  FILENAME:      mmml.c
*  DESCRIPTION:   Micro Music Macro Language (μMML) Player for
*                 AVR microcontrollers.
*
*  NOTES:         To be compiled with AVR C.
*
*                 A four-channel MML inspired 1-bit music
*                 player. Three channels of harmonic pulse
*                 wave and a percussive sampler or noise
*                 generator.
*
*                 Eight different duty cycles possible:
*                 50%, 25%, 12.5%, 6.25%, 3.125%, 1.5625%,
*                 0.78125% and 0.390625%. The 'thinner' pulse
*                 widths are perceived as a change in waveform
*                 power (volume) as opposed to a change in
*                 timbre.
*
*                 Tested on:
*                   * Attiny13/45/85 @ 8MHz
*                   * Atmega168/328  @ 8MHz
*
*  AUTHOR:        Blake 'PROTODOME' Troise
************************************************************H*/

// stuff you can mess with
#define OUTPUT         0    // the PORTB hardware output pin

// stuff you shouldn't really mess with
#define CHANNELS       4    // the number of channels
#define SAMPLE_SPEED   5    // the sampler playback rate
#define SAMPLE_LENGTH  127  // the length of the sample array
#define MAXLOOPS       5    // the maximum number of nested loops

#include <avr/io.h>         // core avr functionality
#include <avr/pgmspace.h>   // * * *
#include "avr-mmml-data.h"  // holds the data[] arrays

// note table (plus an initial 'wasted' entry for rests)
const unsigned int note[13] PROGMEM = 
{
	// the rest command is technically note 0 and thus requires a frequency 
	255,
	// one octave of notes, equal temperament in Gb
	1024,967,912,861,813,767,724,683,645,609,575,542
};

// location of individual samples in sample array
const unsigned char sample_index[6] PROGMEM =
{
	0,19,34,74,118,126
};

// raw PWM sample data
const unsigned char sample[SAMPLE_LENGTH] PROGMEM =
{
	// bwoop (0)
	0b10101010,0b10110110,0b10000111,0b11111000,
	0b10000100,0b00110111,0b11101000,0b11000001,
	0b00000111,0b00111101,0b11111000,0b11100000,
	0b10010001,0b10000111,0b00000111,0b00001111,
	0b00001111,0b00011011,0b00011110,
	// beep (19)
	0b10101010,0b00101010,0b00110011,0b00110011,
	0b00110011,0b00110011,0b00110011,0b11001101,
	0b11001100,0b11001100,0b11001100,0b10101100,
	0b10011001,0b00110001,0b00110011,
	// kick (34)
	0b10010101,0b10110010,0b00000000,0b11100011,
	0b11110000,0b00000000,0b11111111,0b00000000,
	0b11111110,0b00000000,0b00000000,0b00000000,
	0b11111111,0b11111111,0b11111111,0b00100101,
	0b00000000,0b00000000,0b00000000,0b00000000,
	0b11111111,0b11110111,0b11111111,0b11111111,
	0b11111111,0b10111111,0b00010010,0b00000000,
	0b10000000,0b00000000,0b00000000,0b00000000,
	0b00000000,0b11101110,0b11111111,0b11111111,
	0b11111111,0b11110111,0b11111111,0b11111110,
	// snare (74)
	0b10011010,0b10011010,0b10101010,0b10010110,
	0b01110100,0b10010101,0b10001010,0b11011110,
	0b01110100,0b10100000,0b11110111,0b00100101,
	0b01110100,0b01101000,0b11111111,0b01011011,
	0b01000001,0b10000000,0b11010100,0b11111101,
	0b11011110,0b00010010,0b00000100,0b00100100,
	0b11101101,0b11111011,0b01011011,0b00100101,
	0b00000100,0b10010001,0b01101010,0b11011111,
	0b01110111,0b00010101,0b00000010,0b00100010,
	0b11010101,0b01111010,0b11101111,0b10110110,
	0b00100100,0b10000100,0b10100100,0b11011010,
	// hi-hat (118)
	0b10011010,0b01110100,0b11010100,0b00110011,
	0b00110011,0b11101000,0b11101000,0b01010101,
	0b01010101,
	// end (26)
};

// channel variables (grouped in arrays)
unsigned char  out              [CHANNELS],
               octave           [CHANNELS],
               length           [CHANNELS],
               volume           [CHANNELS],
               loops_active     [CHANNELS],
               current_length   [CHANNELS];
unsigned int   data_pointer     [CHANNELS],
               waveform         [CHANNELS],
               pitch_counter    [CHANNELS],
               frequency        [CHANNELS],
               loop_duration    [MAXLOOPS][CHANNELS],
               loop_point       [MAXLOOPS][CHANNELS],
               pointer_location [CHANNELS];

int main(void)
{
	// sampler variables
	unsigned char  currentByte     = 0,
	               currentBit      = 0,
	               sample_counter  = 0,
	               current_sample  = 0;

	// temporary data storage variables
	unsigned char  buffer1         = 0,
	               buffer2         = 0,
	               buffer3         = 0;
	unsigned int   buffer4         = 0;

	// main timer variables
	unsigned int   tick_counter    = 0,
	               tick_speed      = 1024; // default tempo

	/* Set each channel's data pointer to that channel's data location in the core data array.
	 * Initialise each channel's frequencies. By default they're set to zero which causes out of
	 * tune notes (due to timing errors) until every channel is assigned frequency data.
	 * Additionally, default values are set should no volume or octave be specified */

	for(unsigned char i=0; i<CHANNELS; i++)
	{
		data_pointer[i] = pgm_read_byte(&data[(i*2)]) << 8;
		data_pointer[i] = data_pointer[i] | pgm_read_byte(&data[(i*2)+1]);
		frequency[i]    = 255; // random frequency (won't ever be sounded)
		volume[i]       = 1;   // default volume : 50% pulse wave
		octave[i]       = 3;   // default octave : o3
	}

	// initialise output pin
	DDRB = 0b00000001 << OUTPUT;

	while(1){
		/* The code below lowers the volume of the sample channel when the volume is changed,
		 * but slows the routine by a tone... Not desired right now, but could be interesting in
		 * future.
		 *
		 * if(volume[3] > 2)
		 * 	PORTB = 0;
		 */

		/**********************
		 *  Synthesizer Code  *
		 **********************/

		// sampler (channel D) code
		if(sample_counter-- == 0)
		{
			if(currentByte < current_sample - 1)
			{
				// read individual bits from the sample array
				out[3] = ((pgm_read_byte(&sample[currentByte]) >> currentBit++) & 1) << OUTPUT;
			}
			else
			{
				/* Waste the same number of clock cycles as it takes to process the above to
				 * prevent the pitch from changing when the sampler isn't playing. */
				for(unsigned char i=0; i<8; i++)
					asm("nop;nop;");

				// silence the channel when the sample is over
				out[3] = 0;
			}

			// move to the next byte on bit pointer overflow
			if(currentBit > 7)
			{
				currentByte ++;
				currentBit = 0;
			}
			sample_counter = SAMPLE_SPEED;
		}

		/* Port changes (the demarcated 'output' commands) are carefully interleaved with
		 * generation code to balance volume of outputs. */

		// channel A (pulse 0 code)
		PORTB = out[0]; //output A (0)
		pitch_counter[0] += octave[0];
		if(pitch_counter[0] >= frequency[0])
			pitch_counter[0] = pitch_counter[0] - frequency[0];
		if(pitch_counter[0] <= waveform[0])
			out[0] = 1 << OUTPUT;
		PORTB = out[1]; //output B (1)
		if(pitch_counter[0] >= waveform[0])
			out[0] = 0;

		// channel B (pulse 1 code)
		pitch_counter[1] += octave[1];
		if(pitch_counter[1] >= frequency[1])
			pitch_counter[1] = pitch_counter[1] - frequency[1];
		PORTB = out[2]; //output C (2)
		if(pitch_counter[1] <= waveform[1])
			out[1] = 1 << OUTPUT;
		if(pitch_counter[1] >= waveform[1])
			out[1] = 0;

		// channel C (pulse 2 code)
		pitch_counter[2] += octave[2];
		if(pitch_counter[2] >= frequency[2])
			pitch_counter[2] = pitch_counter[2] - frequency[2];
		PORTB = out[3]; //output D (3)
		if(pitch_counter[2] <= waveform[2])
			out[2] = 1 << OUTPUT;
		if(pitch_counter[2] >= waveform[2])
			out[2] = 0;

		/**************************
		 *  Data Processing Code  *
		 **************************/

		if(tick_counter -- == 0)
		{
			// Variable tempo, sets the fastest / smallest possible clock event.
			tick_counter = tick_speed;

			for(unsigned char voice=0; voice<CHANNELS; voice++)
			{
				// If the note ended, start processing the next byte of data.
				if(length[voice] == 0){
					LOOP: // Yup, a goto, I know.

					// Temporary storage of data for quick processing.
					// first nibble of data
					buffer1 = (pgm_read_byte(&data[data_pointer[voice]]) >> 4) & 15;
					// second nibble of data
					buffer2 = pgm_read_byte(&data[data_pointer[voice]]) & 15;

					if(buffer1 == 15)
					{
						// Another buffer for commands that require an additional byte.
						buffer3 = pgm_read_byte(&data[data_pointer[voice]+1]);

						// loop start
						if(buffer2 == 0)
						{
							loops_active[voice]++;
							loop_point[loops_active[voice] - 1][voice] = data_pointer[voice] + 2;
							loop_duration[loops_active[voice] - 1][voice] = buffer3 - 1;
							data_pointer[voice]+= 2;
						}
						// loop end
						else if(buffer2 == 1)
						{
							/* If we reach the end of the loop and the duration isn't zero,
							 * decrement our counter and start again. */
							if(loop_duration[loops_active[voice] - 1][voice] > 0)
							{
								data_pointer[voice] = loop_point[loops_active[voice] - 1 ][voice];
								loop_duration[loops_active[voice] - 1][voice]--;
							}
							// If we're done, move away from the loop.
							else
							{
								loops_active[voice]--;
								data_pointer[voice]++;
							}
						}
						// macro
						else if(buffer2 == 2)
						{
							pointer_location[voice] = data_pointer[voice] + 2;

							data_pointer[voice] = pgm_read_byte(&data[(buffer3 + CHANNELS) * 2]) << 8;
							data_pointer[voice] = data_pointer[voice] | pgm_read_byte(&data[((buffer3 + CHANNELS) * 2) + 1]);
						}
						// tempo
						else if(buffer2 == 3)
						{
							tick_speed = buffer3 << 4;
							data_pointer[voice] += 2;
						}

						// transpose (currently unused)
						else if(buffer2 == 4)
							data_pointer[voice] += 2; // skip data

						// instrument (currently unused)
						else if(buffer2 == 5)
							data_pointer[voice] += 2; // skip data

						// tie command (currently unused)
						else if(buffer2 == 6)
							data_pointer[voice]++; // skip data

						else if(buffer2 == 15)
						{
							// If we've got a previous position saved, go to it...
							if(pointer_location[voice] != 0)
							{
								data_pointer[voice] = pointer_location[voice];
								pointer_location[voice] = 0;
							}
							// ...If not, go back to the start.
							else
							{
								data_pointer[voice] = pgm_read_byte(&data[(voice*2)]) << 8;
								data_pointer[voice] = data_pointer[voice] | pgm_read_byte(&data[(voice*2)+1]);
							}
						}

						/* For any command that should happen 'instantaneously' (e.g. anything
						 * that isn't a note or rest - in mmml notes can't be altered once
						 * they've started playing), we need to keep checking this loop until we
						 * find an event that requires waiting. */

						goto LOOP;
					}

					if(buffer1 == 13 || buffer1 == 14)
					{
						// octave
						if(buffer1 == 13)
							octave[voice] = 1 << buffer2;
						// volume
						if(buffer1 == 14)
							volume[voice] = buffer2;

						data_pointer[voice]++;
						goto LOOP; //see comment above previous GOTO
					}

					// note value
					if(buffer1 != 0 && buffer1 < 14)
					{
						if(voice < 3)
						{
							buffer4 = pgm_read_word(&note[buffer1]);
							frequency[voice] = buffer4;

							/* Calculate the waveform duty cycle by dividing the frequency by
							 * powers of two. */
							waveform[voice] = buffer4 >> volume[voice];
						}
						else
						{
							// reset the sampler
							currentBit = 0;
							currentByte = pgm_read_byte(&sample_index[buffer1 - 1]);
							current_sample = pgm_read_byte(&sample_index[buffer1]);
						}
					}
					// rest
					else
						waveform[voice] = 0;

					// note duration value
					if(buffer2 < 8)
						// standard duration
						length[voice] = 127 >> buffer2;
					else
						// dotted (1 + 1/2) duration
						length[voice] = 95 >> (buffer2 & 7);

					// next element in data
					data_pointer[voice]++;
				}
				// keep waiting until the note is over
				else
					length[voice]--;
			}
		}
	}
}

/************************************************************************************************/