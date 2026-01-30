# 2HPIco DSP Sketches

Arduino Pico sketches for the 2HPico DSP Eurorack module https://github.com/rheslip/2HPico-Eurorack-Module-Hardware

Jan 28 2026 - added Reverb and Delay sketches. 

You must have Arduino 2.xx installed with the Pico board support package https://github.com/earlephilhower/arduino-pico

Select board type as Raspberry Pi Pico or Raspberry Pico Pico 2 depending on what board you used when building the module. Some sketches require overclocking - check the comments in the source code.

Dependencies:

2HPico library from https://github.com/rheslip/2HPico-Sketches/tree/main/lib - install it in your Arduino/Libraries directory

Adafruit Neopixel library

Some sketches use my fork of ElectroSmith's DaisySP library https://github.com/rheslip/DaisySP_Teensy