/*********************************************************************
This is a library for our Monochrome OLEDs based on SSD1306 drivers

  Pick one up today in the adafruit shop!
  ------> http://www.adafruit.com/category/63_98

These displays use SPI to communicate, 4 or 5 pins are required to
interface

Adafruit invests time and resources providing this open source code,
please support Adafruit and open-source hardware by purchasing
products from Adafruit!

Written by Limor Fried/Ladyada  for Adafruit Industries.
BSD license, check license.txt for more information
All text above, and the splash screen below must be included in any redistribution
*********************************************************************/

// Datasheet:
// https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf

#ifdef __AVR__
  #include <avr/pgmspace.h>
#elif defined(ESP8266)
 #include <pgmspace.h>
#else
 //#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#endif

#if !defined(__ARM_ARCH) && !defined(ENERGIA) && !defined(ESP8266)
 //#include <util/delay.h>
#endif

#include <stdlib.h>

#include <Wire.h>
#include <SPI.h>
#include "ada_ssd1306.h"

void Adafruit_SSD1306::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if ((x < 0) || (x >= width()) || (y < 0) || (y >= height()))
    return;

  // check rotation, move pixel around if necessary
  switch (getRotation()) {
    case 1:
      swap(x, y);
      x = WIDTH - x - 1;
      break;
    case 2:
      x = WIDTH - x - 1;
      y = HEIGHT - y - 1;
      break;
    case 3:
      swap(x, y);
      y = HEIGHT - y - 1;
      break;
  }

  // x is which column
  switch (color) {
    case WHITE:
      buffer[x+(y/8)*WIDTH] |=  (1 << (y&7));
      break;
    case BLACK:
      buffer[x+(y/8)*WIDTH] &= ~(1 << (y&7));
      break;
    case INVERSE:
      buffer[x+(y/8)*WIDTH] ^=  (1 << (y&7));
      break;
  }
}

// Init sequence; that is not following the order proposed by the datasheet at
// page 64 at all!?!
const uint8_t initCommands[] = {
  SSD1306_DISPLAYOFF,
  SSD1306_SETDISPLAYCLOCKDIV, 0x80,
  SSD1306_SETMULTIPLEX,
  0,  // Offset 4
  SSD1306_SETDISPLAYOFFSET, 0,
  SSD1306_SETSTARTLINE | 0x0,
  SSD1306_CHARGEPUMP,
  0x10,  // Offset 10
  SSD1306_MEMORYMODE, // 0x20 horizontal addressing mode
  0x00,  // 0x0 act like ks0108
  SSD1306_SEGREMAP | 0x1,
  SSD1306_COMSCANDEC,
  SSD1306_SETCOMPINS,
  0x02,  // Offset 16
  SSD1306_SETCONTRAST,
  0x8F,  // Offset 18
  SSD1306_SETPRECHARGE,
  0x22,  // Offset 20
  SSD1306_SETVCOMDETECT, 0x40,
  SSD1306_DISPLAYALLON_RESUME, SSD1306_NORMALDISPLAY,
  SSD1306_DEACTIVATE_SCROLL,
  SSD1306_DISPLAYON,
};

const uint8_t scrollHorCommands[] = {
  0, 0, 0, 0, 0, 0, 0xFF, SSD1306_ACTIVATE_SCROLL
};

const uint8_t scrollDiagCommands[] = {
  SSD1306_SET_VERTICAL_SCROLL_AREA, 0, 0, 0, 0, 0, 0, 0, 1, SSD1306_ACTIVATE_SCROLL,
};


void Adafruit_SSD1306::begin(uint8_t vccstate, uint8_t i2caddr, bool reset) {
  _vccstate = vccstate;
  _i2caddr = i2caddr;

  // set pin directions
  if (dc != -1) {
    pinMode(dc, OUTPUT);
    pinMode(cs, OUTPUT);
#ifdef HAVE_PORTREG
    csport      = portOutputRegister(digitalPinToPort(cs));
    cspinmask   = digitalPinToBitMask(cs);
    dcport      = portOutputRegister(digitalPinToPort(dc));
    dcpinmask   = digitalPinToBitMask(dc);
#endif
    SPI.begin();
#ifdef SPI_HAS_TRANSACTION
#else
    SPI.setClockDivider(4);
#endif
  } else {
    // I2C Init
    Wire.begin();
#ifdef __SAM3X8E__
    // Force 400 KHz I2C, rawr! (Uses pins 20, 21 for SDA, SCL)
    TWI1->TWI_CWGR = 0;
    TWI1->TWI_CWGR = ((VARIANT_MCK / (2 * 400000)) - 4) * 0x101;
#endif
  }
  if ((reset) && (rst >= 0)) {
    // Setup reset pin direction (used by both SPI and I2C)
    pinMode(rst, OUTPUT);
    digitalWrite(rst, HIGH);
    // VDD (3.3V) goes high at start, lets just chill for a ms
    delay(1);
    // bring reset low
    digitalWrite(rst, LOW);
    // wait 10ms
    delay(10);
    // bring out of reset
    digitalWrite(rst, HIGH);
    // turn on VCC (9V?)
  }

  uint8_t buf[sizeof(initCommands)];
  memcpy(buf, initCommands, sizeof(initCommands));
  buf[4] = HEIGHT-1;
  if (vccstate != SSD1306_EXTERNALVCC) {
    buf[10] = 0x14;
  }
  if (HEIGHT > 32) {
    buf[16] = 0x12;
  }
  buf[18] = calcContrast();
  if (vccstate != SSD1306_EXTERNALVCC) {
    buf[20] = 0xF1;
  }
  ssd1306_commands(buf, sizeof(buf));
}

void Adafruit_SSD1306::invertDisplay(bool i) {
  ssd1306_command(i ? SSD1306_INVERTDISPLAY : SSD1306_NORMALDISPLAY);
}

void Adafruit_SSD1306::ssd1306_command(uint8_t c) {
  ssd1306_commands(&c, 1);
}

void Adafruit_SSD1306::ssd1306_commands(uint8_t *c, uint16_t len) {
  if (dc != -1) {
    // SPI
    setupSPI();
    // TODO(maruel): Use write()!!
    SPI.transfer(c, len);
#ifdef HAVE_PORTREG
    *csport |= cspinmask;
#else
    digitalWrite(cs, HIGH);
#endif
  } else {
    // I2C
    uint8_t buf[len*2];
    memset(buf, 0, sizeof(buf));
    uint8_t control = 0x00;   // Co = 0, D/C = 0
    for (int i = 0; i < len; i++) {
      buf[i*2+1] = c[i];
    }
    Wire.beginTransmission(_i2caddr);
    Wire.write(buf, sizeof(buf));
    Wire.endTransmission();
  }
}

void Adafruit_SSD1306::startScrollHor(bool left, uint8_t start, uint8_t stop) {
  uint8_t b[sizeof(scrollHorCommands)];
  memcpy(b, scrollHorCommands, sizeof(b));
  b[0] = left ? SSD1306_LEFT_HORIZONTAL_SCROLL  : SSD1306_RIGHT_HORIZONTAL_SCROLL;
  b[2] = start;
  b[4] = stop;
  ssd1306_commands(b, sizeof(b));
}

void Adafruit_SSD1306::startScrollDiag(bool left, uint8_t start, uint8_t stop) {
  uint8_t b[sizeof(scrollDiagCommands)];
  memcpy(b, scrollDiagCommands, sizeof(b));
  b[2] = HEIGHT;
  b[3] = left ? SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL : SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL;
  b[5] = start;
  b[7] = stop;
  ssd1306_commands(b, sizeof(b));
}

void Adafruit_SSD1306::stopscroll() {
  ssd1306_command(SSD1306_DEACTIVATE_SCROLL);
}

uint8_t Adafruit_SSD1306::calcContrast() {
  if (HEIGHT == 16) {
    if (_vccstate == SSD1306_EXTERNALVCC) {
      return 0x10;
    } else {
      return 0xAF;
    }
  }
  if (_vccstate == SSD1306_EXTERNALVCC) {
    return 0x9F;
  } else {
    return 0xCF;
  }
}

void Adafruit_SSD1306::dim(bool dim) {
  // The range of contrast to too small to be really useful; it is useful to dim
  // the display.
  uint8_t buf[2] = {SSD1306_SETCONTRAST, dim ? uint8_t(0) : calcContrast()};
  ssd1306_commands(buf, sizeof(buf));
}

void Adafruit_SSD1306::setupSPI() {
  if (dc != -1) {
#ifdef HAVE_PORTREG
    *csport |= cspinmask;
    *dcport |= dcpinmask;
    *csport &= ~cspinmask;
#else
    digitalWrite(cs, HIGH);
    digitalWrite(dc, HIGH);
    digitalWrite(cs, LOW);
#endif
  }
}

void Adafruit_SSD1306::display() {
  const uint8_t displayCommands[] = {
    SSD1306_COLUMNADDR,
    0,
    uint8_t(WIDTH-1),
    SSD1306_PAGEADDR, 0,     // Page start address (0 = reset)
    uint8_t(HEIGHT / 8 - 1), // Page end address
  };

  if (dc != -1) {
    // SPI
    setupSPI();
    SPI.transfer(buffer, WIDTH*HEIGHT/8);
#ifdef HAVE_PORTREG
    *csport |= cspinmask;
#else
    digitalWrite(cs, HIGH);
#endif
  } else {
    // save I2C bitrate
#ifdef TWBR
    uint8_t twbrbackup = TWBR;
    TWBR = 12; // upgrade to 400KHz!
#endif
    // I2C
    Wire.beginTransmission(_i2caddr);
    // TODO(maruel): Async version.
    for (uint16_t i = 0; i < WIDTH*HEIGHT/8; i+=16) {
      // Need to pack the data up.
      Wire.write(0x40);
      Wire.write(&buffer[i], 16);
    }
    Wire.endTransmission();
#ifdef TWBR
    TWBR = twbrbackup;
#endif
  }
}

void Adafruit_SSD1306::clearDisplay() {
  memset(buffer, 0, WIDTH*HEIGHT/8);
}

void Adafruit_SSD1306::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  boolean bSwap = false;
  switch(rotation) {
    case 0:
      // 0 degree rotation, do nothing
      break;
    case 1:
      // 90 degree rotation, swap x & y for rotation, then invert x
      bSwap = true;
      swap(x, y);
      x = WIDTH - x - 1;
      break;
    case 2:
      // 180 degree rotation, invert x and y - then shift y around for height.
      x = WIDTH - x - 1;
      y = HEIGHT - y - 1;
      x -= (w-1);
      break;
    case 3:
      // 270 degree rotation, swap x & y for rotation, then invert y  and adjust y for w (not to become h)
      bSwap = true;
      swap(x, y);
      y = HEIGHT - y - 1;
      y -= (w-1);
      break;
  }

  if (bSwap) {
    drawFastVLineInternal(x, y, w, color);
  } else {
    drawFastHLineInternal(x, y, w, color);
  }
}

void Adafruit_SSD1306::drawFastHLineInternal(int16_t x, int16_t y, int16_t w, uint16_t color) {
  // Do bounds/limit checks
  if (y < 0 || y >= HEIGHT) {
    return;
  }

  // make sure we don't try to draw below 0
  if (x < 0) {
    w += x;
    x = 0;
  }

  // make sure we don't go off the edge of the display
  if ((x + w) > WIDTH) {
    w = (WIDTH - x);
  }

  // if our width is now negative, punt
  if (w <= 0) {
    return;
  }

  // set up the pointer for  movement through the buffer
  register uint8_t *pBuf = buffer;
  // adjust the buffer pointer for the current row
  pBuf += ((y/8)*WIDTH);
  // and offset x columns in
  pBuf += x;
  register uint8_t mask = 1 << (y&7);
  switch (color) {
    case WHITE:
      while (w--) {
        *pBuf++ |= mask;
      }
      break;
    case BLACK:
      mask = ~mask;
      while (w--) {
        *pBuf++ &= mask;
      }
      break;
    case INVERSE:
      while(w--) {
        *pBuf++ ^= mask;
      }
      break;
  }
}

void Adafruit_SSD1306::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  bool bSwap = false;
  switch (rotation) {
    case 0:
      break;
    case 1:
      // 90 degree rotation, swap x & y for rotation, then invert x and adjust x for h (now to become w)
      bSwap = true;
      swap(x, y);
      x = WIDTH - x - 1;
      x -= (h-1);
      break;
    case 2:
      // 180 degree rotation, invert x and y - then shift y around for height.
      x = WIDTH - x - 1;
      y = HEIGHT - y - 1;
      y -= (h-1);
      break;
    case 3:
      // 270 degree rotation, swap x & y for rotation, then invert y
      bSwap = true;
      swap(x, y);
      y = HEIGHT - y - 1;
      break;
  }
  if (bSwap) {
    drawFastHLineInternal(x, y, h, color);
  } else {
    drawFastVLineInternal(x, y, h, color);
  }
}

void Adafruit_SSD1306::drawFastVLineInternal(int16_t x, int16_t __y, int16_t __h, uint16_t color) {
  // do nothing if we're off the left or right side of the screen
  if (x < 0 || x >= WIDTH) {
    return;
  }

  // make sure we don't try to draw below 0
  if (__y < 0) {
    // __y is negative, this will subtract enough from __h to account for __y being 0
    __h += __y;
    __y = 0;
  }

  // make sure we don't go past the height of the display
  if ((__y + __h) > HEIGHT) {
    __h = (HEIGHT - __y);
  }

  // if our height is now negative, punt
  if (__h <= 0) {
    return;
  }

  // this display doesn't need ints for coordinates, use local byte registers for faster juggling
  register uint8_t y = __y;
  register uint8_t h = __h;

  // set up the pointer for fast movement through the buffer
  register uint8_t *pBuf = buffer;
  // adjust the buffer pointer for the current row
  pBuf += ((y/8)*WIDTH);
  // and offset x columns in
  pBuf += x;

  // do the first partial byte, if necessary - this requires some masking
  register uint8_t mod = (y&7);
  if (mod) {
    // mask off the high n bits we want to set
    mod = 8-mod;

    // note - lookup table results in a nearly 10% performance improvement in fill* functions
    // register uint8_t mask = ~(0xFF >> (mod));
    static uint8_t premask[8] = {0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE };
    register uint8_t mask = premask[mod];

    // adjust the mask if we're not going to reach the end of this byte
    if (h < mod) {
      mask &= (0xFF >> (mod-h));
    }

    switch (color) {
      case WHITE:   *pBuf |=  mask;  break;
      case BLACK:   *pBuf &= ~mask;  break;
      case INVERSE: *pBuf ^=  mask;  break;
    }

    // fast exit if we're done here!
    if (h<mod) {
      return;
    }
    h -= mod;
    pBuf += WIDTH;
  }

  // write solid bytes while we can - effectively doing 8 rows at a time
  if (h >= 8) {
    if (color == INVERSE) {
      // separate copy of the code so we don't impact performance of the black/white write version with an extra comparison per loop
      do {
        *pBuf=~(*pBuf);
        // adjust the buffer forward 8 rows worth of data
        pBuf += WIDTH;

        // adjust h & y (there's got to be a faster way for me to do this, but this should still help a fair bit for now)
        h -= 8;
      } while(h >= 8);
    } else {
      // store a local value to work with
      register uint8_t val = (color == WHITE) ? 255 : 0;
      do {
        // write our value in
        *pBuf = val;

        // adjust the buffer forward 8 rows worth of data
        pBuf += WIDTH;

        // adjust h & y (there's got to be a faster way for me to do this, but this should still help a fair bit for now)
        h -= 8;
      } while(h >= 8);
    }
  }

  // now do the final partial byte, if necessary
  if (h) {
    mod = h & 7;
    // this time we want to mask the low bits of the byte, vs the high bits we did above
    // register uint8_t mask = (1 << mod) - 1;
    // note - lookup table results in a nearly 10% performance improvement in fill* functions
    static uint8_t postmask[8] = {0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F };
    register uint8_t mask = postmask[mod];
    switch (color) {
      case WHITE:   *pBuf |=  mask;  break;
      case BLACK:   *pBuf &= ~mask;  break;
      case INVERSE: *pBuf ^=  mask;  break;
    }
  }
}
