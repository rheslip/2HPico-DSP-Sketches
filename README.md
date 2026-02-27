# 2HPico DSP Sketches

Arduino Pico sketches for the 2HPico DSP Eurorack module https://github.com/rheslip/2HPico-Eurorack-Module-Hardware

Jan 28 2026 - added Reverb and Delay sketches. 

Feb 1 2026 - added Chorus, Flanger and Pitchshifter example sketches

Feb 26/26 - added Granular and Ladderfilter sketches

You must have Arduino 2.xx installed with the Pico board support package https://github.com/earlephilhower/arduino-pico

The DSP version of 2HPico requires an RP2350 processor since most DSP apps are fairly compute intensive. The DaisySP library uses floating point calculations extensively and will run very poorly on an RP2040. Some sketches require overclocking - check the comments in the source code.

Dependencies:

2HPico library from https://github.com/rheslip/2HPico-Sketches/tree/main/lib - install it in your Arduino/Libraries directory

Adafruit Neopixel library

Some sketches use my fork of ElectroSmith's DaisySP library https://github.com/rheslip/DaisySP_Teensy