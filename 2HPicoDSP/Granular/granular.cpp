
// granular code (mostly) written by ChapGPT - very cool!

#include <Arduino.h>

#define BUFFER_SIZE 2048
#define MAX_GRAINS 16

struct Grain {
  bool active;
  uint16_t pos;
  uint16_t length;
  uint16_t age;
  float speed;
};

class Granular {
public:
  Granular() : writePos(0), grainSize(200), density(20), pitch(1.0f) {}

  void setGrainSize(uint16_t s) { grainSize = s; }
  void setDensity(uint8_t d) { density = d; }
  void setPitch(float p) { pitch = p; }

  int16_t process(int16_t input) {
    buffer[writePos] = input;
    writePos = (writePos + 1) % BUFFER_SIZE;

    int32_t output = 0;
    int16_t graincnt=0;

    for (int i = 0; i < MAX_GRAINS; i++) {
      if (grains[i].active) {
        ++graincnt;
        uint16_t readPos = (grains[i].pos + (uint16_t)(grains[i].age * grains[i].speed)) % BUFFER_SIZE;
        float env = window(grains[i].age, grains[i].length);
        output += (int16_t)((float)buffer[readPos] * env);

        grains[i].age++;
        if (grains[i].age >= grains[i].length) {
          grains[i].active = false;
        }
      }     
    }
    
    if (random(0, 100) < density) spawnGrain();
    output=output/(graincnt/4);  // scale output by # grains - its a bit low so fudge factor of x4 used
    return (int16_t)(output); 
  }

private:
  int16_t buffer[BUFFER_SIZE];
  uint16_t writePos;
  Grain grains[MAX_GRAINS];

  uint16_t grainSize;
  uint8_t density;
  float pitch;

  void spawnGrain() {
    for (int i = 0; i < MAX_GRAINS; i++) {
      if (!grains[i].active) {
        grains[i].active = true;
        grains[i].age = 0;
        grains[i].length = grainSize;
        grains[i].speed = pitch;
        grains[i].pos = (writePos + random(0,-BUFFER_SIZE/2)) % BUFFER_SIZE;
        Serial.printf("length %d pitch %f pos %d\n",grainSize,pitch,grains[i].pos);
        return;
      }
    }
  }

  float window(uint16_t age, uint16_t len) {
    float phase = (float)age / (float)len;
    return 0.5f - 0.5f * cosf(2.0f * PI * phase); // Hann window
  }
};