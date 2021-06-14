/*
 *  Project     Adalight FastLED
 *  @author     David Madison
 *  @link       github.com/dmadison/Adalight-FastLED
 *  @license    LGPL - Copyright (c) 2017 David Madison
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <Arduino.h>
#include <EEPROM.h>

// --- General Settings
const uint16_t 
	Num_Leds   =  80;         // strip length
uint8_t
  Brightness =  255;        // variable brightness
const uint8_t
  Max_Brightness =  255;        // maximum brightness

// --- FastLED Setings
#define LED_TYPE     WS2812B  // led strip type for FastLED
#define COLOR_ORDER  GRB      // color order for bitbang
#define PIN_DATA     6        // led data output pin
// #define PIN_CLOCK  7       // led data clock pin (uncomment if you're using a 4-wire LED type)

// --- Serial Settings
const unsigned long
	SerialSpeed    = 115200;  // serial port speed
const uint16_t
	SerialTimeout  = 60;      // time before LEDs are shut off if no data (in seconds), 0 to disable

// --- Optional Settings (uncomment to add)
#define SERIAL_FLUSH          // Serial buffer cleared on LED latch
// #define CLEAR_ON_START     // LEDs are cleared on reset
// #define STORE_BRIGHTNESS   // Store brightness in EEPROM if set by the user
// #define FIX_GAMMA          // Enables gamma correction

// --- Debug Settings (uncomment to add)
// #define DEBUG_LED 13       // toggles the Arduino's built-in LED on header match
// #define DEBUG_FPS 8        // enables a pulse on LED latch

// --------------------------------------------------------------------

#include <FastLED.h>

CRGB leds[Num_Leds];
uint8_t * ledsRaw = (uint8_t *)leds;

// Gamma correction table
const uint8_t PROGMEM gamma8[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

// A 'magic word' (along with LED count & checksum) precedes each block
// of LED data; this assists the microcontroller in syncing up with the
// host-side software and properly issuing the latch (host I/O is
// likely buffered, making usleep() unreliable for latch). You may see
// an initial glitchy frame or two until the two come into alignment.
// The magic word can be whatever sequence you like, but each character
// should be unique, and frequent pixel values like 0 and 255 are
// avoided -- fewer false positives. The host software will need to
// generate a compatible header: immediately following the magic word
// are three bytes: a 16-bit count of the number of LEDs (high byte
// first) followed by a simple checksum value (high byte XOR low byte
// XOR 0x55). LED data follows, 3 bytes per LED, in order R, G, B,
// where 0 = off and 255 = max brightness.

const uint8_t magic[] = {
  'A','d','a'};

// Use these to track what state we're in for our header
bool isMagic = false;
bool isCommand = false;

// A secondary magic word used to initiate the command protocol
// various device parameters
const uint8_t cmd[] = {
  'A','d','b'};

// String for state response
const uint8_t st_str[] = {
'S','T'};

// String for brightness response
const uint8_t br_str[] = {
'B','R'};

#define MAGICSIZE  sizeof(magic)

// Check values are header byte # - 1, as they are indexed from 0
#define HICHECK    (MAGICSIZE)
#define LOCHECK    (MAGICSIZE + 1)
#define CHECKSUM   (MAGICSIZE + 2)

enum processModes_t {Header, Data} mode = Header;

int16_t c;  // current byte, must support -1 if no data available
uint16_t outPos;  // current byte index in the LED array
uint32_t bytesRemaining;  // count of bytes yet received, set by checksum
unsigned long t, lastByteTime, lastAckTime;  // millisecond timestamps

void headerMode();
void dataMode();
void timeouts();

// Macros initialized
#ifdef SERIAL_FLUSH
	#undef SERIAL_FLUSH
	#define SERIAL_FLUSH while(Serial.available() > 0) { Serial.read(); }
#else
	#define SERIAL_FLUSH
#endif

#ifdef DEBUG_LED
	#define ON  1
	#define OFF 0

	#define D_LED(x) do {digitalWrite(DEBUG_LED, x);} while(0)
#else
	#define D_LED(x)
#endif

#ifdef DEBUG_FPS
	#define D_FPS do {digitalWrite(DEBUG_FPS, HIGH); digitalWrite(DEBUG_FPS, LOW);} while (0)
#else
	#define D_FPS
#endif

void setup(){
  // Check if we've stored brightness to EEPROM (if enabled), save it if not.
  #ifdef STORE_BRIGHTNESS
  int brightSet = EEPROM.read(0);
  if (brightSet != 1) {
    EEPROM.write(0,1);
    EEPROM.write(1,Brightness);
  } else {
    Brightness = EEPROM.read(1);
  }
  #endif
	#ifdef DEBUG_LED
		pinMode(DEBUG_LED, OUTPUT);
		digitalWrite(DEBUG_LED, LOW);
	#endif

	#ifdef DEBUG_FPS
		pinMode(DEBUG_FPS, OUTPUT);
	#endif

	#if defined(PIN_CLOCK) && defined(PIN_DATA)
		FastLED.addLeds<LED_TYPE, PIN_DATA, PIN_CLOCK, COLOR_ORDER>(leds, Num_Leds);
	#elif defined(PIN_DATA)
		FastLED.addLeds<LED_TYPE, PIN_DATA, COLOR_ORDER>(leds, Num_Leds);
	#else
		#error "No LED output pins defined. Check your settings at the top."
	#endif
	
if (Brightness <= Max_Brightness) {
	FastLED.setBrightness(Brightness);
  } else {
    FastLED.setBrightness(Max_Brightness);  
  }

	#ifdef CLEAR_ON_START
		FastLED.show();
	#endif

	Serial.begin(SerialSpeed);
  Serial.print("Ada\n"); // Send ACK string to host

	lastByteTime = lastAckTime = millis(); // Set initial counters
}

void loop(){ 
	t = millis(); // Save current time

	// If there is new serial data
	if((c = Serial.read()) >= 0){
		lastByteTime = lastAckTime = t; // Reset timeout counters

		switch(mode) {
			case Header:
				headerMode();
				break;
			case Data:
				dataMode();
				break;
		}
	}
	else {
		// No new data
		timeouts();
	}
}

void headerMode(){
	static uint8_t
		headPos,
		hi, lo, chk;

	if(headPos < MAGICSIZE){
    // Check if magic word(s) matches
    if(c == magic[headPos]) {
      isMagic = true;
      headPos++;
    } else if(c == cmd[headPos]) {
      isCommand = true;
      headPos++;
    } else {
      headPos = 0;
	}
  } else {    
		// Magic word matches! Now verify checksum
		switch(headPos){
			case HICHECK:
				hi = c;
				headPos++;
				break;
			case LOCHECK:
				lo = c;
				headPos++;
				break;
			case CHECKSUM:
				chk = c;
        if (isMagic) {
				if(chk == (hi ^ lo ^ 0x55)) {
					// Checksum looks valid. Get 16-bit LED count, add 1
					// (# LEDs is always > 0) and multiply by 3 for R,G,B.
					D_LED(ON);
					bytesRemaining = 3L * (256L * (long)hi + (long)lo + 1L);
					outPos = 0;
					memset(leds, 0, Num_Leds * sizeof(struct CRGB));
					mode = Data; // Proceed to latch wait mode
          }
        }
        // Easy Peasy
        if (isCommand) {
          // If Brightness command is issued, change brightness.
          if (hi == br_str[0] && lo == br_str[1]) {
            if (chk <= Max_Brightness && chk >= 0) {
              Brightness = chk;
              #ifdef STORE_BRIGHTNESS
                EEPROM.write(1,Brightness);
              #endif
              FastLED.setBrightness(Brightness);
              FastLED.show();
              lastByteTime = lastAckTime = millis(); // Set initial counters
            }            
          }
          // User has requested LED count and Brightness
          if (hi == st_str[0] && lo == st_str[1]) { // S & T
            Serial.print("Adb");
            Serial.print("N=");
            Serial.print(Num_Leds);
            Serial.print(";B=");
            Serial.println(Brightness);
            lastByteTime = lastAckTime = millis(); // Set initial counters
				}
        }     
        isMagic = false;
        isCommand = false;
				headPos = 0; // Reset header position regardless of checksum result
				break;
		}
	}
}

void dataMode(){
	// If LED data is not full
	if (outPos < sizeof(leds)){
    #ifdef FIX_GAMMA
      ledsRaw[outPos++] = pgm_read_byte(&gamma8[c]); // Issue next byte from gamma table
    #else
		  ledsRaw[outPos++] = c; // Issue next byte
    #endif
	}
	bytesRemaining--;
 
	if(bytesRemaining == 0) {
		// End of data -- issue latch:
		mode = Header; // Begin next header search
		FastLED.show();
		D_FPS;
		D_LED(OFF);
		SERIAL_FLUSH;
	}
}

void timeouts(){
	// No data received. If this persists, send an ACK packet
	// to host once every second to alert it to our presence.
	if((t - lastAckTime) >= 1000) {
    Serial.println("Ada"); // Send ACK string to host with LED count
		lastAckTime = t; // Reset counter

		// If no data received for an extended time, turn off all LEDs.
		if(SerialTimeout != 0 && (t - lastByteTime) >= (uint32_t) SerialTimeout * 1000) {
			memset(leds, 0, Num_Leds * sizeof(struct CRGB)); //filling Led array by zeroes
			FastLED.show();
			mode = Header;
			lastByteTime = t; // Reset counter
		}
	}
}
