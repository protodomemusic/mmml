/*H************************************************************
*  FILENAME:     mmml-compiler.c
*  DESCRIPTION:  Micro Music Macro Language (μMML) Compiler
*
*  NOTES:        Converts .txt files (or .mmml files) to an
*                .h include file for the μMML AVR program.
*
*                [WARNING] THIS CODE IS HOT GARBAGE - but it
*                works, so it's only 50% garbage at worst.
*
*                To compile, simply run:
*                   $ ./mmml-compiler FILENAME.txt
*                or if you're using the .mmml suffix
*                   $ ./mmml-compiler FILENAME.mmml
*
*  AUTHOR:       Blake 'PROTODOME' Troise
************************************************************H*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h> // needed for int types on Linux

// yes, I should have used malloc
#define INPUT_FILE_SIZE       84000
#define OUTPUT_FILE_SIZE      84000
#define TOTAL_POSSIBLE_MACROS 255  // the player can't see more than this

// total number of channels before macros
#define CHANNELS 4

// terminal print colours
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"

static void error_message(char number, int line);
static void write_file();
static void next_byte();
static void print_duration(int number,FILE *newfile);
static void check_line(FILE *newfile);

signed char   line_counter = -1;
char          input_data[INPUT_FILE_SIZE],
              loop_temp_string[4],
              tempo_temp_string[4],
              macro_temp_string[4];

unsigned int  data_index[CHANNELS + TOTAL_POSSIBLE_MACROS],
              previousDuration = 4; // default duration (if none specified) is a crotchet

unsigned char command,
              channel,
              octave,
              loops,
              macro_line;

unsigned int  total_bytes,
              line = 1,
              highest_macro,
              loop_value,
              tempo_value,
              macro_value;

int main(int argc, char *argv[]){
	unsigned int i;
	unsigned char input;

	printf("μMML Compiler - Version 1.0\n\n");

	if(argc != 2)
		error_message(2,0);
	else{
		FILE *file;
		file = fopen(argv[1], "r");
	
		if(file){
			printf("Reading...\n\n");
			while ((input = getc(file)) != 255)
			{	
				input_data[i] = input;

				//remove spaces
				if(input_data[i]!=' ')
					i++;

				if(i >= INPUT_FILE_SIZE)
					error_message(0,0);
			}
			printf("Read %d elements.\n",i);
			fclose(file);
		}
		else
			error_message(1,0);
	}
	
	printf("\nWriting...\n\n");
	write_file();
	printf("\nμMML compiler end. ~<3\n\n");
}

void error_message(char number, int line){
	printf(ANSI_COLOR_RED "[ERROR %d] ",number);

	line++;

	switch(number){
		case 0 :
			printf("Input exceeded buffer. (64KB Maximum.)\n");
			break;
		case 1 :
			printf("File either doesn't exist, or it can't be opened.\n");
			break;
		case 2 :
			printf("No file specified.\n");
			break;
		case 3 :
			printf("Line %d: Consecutive commands (missing a value).\n\nAlso, there are no two character commands in μMML.\n", line);
			break;
		case 4 :
			printf("Line %d: Consecutive (numerical) values - invalid number.\n\nYou might have entered a number that the command doesn't support. Additionally, the compiler could be expecting this value to be a certain number of digits in length and you might have exceeded that.\n", line);
			break;
		case 5 :
			printf("Line %d: Not a valid note duration.\n\nThe available values here are: 128,64,64.,32,32.,16,16.,8,8.,4,4.,2,2.,1\n", line);
			break;
		case 6 :
			printf("Line %d: Invalid octave value; yours has gone above 'o5'.\n\nThis is easily missed with the '>' command. Check to see where you've incremented over the five octave limit.\n", line);
			break;
		case 7 :
			printf("Line %d: Volumes only range from 1 to 8. (1 quietest - 8 loudest)\n", line);
			break;
		case 8 :
			printf("Line %d: Invalid loop amount, 2 - 255 only.\n\nRemember, a loop refers to how many times the nested material should play, so a value of 0 and 1 would be redundant.\n", line);
			break;
		case 11 :
			printf("Line %d: Invalid octave value; yours has gone below 'o1'.\n\nThis is easily missed with the '<' command. Check to see where you've decremented below the base octave value.\n", line);
			break;
		case 12 :
			printf("Line %d: Invalid macro value.\n\nWhat, is 255 not enough for you?\n", line);
			break;
		case 13 :
			printf("Line %d: Invalid macro value; referred to a macro that doesn't exist.\n", line);
			break;
		case 14 :
			printf("Too few channels stated!\n");
			break;
		case 15 :
			printf("Line %d: Invalid macro value; must be non-zero.\n", line);
			break;
		case 16 :
			printf("Line %d: Invalid tempo; exceeded maximum value.\n\nTempos are represented by 8-bit numbers, therefore the highest value possible is 255.\n", line);
			break;
		case 17 :
			printf("Line %d: Invalid tempo; must be non-zero.\n\nThe compiler will only accept tempi from 1 to 255.\n", line);
			break;
		case 18 :
			printf("Line %d: Invalid loop amount; exceeded maximum value.\n\nLoops are represented by 8-bit numbers, therefore the highest value possible is 255.\n", line);
			break;
		case 19 :
			printf("Line %d: Invalid loop amount; must be non-zero.\n\nThe compiler will only accept tempos from 1 to 255.\n", line);
			break;
		}
		printf(ANSI_COLOR_RESET);

	if (remove("musicdata.h") == 0)
		printf("\nDeleted incomplete 'musicdata.h' file.");

	printf("\nTerminating due to error. μMML Compiler end. :(\n\n");

	exit(0);
}

void write_file(){
	/* 
	 *  COMMAND VALUES
	 *  0  :  Awaiting command (a value was the last input).
	 *  1  :  Note / rest was last input.
	 *  2  :  Octave was last input.
	 *  3  :  Volume was last input.
	 *  4  :  Loop was last input.
	 *  5  :  Comment, ignore input.
	 *  6  :  Apparently nothing? I just jumped to 7?
	 *  7  :  Tempo was last input.
	 *  8  :  Macro was last input.
	 */

	FILE *newfile = fopen("musicdata.h", "w");

	fprintf(newfile, "%s", "const unsigned char data[] PROGMEM = {\n\t");

	/*
	 *  Writing the file.
	 *
	 *  The language is essentially a more legible version of the
	 *  nibble structure read by the mmml.c program, so no complex
	 *  interpretation has to be done. The program looks for the
	 *  first nibble, then the second to finish the byte.
	 *
	 *  There are a few commands that behave differently, for
	 *  example, the channel command, the comments, macros, tempo
	 *  loops and the null byte, which signals the end of the program.
	 */

	for(int i=0; i<INPUT_FILE_SIZE; i++){

		if (line_counter >= 8){
			fprintf(newfile, "%s", "\n\t");
			line_counter = 0;
		}

		switch(input_data[i]){
			/* Report data position */
			case '?' :
				printf("Position: %i, Channel: %i\n",total_bytes-1, channel);
				break;

			/* The return character and counting new lines. Commands are
			 * reset every line (mainly for cancelling the commenting flag). */

			case '\n' :
				line++;
				command = 0;
				break;

			/* End of program (NUL). Jumps to END. I know... */

			case '\0' :
				if(channel < (CHANNELS - 1))
					error_message(14, line);

				if(highest_macro > (channel - CHANNELS))
					error_message(13, macro_line);

				fprintf(newfile, "%s", "0b11111111\n};\n\nconst unsigned int data_index[");
				total_bytes++;
				fprintf(newfile, "%i", channel);
				fprintf(newfile, "%s", "] PROGMEM = {\n\t");

				for(unsigned char i=0; i<channel; i++)
					fprintf(newfile,"%d,", data_index[i]);

				fprintf(newfile, "%s", "\n};");
				goto END;	// it's 3am and I've just had enough.
				break;

			/*  Comment flag. This sets the command value to 5 which, in
			 *  each switch case condition, is checked for to see if the
			 *  character at 'i' should be ignored or not. */

			case '%' :
				command = 5;
				break;

			/* Relative octave command */

			case '>' :
				if(command != 5){
					if(command == 0){
						/* More gotos, I KNOW! I just wanted to add this feature
						 * real quick. It checks to see whether you've put two
						 * octave changes in succession, then ignores the redundant
						 * octave. */
						CHECKOCTAVEUP:
						if(input_data[i+1] == '>'){
							octave++;
							i++;
							goto CHECKOCTAVEUP;
						}
						else{
							fprintf(newfile, "%s", "0b1101");
							if(octave >= 5)
								error_message(6,line);
							else if(octave == 4)
								fprintf(newfile, "%s", "1111,");
							else if(octave == 3)
								fprintf(newfile, "%s", "0111,");
							else if(octave == 2)
								fprintf(newfile, "%s", "0011,");
							else if(octave == 1)
								fprintf(newfile, "%s", "0001,");
							octave++;
							next_byte();
						}
					}
					else
						error_message(3,line);
				}
				break;

			case '<' :
				if(command != 5){
					if(command == 0){
						CHECKOCTAVEDOWN:
						if(input_data[i+1] == '<'){
							octave--;
							i++;
							goto CHECKOCTAVEDOWN;
						}
						else{
							fprintf(newfile, "%s", "0b1101");
							if(octave == 5)
								fprintf(newfile, "%s", "0111,");
							else if(octave == 4)
								fprintf(newfile, "%s", "0011,");
							else if(octave == 3)
								fprintf(newfile, "%s", "0001,");
							else if(octave == 2)
								fprintf(newfile, "%s", "0000,");
							else if(octave <= 1)
								error_message(11,line);
							octave--;
							next_byte();
						}
					}
					else
						error_message(3,line);
				}
				break;

			/* Track flag command */

			case '!' :
				if(command != 5){
					if(command == 0){
						fprintf(newfile, "%s", "0b11111110,");	
						next_byte();
					}
					else
						error_message(3,line);
				}
				break;

			/* Channel command */

			case '@' :
				if(command != 5){
					if(channel > 0)
						fprintf(newfile, "%s", "0b11111111,");
					line_counter++;
					data_index[channel] = total_bytes;
					channel++;

					total_bytes++;
				}
				break;

			/* 
			 *  Note commands. Sharps are found by looking one byte
			 *  ahead. If that byte is a sharp, skip forward one place
			 *  extra and carry on reading, if not, just place the
			 *  natural and continue as normal.
			 */

			case 'r' : // rest command (technically a note)
				if(command != 5){
					if(command == 0){
						fprintf(newfile, "%s", "0b0000");
						command = 1;
					}
					else
						error_message(3,line);
				}
				break;

			case 'c' :
				if(command != 5){
					if(command == 0){
						if(input_data[i+1] == '#' || input_data[i+1] == '+')
							fprintf(newfile, "%s", "0b0010");
						else
							fprintf(newfile, "%s", "0b0001");
						command = 1;
					}
					else
						error_message(3,line);
				}
				break;

			case 'd' :
				if(command != 5){
					if(command == 0){
						if(input_data[i+1] == '#' || input_data[i+1] == '+')
							fprintf(newfile, "%s", "0b0100");
						else
							fprintf(newfile, "%s", "0b0011");
						command = 1;
					}
					else
						error_message(3,line);
				}
				break;

			case 'e' :
				if(command != 5){
					if(command == 0){
						if(input_data[i+1] == '#' || input_data[i+1] == '+')
							fprintf(newfile, "%s", "0b0110");
						else
							fprintf(newfile, "%s", "0b0101");
						command = 1;
					}
					else
						error_message(3,line);
				}
				break;

			case 'f' :
				if(command != 5){
					if(command == 0){
						if(input_data[i+1] == '#' || input_data[i+1] == '+')
							fprintf(newfile, "%s", "0b0111");
						else
							fprintf(newfile, "%s", "0b0110");
						command = 1;
					}
					else
						error_message(3,line);
				}
				break;

			case 'g' :
				if(command != 5){
					if(command == 0){
						if(input_data[i+1] == '#' || input_data[i+1] == '+')
							fprintf(newfile, "%s", "0b1001");
						else
							fprintf(newfile, "%s", "0b1000");
						command = 1;
					}
					else
						error_message(3,line);
				}
				break;

			case 'a' :
				if(command != 5){
					if(command == 0){
						if(input_data[i+1] == '#' || input_data[i+1] == '+')
							fprintf(newfile, "%s", "0b1011");
						else
							fprintf(newfile, "%s", "0b1010");
						command = 1;
					}
					else
						error_message(3,line);
				}
				break;

			case 'b' :
				if(command != 5){
					if(command == 0){
						if(input_data[i+1] == '#' || input_data[i+1] == '+')
							fprintf(newfile, "%s", "0b0001");
						else
							fprintf(newfile, "%s", "0b1100");
						command = 1;
					}
					else
						error_message(3,line);
				}
				break;

			/* Octave command */

			case 'o' :
				if(command != 5){
					if(command == 0){
						fprintf(newfile, "%s", "0b1101");	
						command = 2;
					}
					else
						error_message(3,line);
				}
				break;

			/* Tempo command */

			case 't' :
				if(command != 5){
					if(command == 0){
						fprintf(newfile, "%s", "0b11110011,");
						next_byte(); check_line(newfile);

						for(unsigned char n = 0; n < 4; n++){
							if(isdigit(input_data[i+1])){
								i++;
								tempo_temp_string[n] = input_data[i];
							}
							else{
								if(n == 0)
									error_message(3,line);
								if(n > 3)
									error_message(16,line);
								if(n > 0 && n <= 3){
									tempo_temp_string[n+1] = '\0';

									tempo_value = atoi(tempo_temp_string);

									if(tempo_value > 255){
										error_message(16,line);
									}
									else if(tempo_value == 0){
										error_message(17,line);
									}

									fprintf(newfile, "%s", "0b");

									// run through the char and print the bits to make ASCII binary
									// print the bytes in reverse to get the endian
									for(signed char b=7; b >-1; b--)
										fprintf(newfile, "%d", (tempo_value >> b) & 1);

									fprintf(newfile, "%s", ",");

									// clear the temporary string
									for(unsigned char b = 0; b < 3; b++)
										tempo_temp_string[b] = 0;
								}
								break;
							}
						}
						next_byte(); check_line(newfile);
					}
					else
						error_message(3,line);
				}
				break;

			/* Volume command */

			case 'v' :
				if(command != 5){
					if(command == 0){
						if(input_data[i+1] == '-')
						{
							fprintf(newfile, "%s", "0b11110100,");
							i++;
							next_byte();
						}
						else if(input_data[i+1] == '+')
						{
							fprintf(newfile, "%s", "0b11110101,");
							i++;
							next_byte();
						}
						else
						{
							fprintf(newfile, "%s", "0b1110");
							command = 3;
						}
					}
					else
						error_message(3,line);
				}
				break;

			/* Loop command */

			case '[' :
				if(command != 5){
					if(command == 0){
						fprintf(newfile, "%s", "0b11110000,");
						next_byte();
						check_line(newfile);

						for(unsigned char n = 0; n < 4; n++){
							if(isdigit(input_data[i+1])){
								i++;
								loop_temp_string[n] = input_data[i];
							}
							else{
								if(n == 0)
									error_message(3,line);
								if(n > 3)
									error_message(16,line);
								if(n > 0 && n <= 3){
									loop_temp_string[n+1] = '\0'; // null terminate string

									loop_value = atoi(loop_temp_string);

									if(loop_value > 255){
										error_message(18,line);
									}
									else if(loop_value <= 1){
										error_message(19,line);
									}

									fprintf(newfile, "%s", "0b");

									// run through the char and print the bits to make ASCII binary
									// print the bytes in reverse to get the endian
									for(signed char b=7; b>-1; b--)
										fprintf(newfile, "%d", (loop_value >> b) & 1);

									fprintf(newfile, "%s", ",");

									// clear the temporary string
									for(unsigned char b=0; b<3; b++)
										loop_temp_string[b] = 0;
								}
								break;
							}
						}
						next_byte(); check_line(newfile);
					}
					else
						error_message(3,line);
				}

			case ']' :
				if(command != 5){
					if(command == 0){
						fprintf(newfile, "%s", "0b11110001,");
						next_byte();
					}
					else
						error_message(3,line);
				}
				break;

			/* Macro command */

			case 'm' :
				if(command != 5){
					if(command == 0){
						fprintf(newfile, "%s", "0b11110010,");
						next_byte(); check_line(newfile);

						for(unsigned char n = 0; n < 4; n++){
							if(isdigit(input_data[i+1])){
								i++;
								macro_temp_string[n] = input_data[i];
							}
							else{
								if(n == 0){
									error_message(3,line);
								}
								if(n > 3){
									error_message(12,line);
								}
								if(n > 0 && n <= 3){
									macro_temp_string[n+1] = '\0'; // null terminate string
									macro_value = atoi(macro_temp_string);
									
									if(macro_value > 255){
										error_message(12,line);
									}
									else if(macro_value == 0){
										error_message(15,line);
									}

									fprintf(newfile, "%s", "0b");

									if(macro_value > highest_macro){
										highest_macro = macro_value;
										macro_line = line;
									}

									/* note - macros start at 1 in the compiler and 0
									 * in the AVR code; that's why we minus one below */
									macro_value--;

									/* run through the char and print the bits to make ASCII binary
									 * print the bytes in reverse to get the endian */
									for(signed char b=7;b>-1;b--)
										fprintf(newfile, "%d", (macro_value >> b) & 1);

									fprintf(newfile, "%s", ",");

									// clear the temporary string
									for(unsigned char b = 0; b < 3; b++)
										macro_temp_string[b] = 0;
								}
								break;
							}
						}
						next_byte(); check_line(newfile);
					}
					else
						error_message(3,line);
				}
				break;

			/* 
			 *  Numerical values. Similar to the notes. As the two-character
			 *  decimal value '16' is a valid number the program has to scout
			 *  ahead in the array to work out if the number it's looking at
			 *  is a 1, or a value above 9.
			 */

			case '1' :
				if(command != 5){
					switch(command){
						case 0 :
							error_message(4,line);
							break;
						case 1:
							if(input_data[i+1] == '6'){
								if(input_data[i+2] == '.'){
									print_duration(161,newfile);
									i++;
								}
								else
									print_duration(16,newfile);
								i++;
							}
							else if(input_data[i+1] == '2' && input_data[i+2] == '8'){
								print_duration(128,newfile);
								i+=2;
							}
							else
								print_duration(1,newfile);
							break;
						case 2: // octave
							fprintf(newfile, "%s", "0000,");
							octave = 1;
							break;
						case 3: //volume
							fprintf(newfile, "%s", "1000,");
							break;
					}
					next_byte();
				}
				break;

			case '2' :
				if(command != 5){
					switch(command){
						case 0 :
							error_message(4,line);
							break;
						case 1:
							if(input_data[i+1] == '.'){
								print_duration(21,newfile);
								i++;
							}
							else
								print_duration(2,newfile);
							break;
						case 2: // octave
							fprintf(newfile, "%s", "0001,");
							octave = 2;
							break;
						case 3: //volume
							fprintf(newfile, "%s", "0111,");
							break;
						break;
					}
					next_byte();
				}
				break;

			case '3' :
				if(command != 5){
					switch(command){
						case 0 :
							error_message(4,line);
							break;
						case 1:
							if(input_data[i+1] == '2'){
								if(input_data[i+2] == '.'){
									print_duration(321,newfile);
									i++;
								}
								else
									print_duration(32,newfile);
								i++;
							}
							else
								error_message(5,line);
							break;
						case 2: // octave
							fprintf(newfile, "%s", "0011,");
							octave = 3;
							break;
						case 3: // volume
							fprintf(newfile, "%s", "0110,");
							break;
						break;
					}
					next_byte();
				}
				break;
			
			case '4' :
				if(command != 5){
					switch(command){
						case 0 :
							error_message(4,line);
							break;
						case 1:
							switch(input_data[i+1]){
								case '.' :
									print_duration(41,newfile);
									i++;
									break;
								default :
									print_duration(4,newfile);
									break;
								}
							break;
						case 2: // octave
							fprintf(newfile, "%s", "0111,");
							octave = 4;
							break;
						case 3: // volume
							fprintf(newfile, "%s", "0101,");
							break;
						break;
					}
					next_byte();
				}
				break;

			case '5' :
				if(command != 5){
					switch(command){
						case 0 :
							error_message(4,line);
							break;
						case 1:
							error_message(5,line);
							break;
						case 2: // octave
							fprintf(newfile, "%s", "1111,");
							octave = 5;
							break;
						case 3: // volume
							fprintf(newfile, "%s", "0100,");
							break;
						break;
					}
					next_byte();
				}
				break;

			case '6' :
				if(command != 5){
					switch(command){
						case 0 :
							error_message(4,line);
							break;
						case 1:
							if(input_data[i+1] == '4'){
								if(input_data[i+2] == '.'){
									print_duration(641,newfile);
									i++;
								}
								else
									print_duration(64,newfile);
								i++;
							}
							else
								error_message(5,line);
							break;
						case 2:
							error_message(6,line);
							break;
						case 3: //volume
							fprintf(newfile, "%s", "0011,");
							break;
						break;
					}
					next_byte();
				}
				break;

			case '7' :
				if(command != 5){
					switch(command){
						case 0 :
							error_message(4,line);
							break;
						case 1:
							error_message(5,line);
							break;
						case 2:
							error_message(6,line);
							break;
						case 3: //volume
							fprintf(newfile, "%s", "0010,");
							break;
						break;
					}
					next_byte();
				}
				break;

			case '8' :
				if(command != 5){
					switch(command){
						case 0 :
							error_message(4,line);
							break;
						case 1:
							switch(input_data[i+1]){
								case '.' :
									print_duration(81,newfile);
									i++;
									break;
								default :
									print_duration(8,newfile);
									break;
								}
							break;
						case 2:
							error_message(6,line);
							break;
						case 3: //volume
							fprintf(newfile, "%s", "0001,");
							break;
						break;
					}
					next_byte();
				}
				break;

			case '9' :
				if(command != 5){
					switch(command){
						case 0:
							error_message(4,line);
							break;
						case 1:
							error_message(5,line);
							break;
						case 2:
							error_message(6,line);
							break;
						case 3: //volume
							error_message(7,line);
							break;
						break;
					}
					next_byte();
				}
				break;

			case '0' :
				if(command != 5){
					switch(command){
						case 0:
							error_message(4,line);
							break;
						case 1:
							error_message(5,line);
							break;
						case 2:
							error_message(11,line);
							break;
						case 3: //volume
							error_message(7,line);
						break;
					}
				}
				break;

				// ugh, that whole section was terrible
		}

		/*
		 * The condition below checks to see if, firstly,
		 * the last command was a note value, then checks
		 * whether the next symbol is a non-numerical
		 * command. If two commands have been placed
		 * contiguously (ignoring spaces) then the last
		 * specified duration is inserted.
		 *
		 * Basically, it allows the user to omit identical
		 * duration commands in typical MML fashion.
		 *
		 * The stupidly long condition below was because
		 * I got fed up trying to make it more elegant.
		 * I spent a good two hours trying to iterate
		 * through a command array - and I'm too far in
		 * to completely refactor the code. This works,
		 * it's inelegant, but I'm happy to leave it there.
		 */
		if(command == 1){
			if(   input_data[i+1] == 'r' || input_data[i+1] == 'a' || input_data[i+1] == 'b'
			   || input_data[i+1] == 'c' || input_data[i+1] == 'd' || input_data[i+1] == 'e'
			   || input_data[i+1] == 'f' || input_data[i+1] == 'g' || input_data[i+1] == 'o'
			   || input_data[i+1] == '@' || input_data[i+1] == 'm' || input_data[i+1] == 't'
			   || input_data[i+1] == 'v' || input_data[i+1] == '[' || input_data[i+1] == ']'
			   || input_data[i+1] == '%' || input_data[i+1] == '<' || input_data[i+1] == '>'
			   || input_data[i+1] == '\n' )
			{
				print_duration(previousDuration,newfile);
				next_byte();
			}
		}
	}

	END:

	printf(ANSI_COLOR_GREEN "Successfully compiled!" ANSI_COLOR_RESET);	

	printf("\nTotal sequence is %d bytes.", total_bytes);

	fclose(newfile);

	printf("\nOutput written to 'musicdata.h'.\n");

	/* Cleans up microcontroller compiled files. The AVR compiler
	 * doesn't always re-read the musicdata.h file if these are
	 * extant. It doesn't hurt to remove them and it makes rapid
	 * flash cycles faster and easier. */

	if (remove("mmml.elf") == 0)
		printf("\nDeleted mmml.elf");
	else
		printf("\nNo need to delete mmml.elf");

	if (remove("mmml.hex") == 0)
		printf("\nDeleted mmml.hex");
	else
		printf("\nNo need to delete mmml.hex");

	if (remove("mmml.map") == 0)
		printf("\nDeleted mmml.map");
	else
		printf("\nNo need to delete mmml.map");

	if (remove("mmml.o") == 0)
		printf("\nDeleted mmml.o\n");
	else
		printf("\nNo need to delete mmml.o\n");
}

// just keeps things a bit tidier
void next_byte(){
	command = 0;
	total_bytes++;
	line_counter++;
}

// so rubbish, so poorly implemented
void print_duration(int number,FILE *newfile){
	previousDuration = number;
	switch(number){
		case 128:  //128th note
			fprintf(newfile, "%s", "0111,");
			break;
		case 16:   //16th note
			fprintf(newfile, "%s", "0100,");
			break;
		case 161:  //dotted 16th note
			fprintf(newfile, "%s", "1011,");
			break;
		case 1:    //whole note
			fprintf(newfile, "%s", "0000,");
			break;
		case 2:    //half note
			fprintf(newfile, "%s", "0001,");
			break;
		case 21:   //dotted half note
			fprintf(newfile, "%s", "1000,");
			break;
		case 32:   //32nd note
			fprintf(newfile, "%s", "0101,");
			break;
		case 321:  //dotted 32nd note
			fprintf(newfile, "%s", "1100,");
			break;
		case 41:   //dotted quarter note
			fprintf(newfile, "%s", "1001,");
			break;
		case 4:    //quarter note
			fprintf(newfile, "%s", "0010,");
			break;
		case 641:  //dotted 64th note
			fprintf(newfile, "%s", "1101,");
			break;
		case 64:   //64th note
			fprintf(newfile, "%s", "0110,");
			break;
		case 81:   //dotted 8th note
			fprintf(newfile, "%s", "1010,");
			break;
		case 8:    //8th note
			fprintf(newfile, "%s", "0011,");
			break;
	}
}

void check_line(FILE *newfile){
	if (line_counter >= 8){
		fprintf(newfile, "%s", "\n\t");
		line_counter = 0;
	}
}
