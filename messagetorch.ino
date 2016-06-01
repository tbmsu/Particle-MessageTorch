#include "application.h"
#include "neopixel/neopixel.h"

SYSTEM_MODE(AUTOMATIC);

// IMPORTANT: Set pixel COUNT, PIN and TYPE
#define PIXEL_PIN D2
#define PIXEL_COUNT 240
#define PIXEL_TYPE WS2812B

// Configuration
// =============

// Number of LEDs around the tube. One too much looks better (italic text look)
// than one to few (backwards leaning text look)
// Higher number = diameter of the torch gets larger
const uint16_t ledsPerLevel = 14; // Original: 13, smaller tube 11, high density small 17

// Number of "windings" of the LED strip around (or within) the tube
// Higher number = torch gets taller
const uint16_t levels = 17; // original 18, smaller tube 21, high density small 7

// total number of LEDs
const uint16_t numLeds = ledsPerLevel*levels; 

// set the LEDs' inital brightness level (0..255)
const uint8_t brightnessLevel = 255;

// UTILITIES

uint16_t random(uint16_t aMinOrMax, uint16_t aMax = 0) {
  if (aMax==0) {
    aMax = aMinOrMax;
    aMinOrMax = 0;
  }
  uint32_t r = aMinOrMax;
  aMax = aMax - aMinOrMax + 1;
  r += rand() % aMax;
  return r;
}

inline void reduce(byte &aByte, byte aAmount, byte aMin = 0) {
  int r = aByte-aAmount;
  if (r<aMin)
    aByte = aMin;
  else
    aByte = (byte)r;
}

inline void increase(byte &aByte, byte aAmount, byte aMax = 255) {
  int r = aByte+aAmount;
  if (r>aMax)
    aByte = aMax;
  else
    aByte = (byte)r;
}

// global parameters
enum {
  mode_off = 0,
  mode_torch = 1, // torch
  mode_fire = 2, // fire
  mode_lamp = 3 // lamp
};

byte fadedOff = 0;

// torch parameters
byte base_glimmer = 0; // if set, the base level LEDs (where the fire comes from) glitter.
byte upside_down = 0; // if set, flame (or rather: drop) animation is upside down. Text remains as-is

byte flame_min = 100; // 0..255 100
byte flame_max = 180; // 0..255 220

byte random_spark_probability = 2; // 0..100
byte spark_min = 200; // 0..255
byte spark_max = 255; // 0..255

enum {
  torch_passive = 0, // just environment, glow from nearby radiation
  torch_nop = 1, // no processing
  torch_spark = 2, // slowly looses energy, moves up
  torch_spark_temp = 3, // a spark still getting energy from the level below
};

byte spark_tfr = 40; // 0..256 how much energy is transferred up for a spark per cycle
uint16_t spark_cap = 200; // 0..255: spark cells: how much energy is retained from previous cycle

uint16_t up_rad = 40; // up radiation
uint16_t side_rad = 45; // sidewards radiation // default: 35
uint16_t heat_cap = 0; // 0..255: passive cells: how much energy is retained from previous cycle

byte red_bg = 0;
byte green_bg = 0;
byte blue_bg = 0;
byte red_bias = 10;
byte green_bias = 0;
byte blue_bias = 5; // 0
int red_energy = 180;
int green_energy = 145;
int blue_energy = 0; // 0

// main operation mode
byte operationMode = mode_torch;

// create WS2812B LED driver
Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXEL_COUNT, PIXEL_PIN, PIXEL_TYPE);

// PARTICLE SETUP and MAIN LOOP

void setup() {
    // Cloud functions
    Particle.function("opmode", handleOpMode);
    Particle.function("message", handleOpMode);
    
    // Initialize LEDs
    strip.begin();
    strip.show(); // Initialize all pixels to 'off'
}

void loop() {
    //static unsigned int nextTime = millis() + 1000 * 10;
    
    //if (nextTime < millis()) {
    //    operationMode = mode_off;
    //}
    
    switch(operationMode) {
        case mode_off: {
            fadeOff(50);
            break;
        }
        
        case mode_torch: {
            fadedOff = 0;
            injectRandom();
            calcNextEnergy();
            calcNextColors(1);
            break;
        }
            
        case mode_lamp: {
            fadedOff = 0;
            colorGradient(strip.Color(255, 255, 50), strip.Color(200, 0, 100), 192, 50);
            //colorGradient(strip.Color(200, 200, 0), strip.Color(150, 0, 0), 192, 50);
            //colorGradient(strip.Color(200, 200, 0), strip.Color(0, 220, 0), 192, 50);
            break;
        }
    }
}


// GENERAL EFFECTS

void fadeOff(uint8_t wait) {
    if (fadedOff) {
        delay(wait);
        return;
    }
        
    for(uint8_t i=255; i > 0; i--) {
        for(uint8_t pixel = 0; pixel<strip.numPixels(); pixel++) {
            uint32_t currColor = strip.getPixelColor(pixel);
            uint8_t
                r = (uint8_t)(currColor >> 16),
                g = (uint8_t)(currColor >>  8),
                b = (uint8_t)currColor;
            strip.setPixelColor(pixel, r * i/255, g * i/255, b * i/255);
        }
        strip.show();
        delay(wait);
    }
    fadedOff = 1;
}

void colorGradient(uint32_t startColor, uint32_t endColor, uint8_t brightness, uint8_t wait) {
    // get RGB for start and end color
    uint8_t
        rStart = (uint8_t)(startColor >> 16),
        gStart = (uint8_t)(startColor >>  8),
        bStart = (uint8_t)startColor,
        rEnd = (uint8_t)(endColor >> 16),
        gEnd = (uint8_t)(endColor >>  8),
        bEnd = (uint8_t)endColor;
    
    int n = strip.numPixels();
    
    for(int pixel=0; pixel<strip.numPixels(); pixel++) {
        uint8_t
            rNew = rStart + (rEnd - rStart) * pixel / n,
            gNew = gStart + (gEnd - gStart) * pixel / n,
            bNew = bStart + (bEnd - bStart) * pixel / n;
        
        strip.setColorDimmed(pixel, rNew, gNew, bNew, brightness);
    }
    
    strip.show();
    delay(wait);
}

void colorAll(uint32_t c, uint8_t wait) {
  uint16_t i;

  for(i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
  }
  strip.show();
  delay(wait);
}

void fire() {
    uint8_t r = 255;
    uint8_t g = 0;
    uint8_t b = 40;
    uint16_t i, ii;
    
    for(i=0; i<strip.numPixels(); i++) {
        strip.setPixelColor(i, r, random(0, 150), 0);
    }
    strip.show();
    delay(random(50, 100));
}


// CLOUD FUNCTIONS

int handleOpMode(String opMode) {
    if(opMode=="torch") {
        operationMode = mode_torch;
    }
    else if(opMode=="lamp") {
        operationMode = mode_lamp;
    } else {
        operationMode = mode_off;
    }
    
    return 1;
}



// TORCH

byte currentEnergy[numLeds]; // current energy level
byte nextEnergy[numLeds]; // next energy level
byte energyMode[numLeds]; // mode how energy is calculated for this point

void resetEnergy() {
  for (int i=0; i<numLeds; i++) {
    currentEnergy[i] = 0;
    nextEnergy[i] = 0;
    energyMode[i] = torch_passive;
  }
}

void injectRandom() {
  // random flame energy at bottom row
  for (int i=0; i<ledsPerLevel; i++) {
    currentEnergy[i] = random(flame_min, flame_max);
    energyMode[i] = torch_nop;
  }
  // random sparks at second row
  for (int i=ledsPerLevel; i<2*ledsPerLevel; i++) {
    if (energyMode[i]!=torch_spark && random(100)<random_spark_probability) {
      currentEnergy[i] = random(spark_min, spark_max);
      energyMode[i] = torch_spark;
    }
  }
}

void calcNextEnergy() {
  int i = 0;
  for (int y=0; y<levels; y++) {
    for (int x=0; x<ledsPerLevel; x++) {
      byte e = currentEnergy[i];
      byte m = energyMode[i];
      switch (m) {
        case torch_spark: {
          // loose transfer up energy as long as the is any
          reduce(e, spark_tfr);
          // cell above is temp spark, sucking up energy from this cell until empty
          if (y<levels-1) {
            energyMode[i+ledsPerLevel] = torch_spark_temp;
          }
          break;
        }
        case torch_spark_temp: {
          // just getting some energy from below
          byte e2 = currentEnergy[i-ledsPerLevel];
          if (e2<spark_tfr) {
            // cell below is exhausted, becomes passive
            energyMode[i-ledsPerLevel] = torch_passive;
            // gobble up rest of energy
            increase(e, e2);
            // loose some overall energy
            e = ((int)e*spark_cap)>>8;
            // this cell becomes active spark
            energyMode[i] = torch_spark;
          }
          else {
            increase(e, spark_tfr);
          }
          break;
        }
        case torch_passive: {
          e = ((int)e*heat_cap)>>8;
          increase(e, ((((int)currentEnergy[i-1]+(int)currentEnergy[i+1])*side_rad)>>9) + (((int)currentEnergy[i-ledsPerLevel]*up_rad)>>8));
        }
        default:
          break;
      }
      nextEnergy[i++] = e;
    }
  }
}

const uint8_t energymap[32] = {0, 64, 96, 112, 128, 144, 152, 160, 168, 176, 184, 184, 192, 200, 200, 208, 208, 216, 216, 224, 224, 224, 232, 232, 232, 240, 240, 240, 240, 248, 248, 248};

void calcNextColors(uint8_t wait) {
    for (int i=0; i<numLeds; i++) {
        int ei = i; // index into energy calculation buffer
        if (upside_down) {
            ei = numLeds - i;
        } else {
            ei = i; 
        }
        uint16_t e = nextEnergy[ei];
        currentEnergy[ei] = e;
        if (e>250)
            strip.setColorDimmed(i, 170, 170, e, brightnessLevel); // blueish extra-bright spark
        else {
             // energy to brightness is non-linear
              byte eb = energymap[e>>3];
              byte r = red_bias;
              byte g = green_bias;
              byte b = blue_bias;
              increase(r, (eb*red_energy)>>8);
              increase(g, (eb*green_energy)>>8);
              increase(b, (eb*blue_energy)>>8);
            
            if(base_glimmer && (ei < ledsPerLevel)) {
                // bottom glimmer (tbmsu)
                strip.setColorDimmed(i, r, random(50, 150), b, brightnessLevel);
            }
            else if (e>0) {
                // TODO was here
                strip.setColorDimmed(i, r, g, b, brightnessLevel);
            } else {
              // background, no energy
              strip.setColorDimmed(i, red_bg, green_bg, blue_bg, brightnessLevel);
            }
        }
    }
    
    strip.show();
    delay(wait);
}



// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
   return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= 170;
   return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}
