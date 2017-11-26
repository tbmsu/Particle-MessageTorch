#include "application.h"
#include <neopixel.h>

#include <OneWire.h>
#include "DS18.h"

SYSTEM_MODE(AUTOMATIC);
DS18 sensor(D4);

#define ONE_DAY_MILLIS (24 * 60 * 60 * 1000)

// IMPORTANT: Set pixel COUNT, PIN and TYPE
#define PIXEL_PIN D2
#define PIXEL_COUNT 240
#define PIXEL_TYPE WS2812B

// Configuration
// =============

// Number of LEDs around the tube. One too much looks better (italic text look)
// than one to few (backwards leaning text look)
// Higher number = diameter of the torch gets larger
const uint16_t ledsPerLevel = 15; // Original: 13, smaller tube 11, high density small 17

// Number of "windings" of the LED strip around (or within) the tube
// Higher number = torch gets taller
const uint16_t levels = 16; // original 18, smaller tube 21, high density small 7

// total number of LEDs
const uint16_t leveledNumLeds = ledsPerLevel*levels;
const uint16_t numLeds = leveledNumLeds + (PIXEL_COUNT > leveledNumLeds ? (PIXEL_COUNT - leveledNumLeds) : 0);

// set the LEDs' inital brightness level (0..255)
const uint8_t brightnessLevel = 255;

const bool mirrorText = false;

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

int hexToInt(char aHex) {
  if (aHex<'0') return 0;
  aHex -= '0';
  if (aHex>9) aHex -= 7;
  if (aHex>15) return 0;
  return aHex;
}


// Simple 7 pixel height dot matrix font
// =====================================
// Note: the font is derived from a monospaced 7*5 pixel font, but has been adjusted a bit
//       to get rendered proportionally (variable character width, e.g. "!" has width 1, whereas "m" has 7)
//       In the fontGlyphs table below, every char has a number of pixel colums it consists of, and then the
//       actual column values encoded as a string.

typedef struct {
  uint8_t width;
  const char *cols;
} glyph_t;

const int numGlyphs = 102; // 96 ASCII 0x20..0x7F plus 6 ÄÖÜäöü
const int rowsPerGlyph = 7;
const int glyphSpacing = 2;

static const glyph_t fontGlyphs[numGlyphs] = {
  { 5, "\x00\x00\x00\x00\x00" },  //   0x20 (0)
  { 1, "\x5f" },                  // ! 0x21 (1)
  { 3, "\x03\x00\x03" },          // " 0x22 (2)
  { 5, "\x28\x7c\x28\x7c\x28" },  // # 0x23 (3)
  { 5, "\x24\x2a\x7f\x2a\x12" },  // $ 0x24 (4)
  { 5, "\x4c\x2c\x10\x68\x64" },  // % 0x25 (5)
  { 5, "\x30\x4e\x55\x22\x40" },  // & 0x26 (6)
  { 1, "\x01" },                  // ' 0x27 (7)
  { 3, "\x1c\x22\x41" },          // ( 0x28 (8)
  { 3, "\x41\x22\x1c" },          // ) 0x29 (9)
  { 5, "\x01\x03\x01\x03\x01" },  // * 0x2A (10)
  { 5, "\x08\x08\x3e\x08\x08" },  // + 0x2B (11)
  { 2, "\x50\x30" },              // , 0x2C (12)
  { 5, "\x08\x08\x08\x08\x08" },  // - 0x2D (13)
  { 2, "\x60\x60" },              // . 0x2E (14)
  { 5, "\x40\x20\x10\x08\x04" },  // / 0x2F (15)

  { 5, "\x3e\x51\x49\x45\x3e" },  // 0 0x30 (0)
  { 3, "\x42\x7f\x40" },          // 1 0x31 (1)
  { 5, "\x62\x51\x49\x49\x46" },  // 2 0x32 (2)
  { 5, "\x22\x41\x49\x49\x36" },  // 3 0x33 (3)
  { 5, "\x0c\x0a\x09\x7f\x08" },  // 4 0x34 (4)
  { 5, "\x4f\x49\x49\x49\x31" },  // 5 0x35 (5)
  { 5, "\x3e\x49\x49\x49\x32" },  // 6 0x36 (6)
  { 5, "\x03\x01\x71\x09\x07" },  // 7 0x37 (7)
  { 5, "\x36\x49\x49\x49\x36" },  // 8 0x38 (8)
  { 5, "\x26\x49\x49\x49\x3e" },  // 9 0x39 (9)
  { 2, "\x66\x66" },              // : 0x3A (10)
  { 2, "\x56\x36" },              // ; 0x3B (11)
  { 4, "\x08\x14\x22\x41" },      // < 0x3C (12)
  { 4, "\x24\x24\x24\x24" },      // = 0x3D (13)
  { 4, "\x41\x22\x14\x08" },      // > 0x3E (14)
  { 5, "\x02\x01\x59\x09\x06" },  // ? 0x3F (15)

  { 5, "\x3e\x41\x5d\x55\x5e" },  // @ 0x40 (0)
  { 5, "\x7c\x0a\x09\x0a\x7c" },  // A 0x41 (1)
  { 5, "\x7f\x49\x49\x49\x36" },  // B 0x42 (2)
  { 5, "\x3e\x41\x41\x41\x22" },  // C 0x43 (3)
  { 5, "\x7f\x41\x41\x22\x1c" },  // D 0x44 (4)
  { 5, "\x7f\x49\x49\x41\x41" },  // E 0x45 (5)
  { 5, "\x7f\x09\x09\x01\x01" },  // F 0x46 (6)
  { 5, "\x3e\x41\x49\x49\x7a" },  // G 0x47 (7)
  { 5, "\x7f\x08\x08\x08\x7f" },  // H 0x48 (8)
  { 3, "\x41\x7f\x41" },          // I 0x49 (9)
  { 5, "\x30\x40\x40\x40\x3f" },  // J 0x4A (10)
  { 5, "\x7f\x08\x0c\x12\x61" },  // K 0x4B (11)
  { 5, "\x7f\x40\x40\x40\x40" },  // L 0x4C (12)
  { 7, "\x7f\x02\x04\x0c\x04\x02\x7f" },  // M 0x4D (13)
  { 5, "\x7f\x02\x04\x08\x7f" },  // N 0x4E (14)
  { 5, "\x3e\x41\x41\x41\x3e" },  // O 0x4F (15)

  { 5, "\x7f\x09\x09\x09\x06" },  // P 0x50 (0)
  { 5, "\x3e\x41\x51\x61\x7e" },  // Q 0x51 (1)
  { 5, "\x7f\x09\x09\x09\x76" },  // R 0x52 (2)
  { 5, "\x26\x49\x49\x49\x32" },  // S 0x53 (3)
  { 5, "\x01\x01\x7f\x01\x01" },  // T 0x54 (4)
  { 5, "\x3f\x40\x40\x40\x3f" },  // U 0x55 (5)
  { 5, "\x1f\x20\x40\x20\x1f" },  // V 0x56 (6)
  { 5, "\x7f\x40\x38\x40\x7f" },  // W 0x57 (7)
  { 5, "\x63\x14\x08\x14\x63" },  // X 0x58 (8)
  { 5, "\x03\x04\x78\x04\x03" },  // Y 0x59 (9)
  { 5, "\x61\x51\x49\x45\x43" },  // Z 0x5A (10)
  { 3, "\x7f\x41\x41" },          // [ 0x5B (11)
  { 5, "\x04\x08\x10\x20\x40" },  // \ 0x5C (12)
  { 3, "\x41\x41\x7f" },          // ] 0x5D (13)
  { 4, "\x04\x02\x01\x02" },      // ^ 0x5E (14)
  { 5, "\x40\x40\x40\x40\x40" },  // _ 0x5F (15)

  { 2, "\x01\x02" },              // ` 0x60 (0)
  { 5, "\x20\x54\x54\x54\x78" },  // a 0x61 (1)
  { 5, "\x7f\x44\x44\x44\x38" },  // b 0x62 (2)
  { 5, "\x38\x44\x44\x44\x08" },  // c 0x63 (3)
  { 5, "\x38\x44\x44\x44\x7f" },  // d 0x64 (4)
  { 5, "\x38\x54\x54\x54\x18" },  // e 0x65 (5)
  { 5, "\x08\x7e\x09\x09\x02" },  // f 0x66 (6)
  { 5, "\x48\x54\x54\x54\x38" },  // g 0x67 (7)
  { 5, "\x7f\x08\x08\x08\x70" },  // h 0x68 (8)
  { 3, "\x48\x7a\x40" },          // i 0x69 (9)
  { 5, "\x20\x40\x40\x48\x3a" },  // j 0x6A (10)
  { 4, "\x7f\x10\x28\x44" },      // k 0x6B (11)
  { 3, "\x3f\x40\x40" },          // l 0x6C (12)
  { 7, "\x7c\x04\x04\x38\x04\x04\x78" },  // m 0x6D (13)
  { 5, "\x7c\x04\x04\x04\x78" },  // n 0x6E (14)
  { 5, "\x38\x44\x44\x44\x38" },  // o 0x6F (15)

  { 5, "\x7c\x14\x14\x14\x08" },  // p 0x70 (0)
  { 5, "\x08\x14\x14\x7c\x40" },  // q 0x71 (1)
  { 5, "\x7c\x04\x04\x04\x08" },  // r 0x72 (2)
  { 5, "\x48\x54\x54\x54\x24" },  // s 0x73 (3)
  { 5, "\x04\x04\x7f\x44\x44" },  // t 0x74 (4)
  { 5, "\x3c\x40\x40\x40\x7c" },  // u 0x75 (5)
  { 5, "\x1c\x20\x40\x20\x1c" },  // v 0x76 (6)
  { 7, "\x7c\x40\x40\x38\x40\x40\x7c" },  // w 0x77 (7)
  { 5, "\x44\x28\x10\x28\x44" },  // x 0x78 (8)
  { 5, "\x0c\x50\x50\x50\x3c" },  // y 0x79 (9)
  { 5, "\x44\x64\x54\x4c\x44" },  // z 0x7A (10)
  { 3, "\x08\x36\x41" },          // { 0x7B (11)
  { 1, "\x7f" },                  // | 0x7C (12)
  { 3, "\x41\x36\x08" },          // } 0x7D (13)
  { 4, "\x04\x02\x04\x08" },      // ~ 0x7E (14)
  { 5, "\x7F\x41\x41\x41\x7F" },  //   0x7F (15)

  { 5, "\x7D\x0a\x09\x0a\x7D" },  // Ä 0x41 (1)
  { 5, "\x3F\x41\x41\x41\x3F" },  // Ö 0x4F (15)
  { 5, "\x3D\x40\x40\x40\x3D" },  // Ü 0x55 (5)
  { 5, "\x20\x55\x54\x55\x78" },  // ä 0x61 (1)
  { 5, "\x38\x45\x44\x45\x38" },  // ö 0x6F (15)
  { 5, "\x3c\x41\x40\x41\x7c" },  // ü 0x75 (5)
};

// global parameters

enum {
  mode_off = 0,
  mode_torch = 1, // torch
  mode_lamp = 2 // lamp
};

byte operationMode = mode_torch; // main operation mode
byte fade_base = 140; // crossfading base brightness level
byte fadedOff = 0;
uint16_t cycle_wait = 2; // 0..255; default: 1

// text params
int text_intensity = 255; // intensity of last column of text (where text appears)
int cycles_per_px = 5; // default: 5
int text_repeats = 5; // text displays until faded down to almost zero; default: 5
int fade_per_repeat = 50; // how much to fade down per repeat; default: 10
int text_base_line = 3; // default: 8
byte red_text = 0;
byte green_text = 255;
byte blue_text = 180;

// clock parameters
int clock_interval = 0; // 15*60; // by default, show clock every 15 mins (0=never)
int clock_zone = 2; // UTC+2 = CEST = Central European Summer Time
char clock_fmt[30] = "%k:%M"; // use format specifiers from strftime, see e.g. http://linux.die.net/man/3/strftime. %k:%M is 24h hour/minute clock

// temperature ring parameters
int ring_level = 13; // default: 13
int ring_off_display_interval = 60; // default: 40
int ring_off_display_duration = 5; // default: 5

// torch parameters
byte base_glimmer = 1; // if set, the base level LEDs (where the fire comes from) glitter.
byte upside_down = 0; // if set, flame (or rather: drop) animation is upside down. Text remains as-is

byte flame_min = 100; // 0..255; default: 100
byte flame_max = 220; // 0..255; default: 220

byte random_spark_probability = 3; // 0..100; default: 2
byte spark_min = 180; // 0..255
byte spark_max = 255; // 0..255

enum {
  torch_passive = 0, // just environment, glow from nearby radiation
  torch_nop = 1, // no processing
  torch_spark = 2, // slowly looses energy, moves up
  torch_spark_temp = 3, // a spark still getting energy from the level below
};

byte spark_tfr = 30; // 0..255 how much energy is transferred up for a spark per cycle; default: 40
uint16_t spark_cap = 170; // 0..255: spark cells: how much energy is retained from previous cycle; default: 200

uint16_t up_rad = 40; // up radiation
uint16_t side_rad = 45; // sidewards radiation // default: 35
uint16_t heat_cap = 50; // 0..255: passive cells: how much energy is retained from previous cycle // 0

byte red_bg = 0;
byte green_bg = 0;
byte blue_bg = 0;
byte red_bias = 35; // default: 10
byte green_bias = 0;
byte blue_bias = 0; // 0
int red_energy = 180;
int green_energy = 120; // default: 145
int blue_energy = 0; // 0

// create WS2812B LED driver
Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXEL_COUNT, PIXEL_PIN, PIXEL_TYPE);


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

void colorFade(uint8_t r, uint8_t g, uint8_t b) {
    for(uint16_t i = 0; i < strip.numPixels(); i++) {
        uint8_t curr_r, curr_g, curr_b;
        uint32_t curr_col = strip.getPixelColor(i); // get the current colour
        curr_b = curr_col & 0xFF; curr_g = (curr_col >> 8) & 0xFF; curr_r = (curr_col >> 16) & 0xFF;  // separate into RGB components

        if ((curr_r != r) || (curr_g != g) || (curr_b != b)) {  // while the curr color is not yet the target color
            if (curr_r < r) curr_r++; else if (curr_r > r) curr_r--;  // increment or decrement the old color values
            if (curr_g < g) curr_g++; else if (curr_g > g) curr_g--;
            if (curr_b < b) curr_b++; else if (curr_b > b) curr_b--;
            strip.setPixelColor(i, curr_r, curr_g, curr_b);  // set the color
        }
    }
}

void colorGradient(uint32_t startColor, uint32_t endColor, uint8_t brightness) {
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
        //strip.setPixelColor(pixel, rNew, gNew, bNew);
    }
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

int handleMessage(String message) {
   return newMessage(message);
}

// TORCH

// text layer
// ==========

// text layer, but only one strip around the tube (ledsPerLevel) with the height of the characters (rowsPerGlyph)
const int textPixels = ledsPerLevel*rowsPerGlyph;
byte textLayer[textPixels];
String text;

int textPixelOffset;
int textCycleCount;
int repeatCount;

int newMessage(String aText) {
  // URL decode
  text = "";
  int i = 0;
  char c;
  while (i<(int)aText.length()) {
    if (aText[i]=='%') {
      if ((int)aText.length()<=i+2) break; // end of text
      // get hex
      c = (hexToInt(aText[i+1])<<4) + hexToInt(aText[i+2]);
      i += 2;
    }
    // Ä = C3 84
    // Ö = C3 96
    // Ü = C3 9C
    // ä = C3 A4
    // ö = C3 B6
    // ü = C3 BC
    else if (aText[i]==0xC3) {
      if ((int)aText.length()<=i+1) break; // end of text
      switch (aText[i+1]) {
        case 0x84: c = 0x80; break; // Ä
        case 0x96: c = 0x81; break; // Ö
        case 0x9C: c = 0x82; break; // Ü
        case 0xA4: c = 0x83; break; // ä
        case 0xB6: c = 0x84; break; // ö
        case 0xBC: c = 0x85; break; // ü
        default: c = 0x7F; break; // unknown
      }
      i += 1;
    }
    else {
      c = aText[i];
    }
    // put to output string
    text += String(c);
    i++;
  }
  // initiate display of new text
  textPixelOffset = -ledsPerLevel;
  textCycleCount = 0;
  repeatCount = 0;
  return 1;
}

void resetText() {
  for(int i=0; i<textPixels; i++) {
    textLayer[i] = 0;
  }
}

void crossFade(byte aFader, byte aValue, byte &aOutputA, byte &aOutputB) {
  byte baseBrightness = (aValue*fade_base)>>8;
  byte varBrightness = aValue-baseBrightness;
  byte fade = (varBrightness*aFader)>>8;
  aOutputB = baseBrightness+fade;
  aOutputA = baseBrightness+(varBrightness-fade);
}

int glyphIndexForChar(const char aChar) {
  int i = aChar-0x20;
  if (i<0 || i>=numGlyphs) i = 95; // ASCII 0x7F-0x20
  return i;
}

void renderText() {
  // fade between rows
  byte maxBright = text_intensity-repeatCount*fade_per_repeat;
  byte thisBright, nextBright;
  crossFade(255*textCycleCount/cycles_per_px, maxBright, thisBright, nextBright);
  // generate vertical rows
  int activeCols = ledsPerLevel-2;
  // calculate text length in pixels
  int totalTextPixels = 0;
  int textLen = (int)text.length();
  for (int i=0; i<textLen; i++) {
    // sum up width of individual chars
    totalTextPixels += fontGlyphs[glyphIndexForChar(text[i])].width + glyphSpacing;
  }
  for (int x=0; x<ledsPerLevel; x++) {
    uint8_t column = 0;
    // determine font column
    if (x<activeCols) {
      int colPixelOffset = textPixelOffset + x;
      if (colPixelOffset>=0) {
        // visible column
        // - calculate character index
        int charIndex = 0;
        int glyphOffset = colPixelOffset;
        const glyph_t *glyphP = NULL;
        while (charIndex<textLen) {
          glyphP = &fontGlyphs[glyphIndexForChar(text[charIndex])];
          int cw = glyphP->width + glyphSpacing;
          if (glyphOffset<cw) break; // found char
          glyphOffset -= cw;
          charIndex++;
        }
        // now we have
        // - glyphP = the glyph,
        // - glyphOffset=column offset within that glyph (but might address a spacing column not stored in font table)
        if (charIndex<textLen) {
          // is a column of a visible char
          if (glyphOffset<glyphP->width) {
            // fetch glyph column
            column = glyphP->cols[glyphOffset];
          }
        }
      }
    }
    // now render columns
    for (int glyphRow=0; glyphRow<rowsPerGlyph; glyphRow++) {
      int i;
      int leftstep;
      if (mirrorText) {
        i = (glyphRow+1)*ledsPerLevel - 1 - x; // LED index, x-direction mirrored
        leftstep = 1;
      }
      else {
        i = glyphRow*ledsPerLevel + x; // LED index
        leftstep = -1;
      }
      if (glyphRow < rowsPerGlyph) {
        if (column & (0x40>>glyphRow)) {
          textLayer[i] = thisBright;
          // also adjust pixel left to this one
          if (x>0) {
            increase(textLayer[i+leftstep], nextBright, maxBright);
          }
          continue;
        }
      }
      textLayer[i] = 0; // no text
    }
  }
  // increment
  textCycleCount++;
  if (textCycleCount>=cycles_per_px) {
    textCycleCount = 0;
    textPixelOffset++;
    if (textPixelOffset>totalTextPixels) {
      // text shown, check for repeats
      repeatCount++;
      if (text_repeats!=0 && repeatCount>=text_repeats) {
        // done
        text = ""; // remove text
      }
      else {
        // show again
        textPixelOffset = -ledsPerLevel;
        textCycleCount = 0;
      }
    }
  }
}

void setTextPixels() {
    uint16_t pixel;
    
    for(pixel=0; pixel<strip.numPixels(); pixel++) {
        int textStart = text_base_line*ledsPerLevel;
        int textEnd = textStart+rowsPerGlyph*ledsPerLevel;
        if (pixel>=textStart && pixel<textEnd && textLayer[pixel-textStart]>0) {
            // overlay with text color
            strip.setColorDimmed(pixel, red_text, green_text, blue_text, (brightnessLevel*textLayer[pixel-textStart])>>8);
        }
    }
}


// tbmsu's modes
// =============
void colorAll(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
    uint16_t pixel;

    for(pixel=0; pixel<strip.numPixels(); pixel++) {
        strip.setColorDimmed(pixel, r, g, b, brightness);
    }
}

void lampTemeratureRing(uint16_t ringLevel, uint8_t brightness, float tempC) {
    uint16_t pixel;
    
    // color depending on temperature
    uint8_t 
        rTemp, 
        gTemp, 
        bTemp;
    
    if (tempC <= 0) {   // off/black
        rTemp = 0;
        gTemp = 0;
        bTemp = 0;
    }
    else if (tempC < 14.0) {
        rTemp = 0;
        gTemp = 0;
        bTemp = 255;
    } else if (tempC < 18.0) {
        rTemp = 0;
        gTemp = 170;
        bTemp = 255;
    } else if (tempC < 20.0) {
        rTemp = 0;
        gTemp = 255;
        bTemp = 170;
    } else if (tempC < 22.4) {
        rTemp = 0;
        gTemp = 255;
        bTemp = 0;
    } else if (tempC < 23.3) {
        rTemp = 170;
        gTemp = 255;
        bTemp = 0;
    } else if (tempC < 25.2) {
        rTemp = 255;
        gTemp = 170;
        bTemp = 0;
    } else {
        rTemp = 255;
        gTemp = 0;
        bTemp = 0;
    }
    
    // color ring
    if (ringLevel > levels) {
        ringLevel = levels - 1;
    }
    for(pixel = ringLevel * ledsPerLevel; pixel < (ringLevel + 1) * ledsPerLevel; pixel++) {
        //strip.setPixelColor(pixel, strip.Color(rTemp, gTemp, bTemp));
        strip.setColorDimmed(pixel, rTemp, gTemp, bTemp, brightness);
    }
}


// torch mode
// ==========
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

void calcNextColors(bool doGlimmer) {
    int textStart = text_base_line*ledsPerLevel;
    int textEnd = textStart+rowsPerGlyph*ledsPerLevel;
    
    for (int i=0; i<numLeds; i++) {
        int ei = i; // index into energy calculation buffer
        if (upside_down) {
            ei = numLeds - i;
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
                
            if(base_glimmer && doGlimmer && (i <= ledsPerLevel)) {
                // bottom glimmer (tbmsu)
                strip.setColorDimmed(i, r, random(20, 70), b, brightnessLevel);
            }
            else if (e>0) {
                // TODO was here
                if (base_glimmer && i <= ledsPerLevel) continue; // do not overwrite base glimmer color
                strip.setColorDimmed(i, r, g, b, brightnessLevel);
            } else {
              // background, no energy
              strip.setColorDimmed(i, red_bg, green_bg, blue_bg, brightnessLevel);
            }
        }
    }
}


// PARTICLE SETUP and MAIN LOOP
void setup() {
    // Cloud functions
    Particle.function("opmode", handleOpMode);
    Particle.function("message", handleMessage);
    
    // Temperature
    Serial.begin(9600);
    
    // Initialize LEDs
    strip.begin();
    strip.show(); // Initialize all pixels to 'off'
}

void loop() {
    static unsigned long lastTimeSync = millis();
    static unsigned long lastTimeDisplay = millis();
    static unsigned long lastTimeGlimmer = millis();
    static unsigned long lastTempRead = millis();
    static unsigned long lastRingDisplay = millis();
    
    static float lastTempC = 0.0;
    
     // Request time synchronization from the Particle Cloud (once a day)
    if (millis() - lastTimeSync > ONE_DAY_MILLIS) {
        Particle.syncTime();
        lastTimeSync = millis();
    }
    
    // display current time
    if (clock_interval>0 && (millis() - lastTimeDisplay > (1000*60*clock_interval))) {
        time_t now = Time.now(); // UTC
        now += clock_zone*3600; // add seconds east (=ahead) of UTC
        struct tm *loc;
        loc = localtime(&now);
        int secOfHour = loc->tm_min*60 + loc->tm_sec;
        
        char timeString[30];
        strftime(timeString, 30, clock_fmt, loc);
        newMessage(timeString);
        
        lastTimeDisplay = millis();
    }
    
    // read temperature every 10 seconds
    if (operationMode != mode_torch && (millis() - lastTempRead > 1000*10) && sensor.read()) {
        float currTempC = sensor.celsius();
        lastTempC = currTempC * 0.85; // correct "inner electronics" temperature
        lastTempRead = millis();
        
        Particle.publish("tempC_torch", String(currTempC));
        Particle.publish("tempC_torch_corr", String(lastTempC));
    }
    
    switch(operationMode) {
        case mode_off: {
            long ring_on;
            colorAll(0, 0, 0, 0);
            if (millis() - lastRingDisplay > ring_off_display_interval*1000) {
                lampTemeratureRing(ring_level, 150, lastTempC);
                if (millis() - lastRingDisplay > (ring_off_display_interval + ring_off_display_duration)*1000) {
                    lastRingDisplay = millis();
                }
            }
            break;
        }
        
        case mode_torch: {
            bool doGlimmer = false;
            if (millis() - lastTimeGlimmer > 1250) {
                doGlimmer = true;
                lastTimeGlimmer = millis();
            } else {
                doGlimmer = false;
            }
            injectRandom();
            calcNextEnergy();
            calcNextColors(doGlimmer);
            break;
        }
            
        case mode_lamp: {
            //colorAll(255, 190, 70, 180);
            colorAll(255, 180, 50, 180);
            lampTemeratureRing(0, 200, lastTempC);
            //colorGradient(strip.Color(255, 255, 50), strip.Color(200, 0, 100), 192);
            //colorGradient(strip.Color(200, 200, 0), strip.Color(150, 0, 0), 160);
            //colorGradient(strip.Color(200, 200, 0), strip.Color(0, 220, 0), 160);
            break;
        }
    }
    
    // render the text
    renderText();
    setTextPixels();
    
    // let it shine =)
    strip.show();
    delay(cycle_wait);
}
