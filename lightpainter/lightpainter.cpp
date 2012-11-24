#include "lightpainter.h"

int SDI = 2; //Red wire (not the red 5V wire!)
int CKI = 3; //Green wire
int ledPin = 13; //On board LED
int barY = 0;

#define STRIP_LENGTH 160 //32 LEDs on this strip
int STRIP_LEN[3];
int STRIP_OFF[3];
long strip_colors[STRIP_LENGTH];

long colours[6];

void setup() {
  pinMode(SDI, OUTPUT);
  pinMode(CKI, OUTPUT);
  pinMode(ledPin, OUTPUT);

  //Clear out the array
  for (int x = 0; x < STRIP_LENGTH; x++)
    strip_colors[x] = 0;

  randomSeed(analogRead(0));

  STRIP_LEN[0] = 53;
  STRIP_LEN[1] = 53;
  STRIP_LEN[2] = 54;
  STRIP_OFF[0] = 1;
  STRIP_OFF[1] = 2;
  STRIP_OFF[2] = 0;

  colours[0] = 0x330000;
  colours[1] = 0x003300;
  colours[2] = 0x000033;
  colours[3] = 0x330033;
  colours[4] = 0x333300;
  colours[5] = 0x003333;
//	Serial.begin(9600);
//	Serial.println("Hello!");
}

void loop() {
  post_frame(); //Push the current color frame to the strip

  delay(2000);

  while (1) { //Do nothing
    for (int x = 0; x < 3; x++) {
      for (int y = 0; y < STRIP_LENGTH; y++) {
        setPoint(x, y, colours[((y + barY) / (160 / 6)) % 6]);

      }
    }
    post_frame(); //Push the current color frame to the strip
    barY += 1;
    if (barY >= 160) {
      barY = 0;
    }
    //digitalWrite(ledPin, HIGH);   // set the LED on
    delay(100);                  // wait for a second
    //digitalWrite(ledPin, LOW);    // set the LED off
    //delay(1000);                  // wait for a second
  }
}

void setPoint(int x, int y, long rgb) {
  int stripIndex = stripIndexForPosition(x, y);
  strip_colors[stripIndex] = rgb;
}

/*
 * strip x=0 | --- 000 --- --- 001 --- --- 002 --- --- --> 052 | offset=1
 * strip x=1 | --- --- 105 --- --- 104 --- --- 103 --- <-- 053 | offset=2
 * strip x=2 | 106 --- --- 107 --- --- 108 --- --- 109 --> 159 | offset=0
 *         y | 0   1   2   3   4   5   6   7   8   9   --> 159 |
 */
int stripIndexForPosition(int x, int y) {
  if (isPointMappedOnStrip(x, y)) {
    switch (x) {
    case 0:
      return stripIndex = (y - 1) / 3;
    case 1:
      return (STRIP_LEN[0] + STRIP_LEN[1] - 1) - ((y - 2) / 3);
    case 2:
      return (y / 3) + (STRIP_LEN[0] + STRIP_LEN[1]);
    }
    return -1;
  }
}

bool isPointMappedOnStrip(int stripNumber, int y) {
  return ((y - STRIP_OFF[stripNumber])) % 3 == 0;
}

/*
 * x=2       x=0      x=1
 * x 0 0 0   0 0 x 2  0 x 0 1
 * 0 x 0 1   x 0 0 0  0 0 x 2
 * 0 0 x 2   0 x 0 1  x 0 0 0
 *
 * 0 % 3 = 0
 * 1 % 3 = 1
 * 2 % 3 = 2
 */
int seekForPoint(int x, int y) {
  return ((x / 3) * 3) + ((y + 2) % 3) + (x % 3);
}

int preSeekForPoint(int x, int y) {
  return seekForPoint(x, y);
}

int postSeekForPoint(int x, int y, int width) {
  return width - seekForPoint(x, y);
}

//Takes the current strip color array and pushes it out
void post_frame(void) {
  //Each LED requires 24 bits of data
  //MSB: R7, R6, R5..., G7, G6..., B7, B6... B0
  //Once the 24 bits have been delivered, the IC immediately relays these bits to its neighbor
  //Pulling the clock low for 500us or more causes the IC to post the data.

  for (int LED_number = 0; LED_number < STRIP_LENGTH; LED_number++) {
    long this_led_color = strip_colors[LED_number]; //24 bits of color data

    for (byte color_bit = 23; color_bit != 255; color_bit--) {
      //Feed color bit 23 first (red data MSB)

      digitalWrite(CKI, LOW); //Only change data when clock is low

      long mask = 1L << color_bit;
      //The 1'L' forces the 1 to start as a 32 bit number, otherwise it defaults to 16-bit.

      if (this_led_color & mask)
        digitalWrite(SDI, HIGH);
      else
        digitalWrite(SDI, LOW);

      digitalWrite(CKI, HIGH); //Data is latched when clock goes high
    }
  }

  //Pull clock low to put strip into reset/post mode
  digitalWrite(CKI, LOW);
  delayMicroseconds(500); //Wait for 500us to go into reset
}

void bmpDraw(char *filename, uint8_t x, uint8_t y) {

  File bmpFile;
  int bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t sdbuffer[3 * BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint8_t buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean goodBmp = false;       // Set to true on valid header parse
  boolean flip = true;        // BMP is stored bottom-to-top
  int w, h, row, col;
  uint8_t r, g, b;
  uint32_t pos = 0, startTime = millis();

  if ((x >= tft.width()) || (y >= tft.height()))
    return;

  Serial.println();
  Serial.print("Loading image '");
  Serial.print(filename);
  Serial.println('\'');

  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
    Serial.print("File not found");
    return;
  }

  // Parse BMP header
  if (read16(bmpFile) == 0x4D42) { // BMP signature
    Serial.print("File size: ");
    Serial.println(read32(bmpFile));
    (void) read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print("Image Offset: ");
    Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print("Header size: ");
    Serial.println(read32(bmpFile));
    bmpWidth = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if (read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      Serial.print("Bit Depth: ");
      Serial.println(bmpDepth);
      if ((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        Serial.print("Image size: ");
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if (bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip = false;
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if ((x + w - 1) >= tft.width())
          w = tft.width() - x;
        if ((y + h - 1) >= tft.height())
          h = tft.height() - y;

        // Set TFT address window to clipped image bounds
        tft.setAddrWindow(x, y, x + w - 1, y + h - 1);

        for (row = 0; row < h; row++) { // For each scanline...

          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if (flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else
            // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if (bmpFile.position() != pos) { // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          // Seek to pixel
          bmpFile.seek();

          // Read out scan line pixel to light array
          for (col = 0; col < w; col++) { // For each pixel...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format, push to display
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            setPoint(col, row, ((long) r << 16) && ((long) g << 8) && (long) b);
          } // end pixel

          // Seek to end of scan line
          bmpFile.seek();

        } // end scanline
        Serial.print("Loaded in ");
        Serial.print(millis() - startTime);
        Serial.println(" ms");
      } // end goodBmp
    }
  }

  bmpFile.close();
  if (!goodBmp)
    Serial.println("BMP format not recognized.");
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File f) {
  uint16_t result;
  ((uint8_t *) &result)[0] = f.read(); // LSB
  ((uint8_t *) &result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File f) {
  uint32_t result;
  ((uint8_t *) &result)[0] = f.read(); // LSB
  ((uint8_t *) &result)[1] = f.read();
  ((uint8_t *) &result)[2] = f.read();
  ((uint8_t *) &result)[3] = f.read(); // MSB
  return result;
}

