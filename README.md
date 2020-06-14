# μMML - Micro Music Macro Language
*An MML implementation for AVR microcontrollers (and other platforms, but largely 1-bit focused still).*

This little project was built to facilitate simple composing of 1-bit music for AVR microcontrollers using a derivation of *Music Macro Language* (MML). You can then, with the most basic of components, make cool embedded albums that run off a single coin cell battery.

**Scruss has written an excellent, incredibly comprehensive, guide to programming and building your own 1-bit music boxes using μMML, which you can find [HERE](https://scruss.com/blog/2020/04/02/protodomes-wonderful-chiptunes-how-to-play-them-on-your-own-attiny85-chips/). If you're new to AVR programming, this is where you should really start.**

**You can also find a small tutorial [HERE](https://github.com/farvardin/garvuino/blob/master/doc/garvuino_manual.md) by garvalf (under the MMML heading).**

I hope you have fun writing tiny music! If you have any questions/suggestions/corrections contact me at: hello@protodome.com

## 10th May 2020

Removed the old AVR specific compiler and updated the AVR player to work with the new format. Now there are three possible build targets:

1. `-t avr` for AVR microcontroller. Creates an .h include file for the mmml-avr-player.c program.
2. `-t gb` for Game Boy. Creates a .c file for the mmml-gb-player.c program (not included just yet).
3. `-t data` for desktop/DOS. Creates an .mmmldata file for the desktop synthesiser. Also read by the DOS player (not included just yet).

As the compiler is now centralised, it will be updated in a single location, supporting all future build platforms.

## 9th May 2020

Fixed the compiler on Windows. Additionally added some really early compiler support for the Game Boy μMML player (which is coming later).

As there are multiple build targets, you will have to run the compiler like this:

`$ ./compiler -f FILENAME.mmml -t data`

This will set the build (t)arget to .mmmldata file. (If you want Game Boy, you'll need to do `-t gb`, but that will produce a useless C file right now without the player.)

## 19th April 2020

I've added a bare-bones wave synthesizer that interprets .mmmldata files built by a new desktop compiler. The synthesizer requires two flags, an input file '-f' and a duration in seconds '-s'. As μMML currently has no way to determine the end of a track (officially anyway, you may notice that `0xFE` is a 'track end' flag; it's a pretty flaky system I used for the 4000AD physicals), you currently have to tell the synthesizer how many seconds you'd like it to run for. So, building and running the new features might looks something like this:

Build both programs:
`$ gcc mmml-desktop-compiler.c -o compiler`

`$ gcc mmml-desktop-synthesizer.c -o synthesizer`

Run the compiler first...
`$ ./compiler -f FILENAME.mmml -t data`

...then build the output file.
`$ ./synthesizer -f output.mmmldata -s 60`

Which will create a 60 second long wave file.

Additionally, I'm moving all the compilation features into a single compiler, so that it can build desktop and avr sources. There's a DOS player in the works, so I'm trying not to have multiple forks of the compiler, especially if the core mmml engine is expanded. As a heads-up, to support this planned functionality, the compiler will now require flags to declare input files (-f). This means that the code in the 'avr' folder is destined to be replaced at some point and, as such, I will not be updating the compiler there any longer.

There's also been a fix for 64-bit Linux systems where error 14 fired off erroneously. This fix is on the desktop compiler only for now.

## How To Use

**Note: The compiler is very bare-bones, stubborn and inflexible at the moment. It is in desperate need of complete refactoring.** It does work however, and that's all it needed to do to write my 1-bit album. So, with that in mind, read on and, if you like this project and want to help out, I would be honestly thrilled.

This guide assumes you're using OSX or Linux. It definitely works on Windows so, if you know what you're doing, it should be simple.

There are two required components to this project: the `mmml.c` player for your chosen AVR microcontroller and the `mmml-compiler.c` program for your chosen OS. The `mmml.c` player is built for AVR microcontrollers clocked at 8MHz (such as the Attiny85, or Atmega168) but, with a little tweaking, you should easily be able to adapt for other platforms. The `mmml.c` player requires prerequisite use of the `mmml-compiler.c` program to create a `musicdata.h` data file (where your song data is stored).

The `mmml-compiler.c` program will convert .txt files to create the required bytecode tucked up in the `musicdata.h` include file. You will need to put the resultant `musicdata.h` file in the same directory as the `mmml.c` file so that both can be easily found when flashing to the chip.

### Building The Compiler

To run `mmml-compiler.c`, you must compile with your system. For example, on Linux / OSX:

`$ gcc mmml-compiler.c -o mmml-compiler`

This will create an output file named 'mmml-compiler' (or whatever is entered after the -o flag), then run the output program from the terminal like so:

`$ ./mmml-compiler`

On Linux you will need to install the 'GCC' package if it is not already:

`$ sudo apt-get install gcc`

On OSX, you will need to install 'Command Line Tools'. In Mac OS 10.9 (and later), this can be achieved by using the following command:

`$ xcode-select --install`

### Uploading To Microcontroller

To program your microcontroller is a little more involved, but not hard! You'll need the [AVRDUDE software](https://www.nongnu.org/avrdude/) and you might want to follow [this Hackaday guide](https://hackaday.com/2010/10/23/avr-programming-introduction/) (which explains things better than I could).

## What Does The μMML Player Do?

The mmml.c program is a four channel routine: three channels of melodic, 1-bit pulse waves and a simple, percussive PWM sampler. Each channel is labelled A-D respectively, and all are mixed via pulse interleaving (rapidly switching between channels in sequence). The composer has the choice (before compiling) to replace the sampler (channel D) with a percussive noise generator instead, which (currently) cannot be dynamically toggled in software.

There are eight pulse waves available: 50%, 25%, 12.5%, 6.25%, 3.125%, 1.5625%, 0.78125% and 0.390625%. The fact there are only eight selectable widths is kind of arbitrary (the routine can generate loads of them) however these eight been selected due to both simplicity of implementation (the waveform peak is defined by enumerating the frequency, divided by powers of two: `waveform = frequency >> n;`) and because they are the most timbrally unique. Remember, at thinner widths (6.25% to 0.39065%), there will be a change in volume rather than timbre.

## Let's Get Technical

The compiled bytecode (read from the `musicdata.h` file - generated by the `mmml-compiler.c` program) is structured as follows:

```
        ______BYTE_____
       |               |
BITS : [0000]     [0000]
FUNC : [COMMAND]  [VALUE]
```

Each byte is split into two 'nibbles': four bit values with a possible range of 0-16. Each nibble requires a second, defining the value of the command. This is not entirely abstracted from in the human readable language, but it is key to understanding why some variables are limited to a maximum of 16 states. The language is structured so that the most frequent commands required are represented by the smallest data type interpreted by `mmml.c`. In a few cases, this structure is appended with an additional byte, as below:

```
        ______BYTE_____          __BYTE__
       |               |        |        |
BITS : [0000]     [0000]        [00000000]
FUNC : [COMMAND]  [IDENTIFIER]  [VALUE]
```

This behaviour is required by the 'Function' command, where there is always a trailing byte (technically a two-byte value). When higher precision is required, the nibble datatype, capable of representing numbers from 0 - 15, is not sufficient, thus the Function command uses the structure of a general command as a generic 'flag', capable of specifying sixteen additional functions and indicating whether the next byte in data should be interpreted as a new command, or an extension of the previous.

There are four main chunks of data for each channel, plus an additional block of data for each macro (channel agnostic sequences, referable in a channel's 'main chunk' of data). These are stored contiguously in a single, one-dimensional `unsigned char` array called `data`. The structure of this array is stored in a companion, `unsigned int` array called `data_index` which, as the name suggests, holds an index of where each chunk of data begins. The first four chunks are always channels A, B, C and D respectively, then macros 1, 2, 3... etc. It is unhelpful to imagine a specific maximum length of events for individual channels, it is dependent on the global size of all channels and macros combined, where any data beyond 65535 cannot be indexed (due to limitations of the declared array datatype).

A table listing all possible commands in the core music data read by the `mmml.c` routine, alongside their evocation values can be found below. The 'value' field lists the first nibble in each byte; the program then expects a further trailing number between 0-15. In the case of the 'function' command (1111, or 0xF), this trailing value must be one of those listed in the lower table. `mmml.c` then requires an additional byte, allowing values from 0-255.

```
--------------------------------  -------------------------------------
| µMML | BYTECODE | COMMAND    |  | µMML      | BYTECODE | COMMAND    |
--------------------------------  -------------------------------------
| r    | 0000     | rest       |  | g         | 1000     | note - g   |
| c    | 0001     | note - c   |  | g+        | 1001     | note - g#  |
| c+   | 0010     | note - c#  |  | a         | 1010     | note - a   |
| d    | 0011     | note - d   |  | a+        | 1011     | note - a#  |
| d+   | 0100     | note - d#  |  | b         | 1100     | note - b   |
| e    | 0101     | note - e   |  | o,<,>     | 1101     | octave     |
| f    | 0110     | note - f   |  | v         | 1110     | volume     |
| f+   | 0111     | note - f#  |  | [,],m,t,@ | 1111     | function   | ---
--------------------------------  -------------------------------------    |
------------------------------------------------------------               |
| µMML | BYTECODE | READS NEXT BYTE? | COMMAND             | <-------------
------------------------------------------------------------
| [    | 0000     | yes              | loop start          |
| ]    | 0001     | yes              | loop end            |
| m    | 0010     | yes              | macro               |
| t    | 0011     | yes              | tempo               |
|      | ....     | n/a              | 0100 - 1110 unused  |
| @    | 1111     | no               | channel/macro end   |
------------------------------------------------------------
```

# Writing Music In μMML

Below is a guide to writing μMML files to be compiled by the `mmml-compiler.c` program. I apologise, this is currently not comprehensive and I will be expanding this as I go. In the meantime, here is a cheatsheet of all commands to get you started. You can check out the included `.mmml` files (in /demo-songs) to see how these commands are applied.

```
cX - gX   : Note values (and duration)
rX        : Rest / pause (and duration)
1,2,4,8,
16,32,64,
128       : Possible note / rest durations
X.        : Dotted note (X plus an additional 1/2 X)
+ or #    : Sharpen a note
o1-5      : Channel octave (1 lowest, 5 highest)
< or >    : Decrease, or increase, the current octave
v0-8      : Channel 'volume' (actually pulse width) 8=50%, 7=25%, 6=12.5%, 5=6.25%, 4=3.125%, 3=1.5625, 2=0.78125, 1=0.390625.
[2-255    : Start X number of loops
]         : End a loop
m1-255    : Call a macro (channel agnostic data)
t1-255    : Set the global tempo
@         : Channel / macro start. Channels are declared sequentially from A - D, macros are specified underneath.
cX - eX   : Possible drum sounds (c=pop, c#=beep, d=kick, d#=snare, e=hi-hat)
```

Some quick tips:

1. You can nest up to five loops - or four in a macro.
   e.g.: `[X[X[X[X[X ]]]]]`

2. Notes don't need multiple, identical durations.
   e.g.: "cX cX cX" could be written as `cX c c`

3. Careful when jumping to a macro, it won't remember your octave! Best practice is to declare an octave at the start of each macro, and when you return.
   e.g.: `@ oX` and `mX oX`

4. Save space whenever and wherever you can! Arpeggios can be condensed into loops and repeated passages should become macros.

5. Channels will loop back to the beginning when they have completed reading their data.

6. I always start with a quarter note pause as, when microcontrollers are first switched on, the first few milliseconds can vary slightly, changing the duration.

7. Tempo commands can be placed in any channel, but apply globally (unfortunately, no unique tempo per channel).

8. You can't nest macros.

## The Volume Commmand (`vX`)

Volume is a 'core' function - one of those that requires a single byte to be declared (including the accompanying value). Obviously in a 1-bit environment, volume refers to pulse width, thus timbre and volume are conflated to the `v` command. As mentioned previously, there are eight possible duty cycles which can be called by specifying a value between zero and eight, where zero is silence and eight is a 50% duty cycle. Judicious use of this command allows for a fantastic range of possible timbres and instrumental expression. For example, ADSR envelopes can be created; the sequence `o3 v5 c32 v4 c v3 c8 v2 c v1 c r4` creates a sound with a sharp attack, sustain and decay, and `o3 v1 c32 v2 c v3 c v4 c v5 c v4 c8 v3 c v2 c v1 c` for a slow attack and slow decay. Notice the order in which the volume commands have been placed to create the respective envelopes, or volume 'ramps'.

This whole thing would definitely benefit from an 'instrument' feature - the ability to define premade envelopes to apply to, and call within, individual channels. This is on the feature list. Therefore, the value of a feature such as this is dependent on how much material will make use of it. μMML was designed specifically with consideration to the program memory size of the Attiny45/85. With only four or eight kilobytes (respectively) to store both routine and music, if a piece is unlikely to use a potential feature then it should not be included; this data may be entirely wasted and might be better used for more musical material.

## The Loop Commmand (`[X ]`)
The composer may loop contiguous material, specifically those commands that are in a continuous, unbroken sequence, by inserting a loop start point, loop end point and number of times the inner material should repeat. Looped material should be enclosed in square brackets with the loop number immediately following the open bracket. For example, the arpeggio in: `[8 c8 e g > c < ]` will repeat eight times. The loop number may be a number between 2 and 255. The comparatively larger range to those commands previously listed is due to the command type; as described, the 'function' command allows for a trailing 8-bit value. A value of 1 would perform the material once, thus negate the need for a loop. A value of 0 would not play the material at all, for which, again, a loop would not be required.

Loops may be nested; for example `[2 [8 c4 ]]` will repeat `c4` sixteen times. There is an upper limit on how many repetitions are possible; this value is defined in the header of the `mmml.c` program.

## The Macro Command (`mX`)
Music is typically repetitive in nature; it relies on the human tendency to recognise patterns. Consequently, one may often wish to repeat sections of material multiple times within a piece. The loop command *does* allow for this, but only when the material to be repeated is contiguous, which is rare. Consequently, any material that returns in a piece verbatim, but separated by unique information, cannot be reused. The macro command solves this problem by inserting a symbol where the material should be played. This symbol represents a string of data outside of the four channel structure that the data pointer jumps to, plays, then returns to read the next command in the original channel's data. To select the desired macro, two bytes are required: the first, instructs the program to treat the second as a number. This number is a pointer to an index in the `data_index[]` table and the channel data pointer will move to the defined location in the `data[]` array.

The layout of an μMML project should be structured as follows:

```
@ - oscillator A data
@ - oscillator B data
@ - oscillator C data
@ - sampler data
@ - macro #1
@ - macro #2
@ - macro #3
... and so on.
```

Each `@` symbol declares the end of the previous chunk of data with a channel end command. The first four `@` symbols are the 'home' chunks of data and the last are individual, voice agnostic nuggets of musical material that can be sequenced within the 'home' voice material. The channel command indicates which channel the following material should be placed in. Unlike most MML implementations, material can't be split across the document. Once the end of a channel is specified, the next begins. The last channel declared will always be a macro channel. The order in which material is placed in the first three channels is somewhat irrelevant as the oscillator generation code is homogeneous. Channels are stored contiguously in an array and are demarcated by the channel end command. This is inserted automatically after each channel's material has ended. As this is left up to the compiler, it does not have to be declared in the composer's μMML code. The compiler counts the bytes and generates an index where the `mmml.c` program can find where each of the respective materials begin.

In the current version, macros cannot be nested. Only one index is stored when jumping out of the main routine. A new macro command encountered while venturing out of the original channel's data stream will overwrite the original saved position with a new one. This means that, when this new material is finished, the data pointer will jump back to the previous macro playing then, once it reaches the channel end command, it will interpret this as the end of the original channel and loop back to the start of that channel's material. This could be avoided by using a two dimensional array for storing pointers: the X axis would be channel and the Y would store the locations departed in successive macros. This might be useful but, at the moment, I have had no real use for it. It seems that music is generally repetitive, but only to a certain extent; too much variation arises to warrant the extra few hundred or so bytes to implement such a system.

One must be cognisant when using octaves within macros and remember that the octave jump shorthand is a literal declaration of the current octave. Improper declaration of octaves may result in passages where the octave jumps erratically. The compiler will treat this in relation to the last octave used - probably somewhere at the end of the third channel (as the fourth is just a sampler). To avoid this, the desired octave should be stated at the start of each macro's material. Yes, you can share material between the sampler and the pulse channels. It's utterly pointless but you can do it.

## The Tempo Command (`tX`)
The tempo command is defined using `t` and sets the note playback speed only. These speeds do not correlate to any specific BPM and are defined arbitrarily; both by the internal clock and however long it takes to execute the code. As of writing I have not calculated BPMs, and these will vary based on architecture and clock. When read by `mmml.c`, this value adjusts the `tick_speed` variable.