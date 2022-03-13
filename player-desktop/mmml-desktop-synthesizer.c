/**************************************************************
*  FILENAME:     mmml-desktop-synthesizer.c
*  DESCRIPTION:  1-Bit Micro Music Macro Language (μMML)
*                Desktop Synthesizer
*
*  NOTES:        Converts .mmmldata files to .wav for playback
*                of 1-bit mmml data.
*
*                To generate a wave file, simply run:
*                $ ./synthesizer -f output.mmmldata -s SECONDS
*
*                WARNING - The wave files this generates are
*                huge because of the high sample rate required
*                with 1-bit music. It should be no problem in
*                a world where 2TB HDDs are standard, but just
*                be aware that long files will be in the order
*                of hundreds of megabytes.
* 
*                (This is main.c btw.)
*
*  AUTHOR:       Blake 'PROTODOME' Troise
**************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define TOTAL_CHANNELS  1       // mono will do
#define SAMPLE_RATE     215000  // 1-bit music demands high sample rates!

#include "wave-export.c"
#include "mmml-engine.c"

// terminal print colours
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// where we'll put our input data
char *mmml_source = NULL;

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
		case 3 :
			printf("No duration specified.\n");
			break;
		case 4 :
			printf("Too few variables specified.\nTo select a file, use the '-f' flag.\nTo choose the length of the wave file in seconds, use the '-s' flag.\n");
			break;
		default :
			printf("Generic error.\n");
			break;
	}
		printf(ANSI_COLOR_RESET);

	printf("\nTerminating due to error. Really sorry! μMML Desktop Synthesizer end. :(\n\n");

	exit(0);
}

void read_file(char *file_name)
{
	uint32_t input_buffer_size;

	FILE *input_file = fopen(file_name, "r");

	if (input_file != NULL)
	{
		printf("Reading file: '%s'.\n", file_name);

		// jump to the end of the input file
		if (fseek(input_file, 0L, SEEK_END) == 0)
		{
			// get size of file
			input_buffer_size = ftell(input_file);

			printf("Input file size: %u bytes.\n\n", input_buffer_size);

			if (input_buffer_size == -1)
				error_message(0);
	
			// allocate memory
			mmml_source = (char*)malloc(sizeof(char) * (input_buffer_size + 1));
	
			if (fseek(input_file, 0L, SEEK_SET) != 0)
				error_message(0);
	
			// read file into memory
			size_t newLen = fread(mmml_source, sizeof(char), input_buffer_size, input_file);

			if ( ferror( input_file ) != 0 )
				error_message(1);
			else
				mmml_source[newLen++] = '\0';
		}
		fclose(input_file);
	}
	else
		error_message(1);
}

int main(int argc, char *argv[])
{
    uint32_t duration = 10; // seconds

	printf("Hello and Welcome to the μMML 1-Bit Desktop Synthesizer! (v1.3)\n\n");

	if(argc == 1)
	{
		error_message(4);
		exit(1);
	}

	if(argc < 4)
		error_message(4);

	/* Open the mmmldata file */
	for (uint8_t i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-f") == 0)
		{
			if(i == argc-1)
				error_message(2);
			else
				read_file(argv[i+1]);
			i++;
		}
		else if (strcmp(argv[i], "-s") == 0)
		{
			if(i == argc-1)
				error_message(3);
			else
			{
				duration = atoi(argv[i+1]);
				printf("Duration is set to %u seconds long.\n\n", duration);
			}
			i++;
		}
		else
			error_message(0);
	}

	// where we'll stick the final audio data
    uint32_t total_samples = duration * SAMPLE_RATE;
	uint8_t  *audio_buffer = (uint8_t*)malloc(total_samples * sizeof(uint8_t));

	// generate mmml audio
	generate_mmml(audio_buffer, total_samples, mmml_source);

	// write to file
	wave_export(audio_buffer, total_samples);

	printf(ANSI_COLOR_GREEN "\nWave file (output.wav) written successfully.\n\n" ANSI_COLOR_RESET "μMML 1-Bit Desktop Synthesizer end.\n\n");
}