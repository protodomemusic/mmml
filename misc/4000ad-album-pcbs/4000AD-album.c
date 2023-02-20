/*H************************************************************
*  FILENAME:      mmml-album.c
*  DESCRIPTION:   Micro Music Macro Language (μMML) Player for
*                 AVR microcontrollers.
*
*  NOTES:         A four-channel MML inspired 1-bit music
*                 player. Three channels of harmonic pulse
*                 wave and a percussive sampler or noise
*                 generator.
*
*                 This version allow the user to skip through
*                 tracks and is specifically tailored to the
*                 4000AD album, released in 2020.
*
*  AUTHOR:        Blake 'PROTODOME' Troise
*  PLATFORM:      Atmega328 @ 8MHz
*  DATE:          Version 0.1 - 10th May 2017
*                 Version 0.5 - 31st December 2017
*                 Version 1   - 9th April 2018
*                 Version 1.5 - 16th August 2018
*                 Version 1.6 - 12th January 2020
*                    * Forked to add track selection functions
*                      for album.
************************************************************H*/

// stuff you can mess with (especially the sampler)
#define OUTPUT         0    // the PORTB hardware output pin

// stuff you shouldn't mess with
#define CHANNELS       4    // the number of channels
#define SAMPLE_SPEED   5    // the sampler playback rate
#define SAMPLE_LENGTH  127  // the length of the sample array
#define MAXLOOPS       5    // the maximum number of nested loops
#define TOTAL_TRACKS   5    // the number of tracks on the album

#include <avr/io.h>         // core avr functionality
#include <avr/pgmspace.h>   // * * *
#include <avr/eeprom.h>     // * * *
#include "musicdata.h"      // holds the data[] and data_index[] arrays

// note table (plus an initial 'wasted' entry for rests)
const unsigned int note[13] PROGMEM = 
{
	// the rest command is technically note 0 and thus requires a frequency 
	255,
	// one octave of notes, equal temperament in Gb
	1024,967,912,861,813,767,724,683,645,609,575,542
	// 795,750,708,669,631,596,562,531,501,473,446,421,
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

// album-specific variables
unsigned char current_track;

// locations for different track starting values
const unsigned int track_data_index[TOTAL_TRACKS][CHANNELS] PROGMEM = 
{
	// tiny jazz album
	//{0,   1734,2540,3550},
	//{583, 1948,2802,3649},
	//{1128,2165,3120,3780}

	// 4000ad
	{0   ,6474,10169,14143},
	{1961,7366,11408,14564},
	{2494,7788,11829,14690},
	{4047,8566,12529,15062},
	{5041,9832,13702,15276}

};

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
	               tick_speed      = 0;

	// initialise output pin
	DDRB = 0b00000001 << OUTPUT;

	// get the current track from eeprom
	uint8_t current_track = eeprom_read_byte((uint8_t*)30);

	// change track for next startup
	eeprom_write_byte((uint8_t*)30,(current_track+1)%TOTAL_TRACKS);

	// startup values, including track selection
	for(unsigned char i=0; i<CHANNELS; i++)
	{
		frequency[i]    = 255; // random frequency (won't ever be sounded)
		volume[i]       = 1;   // default volume : 50% pulse wave
		octave[i]       = 3;   // default octave : o3
		data_pointer[i] = pgm_read_word(&track_data_index[current_track][i]);
	}

	while(1)
	{
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

		if(tick_counter-- == 0)
		{
			// Variable tempo, sets the fastest / smallest possible clock event.
			tick_counter = tick_speed;

			for(unsigned char voice=0; voice<CHANNELS; voice++)
			{
				// If the note ended, start processing the next byte of data.
				if(length[voice] == 0){
					LOOP:

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
							loop_point[loops_active[voice] - 1][voice] = data_pointer[voice] + 3;
							loop_duration[loops_active[voice] - 1][voice] = buffer3;
							data_pointer[voice] += 2;
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
							data_pointer[voice] = pgm_read_word(&data_index[buffer3 + CHANNELS]);
						}
						// tempo
						else if(buffer2 == 3)
						{
							tick_speed = buffer3 << 4;
							data_pointer[voice] += 2;
						}
						else if(buffer2 == 14)
						{
							current_track = (current_track + 2) % TOTAL_TRACKS;
							eeprom_write_byte((uint8_t*)30,current_track);
							data_pointer[voice]++;
						}
						// channel/macro end
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
								data_pointer[voice] = pgm_read_word(&data_index[voice]);
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
							octave[voice] = buffer2 + 1;
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
				// keep waiting until the note is over...
				else
				{
					length[voice]--;
				}
			}
		}
	}
}

/************************************************************************************************/