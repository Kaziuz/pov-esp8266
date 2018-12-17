
#include "FastLED.h"
//#define NUM_LEDS 60
#define NUM_LEDS 78
#define DATA_PIN D1
#define CLOCK_PIN D2
CRGB leds[NUM_LEDS];
//FastLED.addLeds<APA102>(leds, NUM_LEDS);
#include <Arduino.h>
//#include <Adafruit_DotStar.h>
//#include <SPI.h> // Enable this line on Pro Trinket
typedef uint16_t line_t; // Bigger images OK on other boards

//#include <programspace.h>
 

//// These are added to attempt to store graphics.h info into flash instead of ram, see this http://arduino-esp8266.readthedocs.io/en/latest/PROGMEM.html 
//#define PROGMEM  ICACHE_RODATA_ATTR
//#define ICACHE_RODATA_ATTR  __attribute__((section(".irom.text")))
//void* memcpy_P(void* dest, PGM_VOID_P src, size_t count);
//#define PGM_P       const char *
//#define PGM_VOID_P  const void *
//#define PSTR(s) (__extension__({static const char __c[] PROGMEM = (s); &__c[0];}))
//// end flash storage addition


//#include "graphics.h" //does not work without removing PROGMEM - how can this be stored and read from flash?
#include "graphicsesp.h" //works with esp8266 by removing PROGMEM


boolean autoCycle = true; // Set to true to cycle images by default
#define CYCLE_TIME 4     // Time, in seconds, between auto-cycle images


//#if defined(LED_DATA_PIN) && defined(LED_CLOCK_PIN)
//// Older DotStar LEDs use GBR order.  If colors are wrong, edit here.
//Adafruit_DotStar strip = Adafruit_DotStar(NUM_LEDS,
//  LED_DATA_PIN, LED_CLOCK_PIN, DOTSTAR_BRG);
//#else
//Adafruit_DotStar strip = Adafruit_DotStar(NUM_LEDS, DOTSTAR_BGR);
//#endif

void     imageInit(void);

void setup() {
   FastLED.addLeds<DOTSTAR, DATA_PIN, CLOCK_PIN, GRB>(leds, NUM_LEDS);

  //strip.clear();                // Make sure strip is clear
  FastLED.show();                  // before measuring battery
  imageInit(); // Initialize pointers for default image
}




// GLOBAL STATE STUFF ------------------------------------------------------

uint32_t lastImageTime = 0L; // Time of last image change
uint8_t  imageNumber   = 0,  // Current image being displayed
         imageType,          // Image type: PALETTE[1,4,8] or TRUECOLOR
        *imagePalette,       // -> palette data in PROGMEM
        *imagePixels,        // -> pixel data in PROGMEM
         palette[16][3];     // RAM-based color table for 1- or 4-bit images
line_t   imageLines,         // Number of lines in active image
         imageLine;          // Current line number in image



//from supernova code
void imageInit() { // Initialize global image state for current imageNumber
  imageType    = images[imageNumber].type;
  imageLines   = images[imageNumber].lines;
  imageLine    = 0;
  imagePalette = (uint8_t *)images[imageNumber].palette;
  imagePixels  = (uint8_t *)images[imageNumber].pixels;
  // 1- and 4-bit images have their color palette loaded into RAM both for
  // faster access and to allow dynamic color changing.  Not done w/8-bit
  // because that would require inordinate RAM (328P could handle it, but
  // I'd rather keep the RAM free for other features in the future).
  if(imageType == PALETTE1)      memcpy_P(palette, imagePalette,  2 * 3);
  else if(imageType == PALETTE4) memcpy_P(palette, imagePalette, 16 * 3);
  lastImageTime = millis(); // Save time of image init for next auto-cycle
}



void nextImage(void) {
  if(++imageNumber >= NUM_IMAGES) imageNumber = 0;
  imageInit();
}

// MAIN LOOP ---------------------------------------------------------------

void loop() {
  uint32_t t = millis();               // Current time, milliseconds

  if(autoCycle) {
    if((t - lastImageTime) >= (CYCLE_TIME * 1000L)) nextImage();
    // CPU clocks vary slightly; multiple poi won't stay in perfect sync.
    // Keep this in mind when using auto-cycle mode, you may want to cull
    // the image selection to avoid unintentional regrettable combinations.
  }



  switch(imageType) { 


//from original adafruit sketch?
    case PALETTE1: { // 1-bit (2 color) palette-based image
      uint8_t  pixelNum = 0, byteNum, bitNum, pixels, idx,
              *ptr = (uint8_t *)&imagePixels[imageLine * NUM_LEDS / 8];
      for(byteNum = NUM_LEDS/8; byteNum--; ) { // Always padded to next byte
        pixels =*ptr++;  // 8 pixels of data (pixel 0 = LSB)
        for(bitNum = 8; bitNum--; pixels >>= 1) {
          idx = pixels & 1; // Color table index for pixel (0 or 1)
          leds[pixelNum++] = CRGB (palette[idx][0], palette[idx][1], palette[idx][2]);
        }
      }
      break;
    }



    case PALETTE4: { // 4-bit (16 color) palette-based image
      uint8_t  pixelNum, p1, p2,
              *ptr = (uint8_t *)&imagePixels[imageLine * NUM_LEDS / 2];
      for(pixelNum = 0; pixelNum < NUM_LEDS; ) {
        p2  = pgm_read_byte(ptr++); // Data for two pixels...
        p1  = p2 >> 4;              // Shift down 4 bits for first pixel
        p2 &= 0x0F;                 // Mask out low 4 bits for second pixel
       leds[pixelNum++] = CRGB (palette[p1][0], palette[p1][1], palette[p1][2]);
       leds[pixelNum++] = CRGB (palette[p2][0], palette[p2][1], palette[p2][2]);
      }
      break;
    }


    case PALETTE8: { // 8-bit (256 color) PROGMEM-palette-based image
      uint16_t  o;
      uint8_t   pixelNum,
               *ptr = (uint8_t *)&imagePixels[imageLine * NUM_LEDS];
      for(pixelNum = 0; pixelNum < NUM_LEDS; pixelNum++) {
        o = pgm_read_byte(ptr++) * 3; // Offset into imagePalette
        leds[pixelNum++] = CRGB(
          pgm_read_byte(&imagePalette[o]),
          pgm_read_byte(&imagePalette[o + 1]),
          pgm_read_byte(&imagePalette[o + 2]));
      }
      break;
    }

    case TRUECOLOR: { // 24-bit ('truecolor') image (no palette)
      uint8_t  pixelNum, r, g, b,
              *ptr = (uint8_t *)&imagePixels[imageLine * NUM_LEDS * 3];
      for(pixelNum = 0; pixelNum < NUM_LEDS; pixelNum++) {
        r = pgm_read_byte(ptr++);
        g = pgm_read_byte(ptr++);
        b = pgm_read_byte(ptr++);
        leds[pixelNum] = (pixelNum, r, g, b);
      }
      break;
    }
  }

  FastLED.show();  // Refresh LEDs
//#if !defined(LED_DATA_PIN) && !defined(LED_CLOCK_PIN)
//  delayMicroseconds(900);  // Because hardware SPI is ludicrously fast
//#endif
  if(++imageLine >= imageLines) imageLine = 0; // Next scanline, wrap around
}





