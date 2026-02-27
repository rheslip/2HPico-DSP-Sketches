
// Copyright 2026 Rich Heslip
//
// Author: Rich Heslip 
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
/*
Ladder Filter example for 2HPico DSP hardware 
R Heslip  Feb 2026

Quite light on CPU - can run 96khz sampling at 150mhz

Top Jack - Audio input 

Middle jack - Frequency CV input exponential 1v/octave - solder the CV jumper on the back of the 2HPico DSP PCB

Bottom Jack - Audio out

First Parameter Page - RED

Top pot - Filter Resonance

Second pot - Initial Frequency

Third pot - CV attenuverter

Fourth pot - Output level


*/

#include "2HPico.h"
#include <I2S.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

#include "pico/multicore.h"

#define DEBUG   // comment out to remove debug code
#define MONITOR_CPU1  // define to enable 2nd core monitoring

//#define SAMPLERATE 11025 
//#define SAMPLERATE 22050  // 
//#define SAMPLERATE 44100  // 
#define SAMPLERATE 96000  // run at high sample rate - works OK at 150mhz

Adafruit_NeoPixel LEDS(NUMPIXELS, LEDPIN, NEO_GRB + NEO_KHZ800);

I2S i2s(INPUT_PULLUP); // both input and output

#include "daisysp.h"

// including the source files is a pain but that way you compile in only the modules you need
// DaisySP statically allocates memory and some modules e.g. reverb use a lot of ram
#include "filters/Moogladder.cpp"

float samplerate=SAMPLERATE;  // for DaisySP

MoogLadder filt;

#define CV_VOLT 580.6  // a/d counts per volt - trim for V/octave

#define NUMUISTATES 1 // 
enum UIstates {SET1};
uint8_t UIstate=SET1;

bool button=0;

#define DEBOUNCE 10
uint32_t buttontimer,parameterupdate;

#define FILTER_MIN_FREQ 20
float outputlevel=0.8;
float filterfreq=FILTER_MIN_FREQ;
float filterresonance=0.1;
float filtersweep=1.0;

void setup() { 
  Serial.begin(115200);

// set up I/O pins
 
#ifdef MONITOR_CPU1 // for monitoring 2nd core CPU usage
  pinMode(CPU_USE,OUTPUT); // hi = CPU busy
#endif 

  pinMode(BUTTON1,INPUT_PULLUP); // button in
  pinMode(MUXCTL,OUTPUT);  // analog switch mux

  LEDS.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
  LEDS.setPixelColor(0, RED); 
  LEDS.show();

  analogReadResolution(AD_BITS); // set up for max resolution

// set up I2S for 32 bits in and out
// PCM1808 is 24 bit only but I could not get 24 bit I2S working. 32 bits is little if any extra overhead
  i2s.setDOUT(I2S_DATA);
  i2s.setDIN(I2S_DATAIN);
  i2s.setBCLK(BCLK); // Note: LRCLK = BCLK + 1
  i2s.setMCLK(MCLK);
  i2s.setMCLKmult(256);
  i2s.setBitsPerSample(32);
  i2s.setFrequency(22050);
  i2s.begin();

  filt.Init(samplerate);
  filt.SetRes(filterresonance); // filter resonance
}


void loop() {

// select UI page - only one page in this sketch
  if (!digitalRead(BUTTON1)) {
    if (((millis()-buttontimer) > DEBOUNCE) && !button) {  // if button pressed advance to next parameter set
      button=1;  
      ++UIstate;
      if (UIstate >= NUMUISTATES) UIstate=SET1;
      lockpots();
    }
  }
  else {
    buttontimer=millis();
    button=0;
  }

  if ((millis() -parameterupdate) > PARAMETERUPDATE) {  // don't update the parameters too often -sometimes it messes up the daisySP models
    parameterupdate=millis();
    samplepots();

// assign parameters from panel pots
    switch (UIstate) {
        case SET1:
          LEDS.setPixelColor(0, RED);  
          if (!potlock[0]) filt.SetRes(mapf(pot[0],0,AD_RANGE-1,0,0.98));  // don't take resonance too high or filter behaves badly
          if (!potlock[1]) filterfreq=(mapf(pot[1],0,AD_RANGE-1,10,3000)); // 
          if (!potlock[2]) filtersweep=(mapf(pot[2],0,AD_RANGE-1,-1.5,1.5)); // 
          if (!potlock[3]) outputlevel=(mapf(pot[3],0,AD_RANGE-1,0,1.0)); // 
          break;
        default:
          break;
    }
  }
  float cv=(AD_RANGE-sampleCV2()); // CV in is inverted. this number will always be at least 1

  filt.SetFreq(filterfreq+pow(2,(cv/CV_VOLT)*filtersweep)*FILTER_MIN_FREQ); // ~ 7 octave range
 // LEDS.show();  // update LED
}

// second core setup
// second core is dedicated to sample processing
void setup1() {
delay (1000); // wait for main core to start up peripherals
}

// process audio samples
void loop1(){
  float sigL;
  int32_t left,right;

// these calls will stall if no data is available
  left=i2s.read();    // input is mono but we still have to read both channels
  right=i2s.read();   // 32 bit left justified signal

#ifdef MONITOR_CPU1
  digitalWrite(CPU_USE,1); // hi = CPU busy
#endif

  sigL=left*DIV_16; // convert input to float for DaisySP

  sigL=filt.Process(sigL); // process the signal thru the filter

  sigL=sigL*outputlevel; // adjust output level

  left=(int32_t)(sigL*MULT_16); // convert output back to int32
  right=(int32_t)(sigL*MULT_16); // convert output back to int32

#ifdef MONITOR_CPU1  
  digitalWrite(CPU_USE,0); // low - CPU not busy
#endif
// these calls will stall if buffer is full
	i2s.write(left); // left passthru
	i2s.write(right); // right passthru
}
