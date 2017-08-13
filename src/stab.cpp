#include <LinkedList.h>
#include <math.h>
#include <stdint.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif
/* 
	 This code should be pasted within the files where this function is needed.
	 This function will not create any code conflicts.

	 The function call is similar to printf: ardprintf("Test %d %s", 25, "string");

	 To print the '%' character, use '%%'

	 This code was first posted on http://arduino.stackexchange.com/a/201
	 */

#ifndef ARDPRINTF
#define ARDPRINTF
#define ARDBUFFER 16 //Buffer for storing intermediate strings. Performance may vary depending on size.
#include <stdarg.h>

int ardprintf(char *str, ...) //Variadic Function
{
	int i, count=0, j=0, flag=0;
	char temp[ARDBUFFER+1];
	for(i=0; str[i]!='\0';i++)  if(str[i]=='%')  count++; //Evaluate number of arguments required to be printed

	va_list argv;
	va_start(argv, count);
	for(i=0,j=0; str[i]!='\0';i++) //Iterate over formatting string
	{
		if(str[i]=='%')
		{
			//Clear buffer
			temp[j] = '\0'; 
			Serial.print(temp);
			j=0;
			temp[0] = '\0';

			//Process argument
			switch(str[++i])
			{
				case 'd': Serial.print(va_arg(argv, int));
									break;
				case 'l': Serial.print(va_arg(argv, long));
									break;
				case 'f': Serial.print(va_arg(argv, double));
									break;
				case 'c': Serial.print((char)va_arg(argv, int));
									break;
				case 's': Serial.print(va_arg(argv, char *));
									break;
				default:  ;
			};
		}
		else 
		{
			//Add to buffer
			temp[j] = str[i];
			j = (j+1)%ARDBUFFER;
			if(j==0)  //If buffer is full, empty buffer.
			{
				temp[ARDBUFFER] = '\0';
				Serial.print(temp);
				temp[0]='\0';
			}
		}
	};

	Serial.println(); //Print trailing newline
	return count + 1; //Return number of arguments detected
}

#undef ARDBUFFER
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
#define STEP_DELAY 5
#define NUMPIXELS  50

// Setup Led Strip
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, DATA_PIN, NEO_GRB + NEO_KHZ800);

/*
	 sequenceMemory looks as follows:

	 1 byte           2 bytes(unsigned int)
	 | pin number (0-255) | time duration of pin | ...

*/
unsigned long before;
unsigned long after;
unsigned long diffRec;
unsigned long diffPlay;

unsigned long measure = 0;
unsigned long measureBeginning;
unsigned long measureEnd;

uint8_t mode = IDLE;

// Allocate memory
uint8_t sequenceMemory[SEQUENCE_MEMORY_SIZE] = {0};
uint8_t colorMap[NUMPIXELS][3];
uint8_t brightnessMap[NUMPIXELS];

// Initialize emtpy sequence
uint8_t *sequenceEnd = sequenceMemory;

uint8_t brightnessMode = 2;
uint8_t colorMode = 0;

void setup() {
	pinMode(X_PIN, INPUT);
	pinMode(Y_PIN, INPUT);
	pinMode(BUTTON_PIN, INPUT);
	pixels.begin();
	Serial.begin(9600);
}

uint8_t getCurrentPin() {

	int xOrig = analogRead(X_PIN);
	int yOrig = analogRead(Y_PIN);

	int xPosition = xOrig - 420;
	int yPosition = yOrig - 420;

	int diff = abs(xPosition) + abs(yPosition);

	if(diff < 100) {
		//Serial.println("diff below threshold");
		return INT8_MAX;
	}

	xPosition = xPosition;
	yPosition = -yPosition;

	float param = (float)xPosition/(float)yPosition;
	float rad = atan(param);
	float deg = rad * (180.0 / PI);

	if(yPosition > 0) {
		deg = 90 - deg;
	}
	if(yPosition < 0) {
		deg = 270 - deg;
	}

	float segment = 360.0/(float)NUMPIXELS;
	uint8_t pin = (((uint8_t)(round(deg/segment))+3*(NUMPIXELS/4)+1) % NUMPIXELS);
	//ardprintf("XPos %d YPos %d XPosAdj %d YPosAdj %d diff %d degree %f segmentWidth %f Pin %d", xOrig, yOrig, xPosition, yPosition, diff, deg, segment, pin);

	return pin;
}

void setColorMapBasedOnPin(uint8_t pin) {

	switch(colorMode) {
		case 0:
			for(uint8_t i=0; i<NUMPIXELS; ++i) {
				colorMap[i][0] = 1;
				colorMap[i][1] = 0;
				colorMap[i][2] = 0;
			}
			break;
		case 1:
			for(uint8_t i=0; i<NUMPIXELS; ++i) {
				uint8_t diff = pin%3;
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
			break;
	}
}

uint8_t distanceToPin(uint8_t first, uint8_t second) {
	return min(
			min(abs((NUMPIXELS - first) + second),abs((NUMPIXELS - second) + first)),
			abs(second - first));
}


void setBrightnessMapBasedOnPin(uint8_t pin) {
	if(pin == INT8_MAX) {
		for(uint8_t i=0; i<NUMPIXELS; ++i) {
			brightnessMap[i] = 0;
		}
		return;
	}
	switch(brightnessMode) {
		case 0:
			//one led at pin
			for(uint8_t i=0; i<NUMPIXELS; ++i) {
				if(i%10 == pin%10) {
					brightnessMap[i] = 250;
				}
				else {
					brightnessMap[i] = 0;
				}
			}
			break;
		case 1:
			//scattered dots equal distance
			for(uint8_t i=0; i<NUMPIXELS; ++i) {
				brightnessMap[i] = 0;
			}
			if(pin != INT8_MAX){
				brightnessMap[pin] = 200;
			}
			break;
		case 2:
			for(uint8_t i=0; i<NUMPIXELS; ++i) {
				uint8_t d = distanceToPin(i, pin);
				uint8_t val = (uint8_t)(((float)d/(float)NUMPIXELS)*500.0);
				//ardprintf("i: %d, pin: %d, dist: %d, val: %d", i, pin, d, val);
				brightnessMap[i] = val;//(uint8_t)(((float)d/(float)NUMPIXELS)*250.0);
			}
			break;
	}
}

void setPinOneStep(uint8_t pin) {
	//ardprintf("Setting pin %d for duration %d", pin, steps);
	setColorMapBasedOnPin(pin);
	setBrightnessMapBasedOnPin(pin);

	for(int i=0; i<NUMPIXELS; ++i) {
		pixels.setPixelColor(i, pixels.Color(colorMap[i][0]*brightnessMap[i],
					colorMap[i][1]*brightnessMap[i],
					colorMap[i][2]*brightnessMap[i]));
	}
	pixels.show();

	//Serial.println("PIN playback");
	//Serial.println(pin);

	delay(STEP_DELAY);
}

void playSequence() {
	while(true) {
		before = millis();

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
			uint16_t steps = *((uint16_t*)ptr);
			ptr+=2;

			for(uint16_t i = 0; i < steps; ++i) {
				setPinOneStep(pin);
			}

			//ardprintf("diffPlay %d duration %d", diff ,duration);
		}
		after = millis();
		diffPlay = after - before;
		Serial.println("delaying for");
		Serial.println(diffRec - diffPlay);
		delay(diffRec - diffPlay);
		Serial.println("diffPlay");
		Serial.println(diffPlay);
		Serial.println("diffRec");
		Serial.println(diffRec);
	}
}

void recordSequence() {

	uint8_t prevPin = 255;

	before = millis();
	while(true) {

		if(digitalRead(BUTTON_PIN) == LOW) {
			Serial.println("Recording done due to button release");
			mode = PLAYBACK;
			after = millis();
			diffRec = after - before;
			return;
		}

		if((sequenceEnd - sequenceMemory) > SEQUENCE_MEMORY_SIZE-2) {
			Serial.println("Memory limit reached, will no longer record");
			continue;
		}

		uint8_t curPin = getCurrentPin();
		setPinOneStep(curPin);

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
			setPinOneStep(pin);

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
