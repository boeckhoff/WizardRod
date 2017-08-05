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

// Parameters
#define STEP_DELAY 30
#define NUMPIXELS  50

// Setup Led Strip
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, DATA_PIN, NEO_GRB + NEO_KHZ800);

/*
   sequenceMemory looks as follows:

   1 byte           2 bytes(unsigned int)
   | pin number (0-255) | time duration of pin | ...

*/

uint8_t mode = IDLE;

// Allocate memory
uint8_t sequenceMemory[SEQUENCE_MEMORY_SIZE] = {0};
uint8_t colorMap[NUMPIXELS][3];
uint8_t brightnessMap[NUMPIXELS];

// Initialize emtpy sequence
uint8_t *sequenceEnd = sequenceMemory;

uint8_t BrightnessMode = 1;

void setup() {
	pinMode(X_PIN, INPUT);
	pinMode(Y_PIN, INPUT);
	pinMode(BUTTON_PIN, INPUT);
	pixels.begin();
	Serial.begin(9600);
}

uint8_t getCurrentPin() {

	int xPosition = analogRead(X_PIN) - 512;
	int yPosition = analogRead(Y_PIN) - 512;

	float param = (float)xPosition/(float)yPosition;
	float rad = atan(param);
	float deg = rad * (180.0 / PI);

	/*
	if(yPosition > 0) {
		deg = 90 - deg;
	}
	if(yPosition < 0) {
		deg = 270 - deg;
	}
	*/

	float segment = 360.0/(float)NUMPIXELS;
	uint8_t pin = (uint8_t)(round(deg/segment));
	//ardprintf("XPos %d YPos %d XPosAdj %d YPosAdj %d AtanOrig %f AtanAdj %f Pin %d", xOrig, yOrig, xPosition, yPosition, atOrig, at, pin);
	return pin;
}

void setColorMapBasedOnPin(uint8_t pin) {
	for(uint8_t i=0; i<NUMPIXELS; ++i) {
		uint8_t diff = max(i%3,pin%3) - min(i%3,pin%3);
		switch(diff) {
			case 0:
				colorMap[i][0] = 1;
				colorMap[i][1] = 0;
				colorMap[i][2] = 0;
				break;
			case 1:
				colorMap[i][0] = 0;
				colorMap[i][1] = 1;
				colorMap[i][2] = 0;
				break;
			case 2:
				colorMap[i][0] = 0;
				colorMap[i][1] = 0;
				colorMap[i][2] = 1;
				break;
		}
	}
}

void setBrightnessMapBasedOnPin(uint8_t pin) {
	switch(BrightnessMode) {
		case 0:
		  for(uint8_t i=0; i<NUMPIXELS; ++i) {
			  if(i%10 == pin%10) {
				  brightnessMap[i] = 200;
			  }
			  else {
				  brightnessMap[i] = 0;
			  }
		  }
		  break;
		case 1:
		  for(uint8_t i=0; i<NUMPIXELS; ++i) {
			brightnessMap[i] = 0;
		  }
		  brightnessMap[pin] = 200;
		  break;
	}
}

void setPinForDuration(uint8_t pin, uint16_t steps) {
	setColorMapBasedOnPin(pin);
	setBrightnessMapBasedOnPin(pin);

	for(int i=0; i<NUMPIXELS; ++i) {
		pixels.setPixelColor(i, pixels.Color(colorMap[i][0]*brightnessMap[i],
					colorMap[i][1]*brightnessMap[i],
					colorMap[i][1]*brightnessMap[i]));
	}
	pixels.show();

	Serial.println("PIN playback");
	Serial.println(pin);

	for(uint16_t i = 0; i < steps; i++) {
		delay(STEP_DELAY);
	}
}

void playSequence() {

	while(true) {

		uint8_t *ptr = sequenceMemory;

		while(ptr != sequenceEnd) {

			if(digitalRead(BUTTON_PIN) == HIGH) {

				// wait until button released
				while(digitalRead(BUTTON_PIN) == HIGH) {}

				Serial.println("Button stopped playSequence");

				// reset sequence by setting endpointer to start of sequence
				sequenceEnd = sequenceMemory;

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

	uint8_t prevPin = 255;

	while(true) {

		if(digitalRead(BUTTON_PIN) == LOW) {
			Serial.println("Recording done due to button release");
			mode = PLAYBACK;
			return;
		}

		if((sequenceEnd - sequenceMemory) > SEQUENCE_MEMORY_SIZE-2) {
			Serial.println("Memory limit reached, will no longer record");
			continue;
		}

		uint8_t curPin = getCurrentPin();
		setPinForDuration(curPin, 1);

		if(curPin == prevPin) {
			// increment previous step counter
			*((uint16_t*)(sequenceEnd-2)) += 1;
		}

		else {
			// add new pin and set step counter to 1
			*sequenceEnd = curPin;
			sequenceEnd++;
			*((uint16_t*)sequenceEnd) = 1;
			sequenceEnd += 2;
		}

		prevPin = curPin;
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
