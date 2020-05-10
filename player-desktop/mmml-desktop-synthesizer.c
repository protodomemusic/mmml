/*H************************************************************
*  FILENAME:     mmml-desktop-synthesizer.c
*  DESCRIPTION:  Micro Music Macro Language (μMML) Desktop
*                Synthesizer
*
*  NOTES:        Converts .mmmldata files to .wav for playback.
*
*                The wave building code was adapted from:
*                https://stackoverflow.com/questions/23030980/
*                creating-a-mono-wav-file-using-c - thanks
*                Safayet Ahmed!
*
*                To generate a wave file, simply run:
*                $ ./synthesizer -f output.mmmldata -s SECONDS
*
*                WARNING - The wave files this generates are
*                huge because of the high sample rate. Should
*                be no problem in a world where 2TB HDDs are
*                standard, but just be aware that long files
*                will be in the order of hundreds of megabytes.
*
*  AUTHOR:       Blake 'PROTODOME' Troise
************************************************************H*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h> // needed for int types on Linux

#define SAMPLE_SPEED   5    // the sampler playback rate
#define SAMPLE_LENGTH  127  // the length of the sample array
#define MAXLOOPS       5    // the maximum number of nested loops
#define TOTAL_VOICES   4    // total number of 1-bit voices to synthesize
#define AMPLITUDE      127  // waveform high position (maximum from DC zero is 127)
#define DC_OFFSET      127  // waveform low position (127 is DC zero)

// terminal print colours
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"

typedef struct wavfile_header_s
{
    char    ChunkID[4];     /*  4   */
    int32_t ChunkSize;      /*  4   */
    char    Format[4];      /*  4   */

    char    Subchunk1ID[4]; /*  4   */
    int32_t Subchunk1Size;  /*  4   */
    int16_t AudioFormat;    /*  2   */
    int16_t NumChannels;    /*  2   */
    int32_t SampleRate;     /*  4   */
    int32_t ByteRate;       /*  4   */
    int16_t BlockAlign;     /*  2   */
    int16_t BitsPerSample;  /*  2   */

    char    Subchunk2ID[4];
    int32_t Subchunk2Size;
} wavfile_header_t;

#define SUBCHUNK1SIZE   (16)
#define AUDIO_FORMAT    (1) /*For PCM*/
#define NUM_CHANNELS    (1)
#define SAMPLE_RATE     (215000)

#define BITS_PER_SAMPLE (8)

#define BYTE_RATE       (SAMPLE_RATE * NUM_CHANNELS * BITS_PER_SAMPLE/8)
#define BLOCK_ALIGN     (NUM_CHANNELS * BITS_PER_SAMPLE/8)

char *source = NULL;
long bufsize;

/********************** µMML variables ***********************/

// note table (plus an initial 'wasted' entry for rests)
const unsigned int note[13] = 
{
	// the rest command is technically note 0 and thus requires a frequency 
	255,
	// one octave of notes, equal temperament
	1024,967,912,861,813,767,724,683,645,609,575,542
};

// location of individual samples in sample array
const unsigned char sample_index[6] =
{
    0,19,34,74,118,126
};

// raw PWM sample data
const unsigned char sample[SAMPLE_LENGTH] =
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

unsigned int header_size = 0;

// channel variables (grouped in arrays)
unsigned char  output           [TOTAL_VOICES],
               octave           [TOTAL_VOICES],
               length           [TOTAL_VOICES],
               volume           [TOTAL_VOICES],
               loops_active     [TOTAL_VOICES],
               current_length   [TOTAL_VOICES];
unsigned int   data_pointer     [TOTAL_VOICES],
               waveform         [TOTAL_VOICES],
               pitch_counter    [TOTAL_VOICES],
               frequency        [TOTAL_VOICES],
               loop_duration    [MAXLOOPS][TOTAL_VOICES],
               loop_point       [MAXLOOPS][TOTAL_VOICES],
               pointer_location [TOTAL_VOICES];

// sampler variables
unsigned char  current_byte    = 0,
               current_bit     = 0,
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


void error_message(char number)
{
	printf(ANSI_COLOR_RED "[ERROR %d] ",number);

	switch(number)
	{
		case 0 :
			printf("Generic error.\n");
			break;
		case 1 :
			printf("File either doesn't exist, or it can't be opened.\n");
			break;
		case 2 :
			printf("No file specified.\n");
			break;
	}
		printf(ANSI_COLOR_RESET);

	printf("\nTerminating due to error. Really sorry! μMML Desktop Synthesizer end. :(\n\n");

	exit(0);
}

/*Return 0 on success and -1 on failure*/
int write_PCM8_mono_header(FILE* file_p, int32_t SampleRate, int32_t FrameCount)
{
	int ret;

	wavfile_header_t wav_header;
	int32_t subchunk2_size;
	int32_t chunk_size;

	size_t write_count;

	subchunk2_size  = FrameCount * NUM_CHANNELS * BITS_PER_SAMPLE/8;
	chunk_size      = 4 + (8 + SUBCHUNK1SIZE) + (8 + subchunk2_size);

	wav_header.ChunkID[0] = 'R';
	wav_header.ChunkID[1] = 'I';
	wav_header.ChunkID[2] = 'F';
	wav_header.ChunkID[3] = 'F';

	wav_header.ChunkSize = chunk_size;

	wav_header.Format[0] = 'W';
	wav_header.Format[1] = 'A';
	wav_header.Format[2] = 'V';
	wav_header.Format[3] = 'E';

	wav_header.Subchunk1ID[0] = 'f';
	wav_header.Subchunk1ID[1] = 'm';
	wav_header.Subchunk1ID[2] = 't';
	wav_header.Subchunk1ID[3] = ' ';

	wav_header.Subchunk1Size = SUBCHUNK1SIZE;
	wav_header.AudioFormat = AUDIO_FORMAT;
	wav_header.NumChannels = NUM_CHANNELS;
	wav_header.SampleRate = SampleRate;
	wav_header.ByteRate = BYTE_RATE;
	wav_header.BlockAlign = BLOCK_ALIGN;
	wav_header.BitsPerSample = BITS_PER_SAMPLE;

	wav_header.Subchunk2ID[0] = 'd';
	wav_header.Subchunk2ID[1] = 'a';
	wav_header.Subchunk2ID[2] = 't';
	wav_header.Subchunk2ID[3] = 'a';
	wav_header.Subchunk2Size = subchunk2_size;

	write_count = fwrite(&wav_header, sizeof(wavfile_header_t), 1, file_p);

	ret = (1 != write_count)? -1 : 0;

	return ret;
}

typedef struct PCM8_mono_s
{
	uint8_t mono;
} PCM8_mono_t;

PCM8_mono_t *allocate_PCM8_mono_buffer(int32_t FrameCount)
{
	return (PCM8_mono_t *)malloc(sizeof(PCM8_mono_t) * FrameCount);
}

/*Return the number of audio frames sucessfully written*/
size_t  write_PCM8_wav_data(FILE* file_p, int32_t FrameCount, PCM8_mono_t *buffer_p)
{
	size_t ret;

	ret = fwrite(buffer_p, sizeof(PCM8_mono_t), FrameCount, file_p);

	return ret;
}

void read_file(char *file_name)
{
	FILE *input_file = fopen(file_name, "r");

	if (input_file != NULL)
	{
		printf("Reading file: '%s'.\n", file_name);

		// jump to the end of the input file
		if (fseek(input_file, 0L, SEEK_END) == 0)
		{
			// get size of file
			bufsize = ftell(input_file);

			printf("Input file size: %ld bytes.\n\n", bufsize);

			if (bufsize == -1)
				error_message(0);
	
			// allocate memory
			source = (char*)malloc(sizeof(char) * (bufsize + 1));
	
			if (fseek(input_file, 0L, SEEK_SET) != 0)
				error_message(0);
	
			// read file into memory
			size_t newLen = fread(source, sizeof(char), bufsize, input_file);

			if ( ferror( input_file ) != 0 )
				error_message(1);
			else
				source[newLen++] = '\0';
		}
		fclose(input_file);
	}
	else
		error_message(1);
}

int generate_audio( int32_t SampleRate, int32_t FrameCount, PCM8_mono_t  *buffer_p, char *source)
{
    int ret = 0;
    uint32_t k;

	//===== WAVE SAMPLE DATA CODE =====//

	printf("Synthesising wave file...\n");

	unsigned int v = 0;

	// set up mmml variables
	for(unsigned char i = 0; i < TOTAL_VOICES; i++)
	{
		data_pointer[i] = (unsigned char)source[(i*2)] << 8;
		data_pointer[i] = data_pointer[i] | (unsigned char)source[(i*2)+1];

		frequency[i]    = 255; // random frequency (won't ever be sounded)
		volume[i]       = 1;   // default volume : 50% pulse wave
		octave[i]       = 3;   // default octave : o3
	}

	header_size = data_pointer[0];

	for(k = 0; k < FrameCount;)
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
				output[3] = ((sample[current_byte] >> current_bit++) & 1);
			}
			else
				// silence the channel when the sample is over
				output[3] = 0;

			// move to the next byte on bit pointer overflow
			if(current_bit > 7)
			{
				current_byte++;
				current_bit = 0;
			}
			sample_counter = SAMPLE_SPEED;
		}

		// calculate pulse values
		for(unsigned char voice = 0; voice < TOTAL_VOICES - 1; voice++){
			pitch_counter[voice] += octave[voice];
			if(pitch_counter[voice] >= frequency[voice])
				pitch_counter[voice] = pitch_counter[voice] - frequency[voice];
			if(pitch_counter[voice] <= waveform[voice])
				output[voice] = 1;
			if(pitch_counter[voice] >= waveform[voice])
				output[voice] = 0;
		}

		// output and interleave samples using PIM
		for(unsigned char voice = 0; voice < TOTAL_VOICES; voice++)
			buffer_p[k++].mono = (output[voice] * AMPLITUDE) + DC_OFFSET;

		/**************************
		 *  Data Processing Code  *
		 **************************/

		if(tick_counter -- == 0)
		{
			// Variable tempo, sets the fastest / smallest possible clock event.
			tick_counter = tick_speed;

			for(unsigned char voice = 0; voice < TOTAL_VOICES; voice++)
			{
				// If the note ended, start processing the next byte of data.
				if(length[voice] == 0){
					LOOP:

					// Temporary storage of data for quick processing.
					// first nibble of data
					buffer1 = ((unsigned char)source[(data_pointer[voice])] >> 4);
					// second nibble of data
					buffer2 = (unsigned char)source[data_pointer[voice]] & 0x0F;

					// function command
					if(buffer1 == 15)
					{
						// Another buffer for commands that require an additional byte.
						buffer3 = (unsigned char)source[data_pointer[voice] + 1];

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
							
							data_pointer[voice] = (unsigned char)source[(buffer3 + TOTAL_VOICES) * 2] << 8;
							data_pointer[voice] = data_pointer[voice] | (unsigned char)source[((buffer3 + TOTAL_VOICES) * 2) + 1];
						}
						// tempo
						else if(buffer2 == 3)
						{
							tick_speed = buffer3 << 4;
							data_pointer[voice] += 2;
						}
						// relative volume decrease
						else if(buffer2 == 4)
						{
							if(volume[voice] < 8)
								volume[voice]++;
							data_pointer[voice]++;
						}
						// relative volume increase
						else if(buffer2 == 5)
						{
							if(volume[voice] > 1)
								volume[voice]--;
							data_pointer[voice]++;
						}
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
								data_pointer[voice] = (unsigned char)source[(voice*2)] << 8;
								data_pointer[voice] = data_pointer[voice] | (unsigned char)source[(voice*2)+1];
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
							buffer4 = note[buffer1];
							frequency[voice] = buffer4;

							/* Calculate the waveform duty cycle by dividing the frequency by
							 * powers of two. */
							waveform[voice] = (buffer4 >> volume[voice])+1;
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
					length[voice]--;
			}
		}
	}

	error0:
    	return ret;
}

int main(int argc, char *argv[])
{
    int ret;
    FILE* file_p;

    double duration = 10; // seconds

    PCM8_mono_t  *buffer_p = NULL;

    size_t written;

	printf("Hello and Welcome to the μMML Desktop Synthesizer! (v1.2)\n\n");

	if(argc == 1){
		printf("No arguments!");
		exit(1);
	}

	/* Open the mmmldata file */
	for (uint8_t i = 1; i < argc; i++){
		if (strcmp(argv[i], "-f") == 0){
			if(i == argc-1)
				error_message(0);
			else
				read_file(argv[i+1]);
			i++;
		}
		else if (strcmp(argv[i], "-s") == 0){
			if(i == argc-1)
				error_message(0);
			else
				duration = atoi(argv[i+1]);
			i++;
		}
		else
			error_message(0);
	}

	uint32_t FrameCount = duration * SAMPLE_RATE;

	/*Open the wav file*/
	file_p = fopen("./output.wav", "w");
	if(NULL == file_p)
	{
		perror("fopen failed in main");
		ret = -1;
		goto error0;
	}

	/*Allocate the data buffer*/
	buffer_p = allocate_PCM8_mono_buffer(FrameCount);
	if(NULL == buffer_p)
	{
		perror("fopen failed in main");
		ret = -1;
		goto error1;        
	}

	/*Fill the buffer*/
	ret = generate_audio(SAMPLE_RATE, FrameCount, buffer_p, source);
	if(ret < 0)
	{
		fprintf(stderr, "generate_audio failed in main\n");
		ret = -1;
		goto error2;
	}

	/*Write the wav file header*/
	ret = write_PCM8_mono_header(file_p, SAMPLE_RATE, FrameCount);
	if(ret < 0)
	{
		perror("write_PCM8_mono_header failed in main");
		ret = -1;
		goto error2;
	}

	/*Write the data out to file*/
	written = write_PCM8_wav_data(file_p, FrameCount, buffer_p);
	if(written < FrameCount)
	{
		perror("write_PCM8_wav_data failed in main");
		ret = -1;
		goto error2;
	}

	printf("\nCompilation finished! μMML Desktop Synthesizer end.\n\n");

	/*Free and close everything*/    
	error2:
		free(buffer_p);
	error1:
		fclose(file_p);
	error0:
		return ret;    
}