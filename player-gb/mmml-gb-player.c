/*H************************************************************
*  FILENAME:      mmml-gb-player.c
*  DESCRIPTION:   Micro Music Macro Language (μMML) Player for
*                 Game Boy.
*
*  NOTES:         To be compiled with GBDK 3.0.
*
*                 A four-channel MML inspired 1-bit music
*                 player. Three channels of harmonic pulse
*                 wave and a percussive sampler or noise
*                 generator.
*
*                 Limits the capabilities of the sound engine
*                 to those operations available in μMML.
*                 Essentially, this approximates 1-bit music
*                 by linking the pulse width to the volume.
*                 Wave channel generates a pulse wave only and
*                 is shifted up an octave so that the range
*                 matches channels 1 and 2.
*
*  AUTHOR:        Blake 'PROTODOME' Troise
************************************************************H*/

#include <stdio.h>
#include <gb/gb.h>

#include "gb-mmml-data.c"

#define MAXLOOPS     5  // the maximum number of nested loops
#define TOTAL_VOICES 4  // total number of voices to synthesize

// values taken from 'http://www.devrs.com/gb/files/sndtab.html'
static UINT16 note[72] = 
{
//  c    c#   d    d#   e    f    f#   g    g#   a    a#   b
	44,  156, 262, 363, 457, 547, 631, 710, 786, 854, 923, 986,
	1046,1102,1155,1205,1253,1297,1339,1379,1417,1452,1486,1517,
	1546,1575,1602,1627,1650,1673,1694,1714,1732,1750,1767,1783,
	1798,1812,1825,1837,1849,1860,1871,1881,1890,1899,1907,1915,
	1923,1930,1936,1943,1949,1954,1959,1964,1969,1974,1978,1982,
	1985,1988,1992,1995,1998,2001,2004,2006,2009,2011,2013,2015
};

// channel variables (grouped in arrays)
INT8   transposition    [TOTAL_VOICES];
UINT8  octave           [TOTAL_VOICES];
UINT8  channel_volume   [TOTAL_VOICES];
UINT8  channel_timbre   [TOTAL_VOICES];
UINT8  length           [TOTAL_VOICES];
UINT8  loops_active     [TOTAL_VOICES];
UINT16 data_pointer     [TOTAL_VOICES];
UINT16 loop_duration    [MAXLOOPS][TOTAL_VOICES];
UINT16 loop_point       [MAXLOOPS][TOTAL_VOICES];
UINT16 pointer_location [TOTAL_VOICES];

// temporary data storage variables
UINT8  buffer1   = 0;
UINT8  buffer2   = 0;
UINT8  buffer3   = 0;
UINT16 buffer4   = 0;
UINT16 buffer5   = 0;

// correct tempo drift due to inconsistent
// processing time
UINT8 tempo_corrector = 0;
UINT8 no = 0;

UINT16 header_size   = 0;
UINT8  drum_duration = 0;

// main timer variables
UINT16 tick_counter = 1;
UINT16 tick_speed   = 0;

void update_wavetable(UINT8 volume)
{
	UINT8  position    = 0;
	UINT8  output_byte = 0;
	UINT8  sample      = 0;

	// change pulse width w/ volume
	if (volume > 10)
		sample = 4;
	if (volume > 8 && volume <= 10)
		sample = 3;
	if (volume > 3 && volume <= 8)
		sample = 2;
	if (volume <= 3)
		sample = 1;

	// switch off channel c dac
	NR32_REG = 0x00;
	NR30_REG = 0x00;

	// populate wavetable
	for (position = 0; position < 16; position++)
	{
		// run this twice to match the octave with the other channels
		if ((position & 0x07) < sample)
			output_byte = (volume << 4) | volume;
		else
			output_byte = 0;

		(*(UBYTE *) (0xFF30 + position)) = output_byte;
	}

	// switch on channel c dac
	NR30_REG = 0x80;
	NR32_REG = 0x20;
}

void update_channel_d(UINT16 frequency,
                      UINT8  volume,
                      UINT8  divisor,
                      UINT8  envelope_length,
                      UINT8  noise_mode
){
	NR51_REG |= 0x88;
	NR41_REG = 0x00;
	NR42_REG = 0x00 | (volume << 4) | (envelope_length & 0x7);
	NR43_REG = 0x00 | (frequency << 4) | ((noise_mode & 0x1) << 3) | (divisor & 0x7);
	NR44_REG = 0x80; 
}

void initialise_audio()
{
	// start-up
	NR52_REG = 0x80;
	NR50_REG = 0x77;
	NR51_REG = 0xFF;

	/* initialise channels */

	// channel 1
	NR10_REG = 0x00;
	NR11_REG = 0x00 | (0x00 << 6);

	// channel 2
	NR21_REG = 0x00 | (0x01 << 6);

	// channel 3
	NR30_REG = 0x80;
	NR31_REG = 0x00;
	NR32_REG = 0x20;
}

void main()
{
	UINT8 i;
	UINT8 voice;
	
	initialise_audio();

	// set up mmml variables
	for(i = 0; i < TOTAL_VOICES; i++)
	{
		data_pointer[i]  = source[(i*2)];
		data_pointer[i]  = data_pointer[i] << 8;
		data_pointer[i]  = data_pointer[i] | source[(i*2)+1];
		transposition[i] = 0;
		octave[i]        = 3;   // default octave : o3
	}

	header_size = data_pointer[0];

	while(1)
	{
		if(tick_counter-- == 0)
		{
			// Variable tempo, sets the fastest / smallest possible clock event.
			tick_counter = (tick_speed >> 2) + 130; //ugh, tempo fudge

			for(voice = 0; voice < TOTAL_VOICES; voice++)
			{
				// If the note ended, start processing the next byte of data.
				if(length[voice] == 0){
					LOOP:

					// Temporary storage of data for quick processing.
					// first nibble of data
					buffer1 = source[(data_pointer[voice])] >> 4;
					// second nibble of data
					buffer2 = source[data_pointer[voice]] & 0x0F;

					// function command
					if(buffer1 == 15)
					{
						// Another buffer for commands that require an additional byte.
						buffer3 = source[data_pointer[voice] + 1];

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
							
							data_pointer[voice] = source[(buffer3 + TOTAL_VOICES) * 2];
							data_pointer[voice] = data_pointer[voice] << 8;
							data_pointer[voice] = data_pointer[voice] | source[((buffer3 + TOTAL_VOICES) * 2) + 1];
						}
						// tempo
						else if(buffer2 == 3)
						{
							tick_speed = buffer3 << 4;
							data_pointer[voice]+=2;
						}
						// transpose
						else if(buffer2 == 4)
						{
							transposition[voice] = buffer3;
							data_pointer[voice]+=2;
						}

						// instrument (currently unused)
						else if(buffer2 == 5)
							data_pointer[voice] += 2; // skip data

						// tie command (currently unused)
						else if(buffer2 == 6)
							data_pointer[voice]++; // skip data

						// panning command (currently unused)
						else if(buffer2 == 7)
							data_pointer[voice] += 2; // skip data

						// channel end
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
								data_pointer[voice] = source[(voice * 2)];
								data_pointer[voice] = data_pointer[voice] << 8;
								data_pointer[voice] = data_pointer[voice] | source[(voice*2)+1];
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
							octave[voice] = buffer2;
						// volume
						if(buffer1 == 14)
						{
							if (voice == 0)
							{
								// change pulse width w/ volume
								if (buffer2 > 10)
									NR11_REG = 0x80;
								if (buffer2 > 4 && buffer2 <= 10)
									NR11_REG = 0x40;
								if (buffer2 <= 4)
									NR11_REG = 0x00;

								channel_volume[0] = buffer2;
							}
							if (voice == 1)
							{
								// change pulse width w/ volume
								if (buffer2 > 10)
									NR21_REG = 0x80;
								if (buffer2 > 4 && buffer2 <= 10)
									NR21_REG = 0x40;
								if (buffer2 <= 4)
									NR21_REG = 0x00;

								channel_volume[1] = buffer2;
							}
							if (voice == 2)
								update_wavetable(buffer2);
						}

						data_pointer[voice]++;
						goto LOOP; //see comment above previous GOTO
					}

					// note value
					if(buffer1 != 0 && buffer1 < 14)
					{
						if(voice < 3)
						{
							buffer1 += (octave[voice] * 12);
							buffer5 = note[buffer1 + transposition[voice]];

							if (voice == 0)
							{
								NR12_REG = 0x08 | (channel_volume[0] << 4);
								NR13_REG = buffer5 & 0x00FF;
								NR14_REG = 0x80 | (buffer5 >> 8);
							}
							if (voice == 1)
							{
								NR22_REG = 0x08 | (channel_volume[1] << 4);
								NR23_REG = buffer5 & 0x00FF;
								NR24_REG = 0x80 | (buffer5 >> 8);
							}
							if (voice == 2)
							{
								// switching on and off the wav channel
								// when triggering a note prevents the
								// data at 0xFF30 from randomly changing
								// ... wierd.
								NR30_REG = 0x00;
								NR30_REG = 0x80;

								// anyway, business as usual
								NR32_REG = 0x20;
								NR33_REG = buffer5 & 0x00FF;
								NR34_REG = 0x80 | (buffer5 >> 8);
							}
						}
						else
						{
							// create different percussive effects
							switch(buffer1)
							{
								case 1: // perc 1
									update_channel_d(0x04, 0xA, 0x02, 0x01, 0x00);
									drum_duration = 1;
									break;
								case 2: // perc 2
									update_channel_d(0x01, 0xC, 0x01, 0x01, 0x00);
									drum_duration = 200;
									break;
								case 3: // kick
									update_channel_d(0x04, 0xD, 0x04, 0x01, 0x00);
									drum_duration = 5;
									break;
								case 4: // snare
									update_channel_d(0x04, 0xA, 0x02, 0x01, 0x00);
									drum_duration = 20;
									break;
								case 5:  // hh
									update_channel_d(0x01, 0x6, 0x01, 0x01, 0x00);
									drum_duration = 6;
									break;
							}
						}
					}
					// rest
					else
					{
						// rather than silencing the channel, retriggering the
						// note with volume zero removes clicking on channels 1+2
						if (voice == 0)
						{
							NR12_REG = 0x08;
							NR13_REG = buffer5 & 0x00FF;
							NR14_REG = 0x80 | (buffer5 >> 8);
						}
						if (voice == 1)
						{
							NR22_REG = 0x08;
							NR23_REG = buffer5 & 0x00FF;
							NR24_REG = 0x80 | (buffer5 >> 8);
						}
						if (voice == 2)
							NR32_REG = 0x00;
					}

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

					// correct tempo (bit of a hack right now)
					for (tempo_corrector = 0; tempo_corrector < 40; ++tempo_corrector)
						no++;

					// silence drums
					if (drum_duration-- == 0)
						NR51_REG &= 0x77;
				}
			}
		}
	}
}