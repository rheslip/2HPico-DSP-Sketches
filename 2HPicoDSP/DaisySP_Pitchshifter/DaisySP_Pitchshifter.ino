
// Copyright 2025 Rich Heslip
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
Pitch shifter example for 2HPico DSP hardware 
R Heslip  Jan 2026

Reasonably light on CPU - can run 44khz sampling at 150mhz

Top Jack - Audio input 

Middle jack - Right Audio out (solder the DAC jumper on the back of the PCB)

Bottom Jack - Left Audio out

First Parameter Page - RED

Top pot - Delay size

Second pot - Transposition

Third pot - Random Modulation

Fourth pot - Wet/Dry Mix


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
#define SAMPLERATE 44100  // run at higher sample rate - works OK at 150mhz

Adafruit_NeoPixel LEDS(NUMPIXELS, LEDPIN, NEO_GRB + NEO_KHZ800);

I2S i2s(INPUT_PULLUP); // both input and output

#include "daisysp.h"

// including the source files is a pain but that way you compile in only the modules you need
// DaisySP statically allocates memory and some modules e.g. reverb use a lot of ram
#include "control/phasor.cpp"
#include "effects/pitchshifter.h"

float samplerate=SAMPLERATE;  // for DaisySP
float MINDELAY = 0.03/(1/(float)SAMPLERATE);  // valid range for delay is 30-100ms from DaisySP source code
float MAXDELAY = 0.1/(1/(float)SAMPLERATE);

daisysp::PitchShifter pitchshifter;

#define CV_VOLT 580.6  // a/d counts per volt - trim for V/octave

#define NUMUISTATES 1 // 
enum UIstates {PITCHSHIFTER};
uint8_t UIstate=PITCHSHIFTER;

bool button=0;

#define DEBOUNCE 10
uint32_t buttontimer,parameterupdate;

float mix=0.5;
float outputlevel=0.8;


void setup() { 
  Serial.begin(115200);

#ifdef DEBUG

  Serial.println("starting setup");  
#endif

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

  pitchshifter.Init(samplerate);

#ifdef DEBUG  
  Serial.println("finished setup");  
#endif
}


void loop() {

// select UI page - only one page in this app
  if (!digitalRead(BUTTON1)) {
    if (((millis()-buttontimer) > DEBOUNCE) && !button) {  // if button pressed advance to next parameter set
      button=1;  
      ++UIstate;
      if (UIstate >= NUMUISTATES) UIstate=PITCHSHIFTER;
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
        case PITCHSHIFTER:
          LEDS.setPixelColor(0, RED); 
          if (!potlock[0]) pitchshifter.SetDelSize(map(pot[0],0,AD_RANGE-1,(uint32_t)MINDELAY,(uint32_t)MAXDELAY)); // this needs more smoothing - can hear glitching 
          if (!potlock[1]) pitchshifter.SetTransposition(mapf(pot[1],0,AD_RANGE-1,-12,12)); 
          if (!potlock[2]) pitchshifter.SetFun(mapf(pot[2],0,AD_RANGE-1,0,1.0));
          if (!potlock[3]) if (!potlock[3]) mix=(mapf(pot[3],0,AD_RANGE-1,0,1.0));  
          break;
        default:
          break;
    }
  }
 // LEDS.show();  // update LED
}

// second core setup
// second core is dedicated to sample processing
void setup1() {
delay (1000); // wait for main core to start up peripherals
}

// process audio samples
void loop1(){
  float sigL,sigR,outL,outR;
  int32_t left,right;

// these calls will stall if no data is available
  left=i2s.read();    // input is mono but we still have to read both channels
  right=i2s.read();

#ifdef MONITOR_CPU1
  digitalWrite(CPU_USE,1); // hi = CPU busy
#endif

  sigL=left*DIV_16; // convert input to float for DaisySP
 // sigR=left*DIV_16; 

  outL=pitchshifter.Process(sigL); // DaisySP docs imply its a mono effect 

  outL=(sigL*(1-mix)+ outL*mix)*outputlevel; // full dry to full wet signal mix

//  outL=(sigL+ outL*mix)*outputlevel; // add to dry signal 

  left=(int32_t)(outL*MULT_16); // convert output back to int32
  right=(int32_t)(outL*MULT_16); // convert output back to int32

#ifdef MONITOR_CPU1  
  digitalWrite(CPU_USE,0); // low - CPU not busy
#endif
// these calls will stall if buffer is full
	i2s.write(left); // left passthru
	i2s.write(right); // right passthru

}
