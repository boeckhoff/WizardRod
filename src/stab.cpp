#include <LinkedList.h>
#include <math.h>
#include <stdint.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
	#include <avr/power.h>
#endif

// Pins
#define X_PIN A3
#define Y_PIN A5
#define DATA_PIN 5
#define BUTTON_PIN 9

#define POSITION_MAX 1000
#define SEQUENCE_MEMORY_SIZE 1000

// Modes
#define IDLE 0
#define RECORDING 1
#define PLAYBACK 2

#define STEP_DELAY 30
#define NUMPIXELS  50

// Setup Led Strip
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, DATA_PIN, NEO_GRB + NEO_KHZ800);

/*
   sequenceMemory

     1 byte             2 bytes(unsigned int)
 | pin number (0-255) | time duration of pin | ...

*/

uint8_t mode = IDLE;
uint8_t sequenceMemory[SEQUENCE_MEMORY_SIZE] = {0};
uint8_t *sequenceEnd = (uint8_t*)sequenceMemory;

void setup() {
  pinMode(X_PIN, INPUT);
  pinMode(Y_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT);
  pixels.begin();
  Serial.begin(9600);
}

void turnOffAllPixels() {
	for(int i = 0; i < NUMPIXELS; ++i) {
		pixels.setPixelColor(i, pixels.Color(0,0,0));
	}
}

uint8_t getCurrentPin() {

  int xPosition = analogRead(X_PIN) - 512;
  int yPosition = analogRead(Y_PIN) - 512;

  float param = (float)xPosition/(float)yPosition;
  float rad = atan(param);
  float deg = rad * (180 / PI);

  /*
	 if(yPosition > 0) {
	 at = 90 - at;
	 }
	 if(xPosition < 0 && yPosition < 0) {
	 at = 270 - at;
	 }
	 if(xPosition > 0 && yPosition < 0) {
	 at = 270 - at;
	 }
	 */
  float segment = 360.0/(float)NUMPIXELS;

  uint8_t pin = (int)deg/segment;
  //if(pin != prevPin)
  //ardprintf("XPos %d YPos %d XPosAdj %d YPosAdj %d AtanOrig %f AtanAdj %f Pin %d", xOrig, yOrig, xPosition, yPosition, atOrig, at, pin);
  return pin;
}

void setPinForDuration(uint8_t pin, uint16_t steps) {
	turnOffAllPixels();
  pixels.setPixelColor(pin, pixels.Color(0,150,0));
  pixels.show();

  Serial.println("PIN playback");
  Serial.println(pin);

  for(uint16_t i = 0; i < steps; i++) {
		delay(STEP_DELAY);
  }
}

void playSequence() {

  while(true) {

	uint8_t *ptr = (uint8_t*)sequenceMemory;

	while(ptr != sequenceEnd) {

	  if(digitalRead(BUTTON_PIN) == HIGH) {

			// wait until button released
			while(digitalRead(BUTTON_PIN) == HIGH) {}

			Serial.println("Button stopped playSequence");

			// reset sequence by setting endpointer to start of sequence
			sequenceEnd = (uint8_t*)sequenceMemory;

			mode = IDLE;
			return;
	  }

		// get pin and duration from sequenceMemory
	  uint8_t pin = *ptr;
	  ptr++;
	  uint16_t duration = *((uint16_t*)ptr);
	  ptr+=2;

	  setPinForDuration(pin, duration);
		}
  }
}

void recordSequence() {

	// when this function is called, sequenceEnd should be at beginning of sequenceMemory array

	uint8_t prevPin = 255;

	while(true) {

		if(digitalRead(BUTTON_PIN) == LOW) {
			Serial.println("Recording done due to button release");
			mode = PLAYBACK;
			return;
		}

		if((sequenceEnd - sequenceMemory) == SEQUENCE_MEMORY_SIZE) {
			Serial.println("Memory limit reached, will no longer record");
			continue;
		}

		uint8_t curPin = getCurrentPin();

		if(curPin == prevPin) {
			// increment previous step counter
			sequenceEnd -= 2;
			*((uint16_t*)sequenceEnd) += 1;
			sequenceEnd += 2;
		}

		else {
			// add new pin and set step counter to 1
			*sequenceEnd = curPin;
			sequenceEnd++;
			*((uint16_t*)sequenceEnd) = 1;
			sequenceEnd += 2;
		}

		prevPin = curPin;
		delay(STEP_DELAY);
	}
}


void loop() {

	switch(mode) {

		case IDLE:
			uint8_t pin;
			pin = getCurrentPin();
			setPinForDuration(pin, 1);

			if(digitalRead(BUTTON_PIN) == HIGH) {
				mode = RECORDING;
			}
			break;

		case RECORDING:
			recordSequence();
			break;

		case PLAYBACK:
			playSequence();
			break;
	}
}
