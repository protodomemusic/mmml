/**************************************************************
* This is where all the sequencing happens.
*
* Feed this .mmmldata files, the number of samples (frames) you
* want and it'll send 1-bit (well, technically 8-bit) mono
* samples to the input audio buffer.
**************************************************************/

#define SAMPLE_SPEED   5    // the sampler playback rate
#define SAMPLE_LENGTH  127  // the length of the sample array
#define MAXLOOPS       5    // the maximum number of nested loops
#define TOTAL_VOICES   4    // total number of 1-bit voices to synthesize
#define AMPLITUDE      127  // waveform high position (maximum from DC zero is 127)
#define DC_OFFSET      127  // waveform low position (127 is DC zero)

void generate_mmml(uint8_t *input_buffer, int32_t total_samples, char *mmml_source)
{
	// note table (plus an initial 'wasted' entry for rests)
	const uint16_t note[13] =
	{
		// the rest command is technically note 0 and thus requires a frequency
		255,
		// one octave of notes, equal temperament
		1024,967,912,861,813,767,724,683,645,609,575,542
	};

	// location of individual samples in sample array
	const uint8_t sample_index[6] =
	{
	    0,19,34,74,118,126
	};

	// raw PWM sample data
	const uint8_t sample[SAMPLE_LENGTH] =
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
		// end (126)
	};
	// channel variables (grouped in arrays)
	uint8_t        output           [TOTAL_VOICES] = {0};
	uint8_t        octave           [TOTAL_VOICES] = {3}; // default octave : o3
	uint8_t        volume           [TOTAL_VOICES] = {1}; // default volume : 50% pulse wave
	uint8_t        length           [TOTAL_VOICES] = {0};
	uint8_t        loops_active     [TOTAL_VOICES] = {0};
	uint8_t        current_length   [TOTAL_VOICES] = {0};
	uint16_t       frequency        [TOTAL_VOICES] = {255}; // random frequency (won't ever be sounded)
	uint16_t       data_pointer     [TOTAL_VOICES] = {0};
	uint16_t       waveform         [TOTAL_VOICES] = {0};
	uint16_t       pitch_counter    [TOTAL_VOICES] = {0};
	uint16_t       loop_duration    [MAXLOOPS][TOTAL_VOICES];
	uint16_t       loop_point       [MAXLOOPS][TOTAL_VOICES];
	uint16_t       pointer_location [TOTAL_VOICES];

	// sampler variables
	uint8_t        current_byte    = 0;
	uint8_t        current_bit     = 0;
	uint8_t        sample_counter  = 0;
	uint8_t        current_sample  = 0;

	// temporary data storage variables
	uint8_t        buffer1         = 0;
	uint8_t        buffer2         = 0;
	uint8_t        buffer3         = 0;
	uint16_t       buffer4         = 0;

	// main timer variables
	uint16_t       tick_counter    = 0;
	uint16_t       tick_speed      = 0;

	// misc
	uint16_t       header_size     = 0;

	//===== WAVE SAMPLE DATA CODE =====//

	printf("Let's synthesize...\n");

	uint16_t v = 0;

	// set up mmml variables
	for (uint8_t i = 0; i < TOTAL_VOICES; i++)
	{
		data_pointer[i] = (uint8_t)mmml_source[(i*2)] << 8;
		data_pointer[i] = data_pointer[i] | (uint8_t)mmml_source[(i*2)+1];
	}

	header_size = data_pointer[0];

	for(uint32_t k = 0; k < total_samples;)
	{
		/**********************
		 *  Synthesizer Code  *
		 **********************/

		// sampler (channel D) code
		if(sample_counter-- == 0)
		{
			if(current_byte < current_sample - 1)
			{
				// read individual bits from the sample array
				output[TOTAL_VOICES - 1] = ((sample[current_byte] >> current_bit++) & 1);
			}
			else
				// silence the channel when the sample is over
				output[TOTAL_VOICES - 1] = 0;

			// move to the next byte on bit pointer overflow
			if(current_bit > 7)
			{
				current_byte++;
				current_bit = 0;
			}
			sample_counter = SAMPLE_SPEED;
		}

		// calculate pulse values
		for (uint8_t v = 0; v < TOTAL_VOICES - 1; v++)
		{
			pitch_counter[v] += octave[v];
			if(pitch_counter[v] >= frequency[v])
				pitch_counter[v] = pitch_counter[v] - frequency[v];
			if(pitch_counter[v] <= waveform[v])
				output[v] = 1;
			if(pitch_counter[v] >= waveform[v])
				output[v] = 0;
		}

		// output and interleave samples using PIM
		for (uint8_t v = 0; v < TOTAL_VOICES; v++)
			input_buffer[k++] = (output[v] * AMPLITUDE) + DC_OFFSET;

		/**************************
		 *  Data Processing Code  *
		 **************************/

		if(tick_counter -- == 0)
		{
			// Variable tempo, sets the fastest / smallest possible clock event.
			tick_counter = tick_speed;

			for (uint8_t v = 0; v < TOTAL_VOICES; v++)
			{
				// If the note ended, start processing the next byte of data.
				if(length[v] == 0)
				{
					LOOP:

					// Temporary storage of data for quick processing.
					// first nibble of data
					buffer1 = ((uint8_t)mmml_source[(data_pointer[v])] >> 4);
					// second nibble of data
					buffer2 = (uint8_t)mmml_source[data_pointer[v]] & 0x0F;

					// function command
					if(buffer1 == 15)
					{
						// Another buffer for commands that require an additional byte.
						buffer3 = (uint8_t)mmml_source[data_pointer[v] + 1];

						// loop start
						if(buffer2 == 0)
						{
							loops_active[v]++;
							loop_point[loops_active[v] - 1][v] = data_pointer[v] + 2;
							loop_duration[loops_active[v] - 1][v] = buffer3 - 1;
							data_pointer[v]+= 2;
						}
						// loop end
						else if(buffer2 == 1)
						{
							/* If we reach the end of the loop and the duration isn't zero,
							 * decrement our counter and start again. */
							if(loop_duration[loops_active[v] - 1][v] > 0)
							{
								data_pointer[v] = loop_point[loops_active[v] - 1 ][v];
								loop_duration[loops_active[v] - 1][v]--;
							}
							// If we're done, move away from the loop.
							else
							{
								loops_active[v]--;
								data_pointer[v]++;
							}
						}
						// macro
						else if(buffer2 == 2)
						{
							pointer_location[v] = data_pointer[v] + 2;

							data_pointer[v] = (uint8_t)mmml_source[(buffer3 + TOTAL_VOICES) * 2] << 8;
							data_pointer[v] = data_pointer[v] | (uint8_t)mmml_source[((buffer3 + TOTAL_VOICES) * 2) + 1];
						}
						// tempo
						else if(buffer2 == 3)
						{
							tick_speed = buffer3 << 4;
							data_pointer[v] += 2;
						}
						// transpose (currently unused)
						else if(buffer2 == 4)
							data_pointer[v] += 2; // skip data

						// instrument (currently unused)
						else if(buffer2 == 5)
							data_pointer[v] += 2; // skip data

						// tie command (currently unused)
						else if(buffer2 == 6)
							data_pointer[v]++; // skip data

						// panning command (currently unused)
						else if(buffer2 == 7)
							data_pointer[v] += 2; // skip data

						// debug pointer flag
						else if(buffer2 == 14)
						{
							printf("Flag location: %u\n",data_pointer[v]); // report data pointer location
							data_pointer[v]++; // skip data
						}
						// channel end
						else if(buffer2 == 15)
						{
							// If we've got a previous position saved, go to it...
							if(pointer_location[v] != 0)
							{
								data_pointer[v] = pointer_location[v];
								pointer_location[v] = 0;
							}
							// ...If not, go back to the start.
							else
							{
								data_pointer[v] = (uint8_t)mmml_source[(v*2)] << 8;
								data_pointer[v] = data_pointer[v] | (uint8_t)mmml_source[(v*2)+1];
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
							octave[v] = 1 << buffer2;
						// volume
						if(buffer1 == 14)
							volume[v] = buffer2;

						data_pointer[v]++;
						goto LOOP; //see comment above previous GOTO
					}

					// note value
					if(buffer1 != 0 && buffer1 < 14)
					{
						if(v < TOTAL_VOICES - 1)
						{
							buffer4 = note[buffer1];
							frequency[v] = buffer4;

							/* Calculate the waveform duty cycle by dividing the frequency by
							 * powers of two. */
							waveform[v] = (buffer4 >> volume[v])+1;
						}
						else
						{
							// reset the sampler
							current_bit = 0;
							current_byte = sample_index[buffer1 - 1];
							current_sample = sample_index[buffer1];
						}
					}
					// rest
					else
						waveform[v] = 0;

					// note duration value
					if(buffer2 < 8)
						// standard duration
						length[v] = 127 >> buffer2;
					else
						// dotted (1 + 1/2) duration
						length[v] = 95 >> (buffer2 & 7);

					// next element in data
					data_pointer[v]++;
				}
				// keep waiting until the note is over...
				else
					length[v]--;
			}
		}
	}
}
