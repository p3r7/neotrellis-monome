/***********************************************************
 *  DIY monome compatible grid w/ Adafruit NeoTrellis
 *  for Teensy
 *
 *  This code makes the Adafruit Neotrellis boards into a Monome compatible grid via monome's mext protocol
 *  ----> https://www.adafruit.com/product/3954
 *
 *  Code here is for a 16x8 grid, but can be modified for 4x8, 8x8, or 16x16 (untested on larger grid arrays)
 *
 *  Many thanks to:
 *  scanner_darkly <https://github.com/scanner-darkly>,
 *  TheKitty <https://github.com/TheKitty>,
 *  Szymon Kaliski <https://github.com/szymonkaliski>,
 *  John Park, Todbot, Juanma, Gerald Stevens, and others
 *
*/

#include "MonomeSerialDevice.h"
#include <Adafruit_NeoTrellis.h>
#include "debug.h"

#define NUM_ROWS 8 // DIM_Y number of rows of keys down
#define NUM_COLS 16 // DIM_X number of columns of keys across

#define INT_PIN 9
#define LED_PIN 13 // teensy LED

// This assumes you are using a USB breakout board to route power to the board
// If you are plugging directly into the teensy, you will need to adjust this brightness to a much lower value
#define BRIGHTNESS 255 // overall grid brightness - use gamma table below to adjust levels

// RGB values - vary for colors other than white
#define DEFAULT_R 255
#define DEFAULT_G 255
#define DEFAULT_B 255
//  amber-ish {255,191,0}
//  warmer white? {255,255,200}

// Gamma table to customize 16 levels of brightness
// RGB values above will be scaled to these
const uint8_t gammaTable[16] = { 2,  3,  4,  6,  11, 18, 25, 32, 41, 59, 70, 80, 92, 103, 115, 128};


bool isInited = false;
elapsedMillis monomeRefresh;

// set your monome device name here
String deviceID = "neo-monome";

// Monome class setup
MonomeSerialDevice mdp;

int prevLedBuffer[mdp.MAXLEDCOUNT];


// NeoTrellis setup
/*
// 8x8 setup
Adafruit_NeoTrellis trellis_array[NUM_ROWS / 4][NUM_COLS / 4] = {
  { Adafruit_NeoTrellis(0x2F), Adafruit_NeoTrellis(0x2E) },
  { Adafruit_NeoTrellis(0x3E), Adafruit_NeoTrellis(0x36) }
};
*/
// 16x8
Adafruit_NeoTrellis trellis_array[NUM_ROWS / 4][NUM_COLS / 4] = {
  { Adafruit_NeoTrellis(0x32), Adafruit_NeoTrellis(0x30), Adafruit_NeoTrellis(0x2F), Adafruit_NeoTrellis(0x2E) }, // top row
  { Adafruit_NeoTrellis(0x33), Adafruit_NeoTrellis(0x31), Adafruit_NeoTrellis(0x3E), Adafruit_NeoTrellis(0x36) } // bottom row
};

/*
// 16x16
Adafruit_NeoTrellis trellis_array[NUM_ROWS / 4][NUM_COLS / 4] = {
  { Adafruit_NeoTrellis(0x33), Adafruit_NeoTrellis(0x31), Adafruit_NeoTrellis(0x2F), Adafruit_NeoTrellis(0x2E)}, // top row
  { Adafruit_NeoTrellis(0x35), Adafruit_NeoTrellis(0x39), Adafruit_NeoTrellis(0x3F), Adafruit_NeoTrellis(0x37)}, // bottom row
  { Adafruit_NeoTrellis(0x43), Adafruit_NeoTrellis(0x41), Adafruit_NeoTrellis(0x36), Adafruit_NeoTrellis(0x3E)},
  { Adafruit_NeoTrellis(0x45), Adafruit_NeoTrellis(0x49), Adafruit_NeoTrellis(0x4d), Adafruit_NeoTrellis(0x47)}
};
*/

Adafruit_MultiTrellis trellis((Adafruit_NeoTrellis *)trellis_array, NUM_ROWS / 4, NUM_COLS / 4);



// ***************************************************************************
// **                                HELPERS                                **
// ***************************************************************************

// Pad a string of length 'len' with nulls
void pad_with_nulls(char* s, int len) {
  int l = strlen(s);
  for( int i=l;i<len; i++) {
    s[i] = '\0';
  }
}

// Input a value 0 to 255 to get a color value.
// The colors are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
   return seesaw_NeoPixel::Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return seesaw_NeoPixel::Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= 170;
   return seesaw_NeoPixel::Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  return 0;
}

// ***************************************************************************
// **                          FUNCTIONS FOR TRELLIS                        **
// ***************************************************************************


// Define a callback for key presses
TrellisCallback keyCallback(keyEvent evt){
	uint8_t x;
	uint8_t y;
	x  = evt.bit.NUM % NUM_COLS; // NUM_COLS;
	y = evt.bit.NUM / NUM_COLS; // NUM_COLS;
	if(evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING){
		//on
		//Serial.println(" pressed ");
		mdp.sendGridKey(x, y, 1);
	}else if(evt.bit.EDGE == SEESAW_KEYPAD_EDGE_FALLING){
		//off
		//Serial.println(" released ");
		mdp.sendGridKey(x, y, 0);
	}
	return 0;
}

// ***************************************************************************
// **                                 SETUP                                 **
// ***************************************************************************

void setup(){
	uint8_t x, y;
	Serial.begin(115200);

   // monome connection setup
  mdp.isMonome = true;
  mdp.deviceID = deviceID;
  mdp.setupAsGrid(NUM_ROWS, NUM_COLS);
  mdp.R = DEFAULT_R;
  mdp.G = DEFAULT_G;
  mdp.B = DEFAULT_B;
  monomeRefresh = 0;
  isInited = true;

  int var = 0;
  while (var < 8) {
    mdp.poll();
    var++;
    delay(100);
  }

  if (!trellis.begin()) {
    Serial.println("trellis.begin() failed!");
    Serial.println("check your addresses.");
    Serial.println("reset to try again.");
    while(1);  // loop forever
  }

	// setup trellis key callbacks
	for (y = 0; y < NUM_ROWS; y++) {
		for (x = 0; x < NUM_COLS; x++) {
		  trellis.activateKey(x, y, SEESAW_KEYPAD_EDGE_RISING, true);
		  trellis.activateKey(x, y, SEESAW_KEYPAD_EDGE_FALLING, true);
		  trellis.registerCallback(x, y, keyCallback);
		}
	}

  // set overall brightness for all pixels
  for (x = 0; x < NUM_COLS / 4; x++) {
    for (y = 0; y < NUM_ROWS / 4; y++) {
      trellis_array[y][x].pixels.setBrightness(BRIGHTNESS);
    }
  }

  // clear grid leds
  // delay(100);
  mdp.setAllLEDs(0);
  sendLeds();

  // Blink one led to show it's started up
  trellis.setPixelColor(0, 0xFFFFFF);
  trellis.show();
  delay(100);
  trellis.setPixelColor(0, 0x000000);
  trellis.show();
}

// ***************************************************************************
// **                                SEND LEDS                              **
// ***************************************************************************

void sendLeds(){
  uint8_t value, prevValue = 0;
  uint32_t hexColor;
  bool isDirty = false;

  for(int i=0; i< NUM_ROWS * NUM_COLS; i++){
    value = mdp.leds[i];
    prevValue = prevLedBuffer[i];
    uint8_t gvalue = gammaTable[value];

    if (value != prevValue || mdp.colorDirty) {
      //hexColor = (((mdp.R * value) >> 4) << 16) + (((mdp.G * value) >> 4) << 8) + ((mdp.B * value) >> 4);
      hexColor =  (((gvalue*mdp.R)/256) << 16) + (((gvalue*mdp.G)/256) << 8) + (((gvalue*mdp.B)/256) << 0);
      trellis.setPixelColor(i, hexColor);

      prevLedBuffer[i] = value;
      isDirty = true;
    }
  }

  if (isDirty) {
    trellis.show();
    mdp.colorDirty = false;
  }
}



// ***************************************************************************
// **                                 LOOP                                  **
// ***************************************************************************

void loop() {

    mdp.poll(); // process incoming serial from Monomes

    // refresh every 16ms or so
    if (isInited && monomeRefresh > 16) {
        trellis.read();
        sendLeds();
        monomeRefresh = 0;
    }

}
