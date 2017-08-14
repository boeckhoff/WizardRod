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
#define REC_BUTTON_PIN 9
#define MODE_BUTTON_PIN 3

#define POSITION_MAX 1000
#define SEQUENCE_MEMORY_SIZE 1000

// Modes
#define IDLE 0
#define RECORDING 1
#define PLAYBACK 2

#define maxBrightnessMode 3
#define maxColorMode 1

// Parameters
#define STEP_DELAY 20
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

uint8_t prevModeButtonStatus = LOW;
uint8_t modeButtonStatus = LOW;

// Allocate memory
uint8_t sequenceMemory[SEQUENCE_MEMORY_SIZE] = {0};
uint8_t colorMap[NUMPIXELS][3];
uint8_t brightnessMap[NUMPIXELS];

// Initialize emtpy sequence
uint8_t *sequenceEnd = sequenceMemory;

uint8_t brightnessMode = 0;
uint8_t colorMode = 0;

int xDefPos;
int yDefPos;

void setup() {
	pinMode(X_PIN, INPUT);
	pinMode(Y_PIN, INPUT);
	pinMode(REC_BUTTON_PIN, INPUT);
	pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);

	xDefPos = analogRead(X_PIN);
	yDefPos = analogRead(Y_PIN);

	pixels.begin();
	Serial.begin(9600);
}

uint8_t getCurrentPin() {

	int xOrig = analogRead(X_PIN);
	int yOrig = analogRead(Y_PIN);

	int xAdj = xOrig - xDefPos;
	int yAdj = yOrig - yDefPos;

	int diff = abs(xAdj) + abs(yAdj);
	
	if(diff < 20) {
		//Serial.println("diff below threshold");
		return INT8_MAX;
	}

	float xSquare = (xOrig - xDefPos)/425.0;
	float ySquare = (yOrig - yDefPos)/425.0;

	ySquare = -ySquare;

	if(xSquare > 1.0) {
		xSquare = 1.0;
	}

	if(xSquare < -1.0) {
		xSquare = -1.0;
	}

	if(ySquare > 1.0) {
		ySquare = 1.0;
	}

	if(ySquare < -1.0) {
		ySquare = -1.0;
	}

	//ySquare = -ySquare;

	float xCircle = xSquare * sqrt(1.0 - 0.5*ySquare*ySquare);
	float yCircle = ySquare * sqrt(1.0 - 0.5*xSquare*xSquare);

	float param = (float)xCircle/(float)yCircle;
	float rad = atan(param);
	float deg = rad * (180.0 / PI);

	if(yCircle > 0.01) {
		deg = 90 - deg;
	}
	if(yCircle < -0.01) {
		deg = 270 - deg;
	}

	float segment = 360.0/(float)NUMPIXELS;
	uint8_t pin = (((uint8_t)(round(deg/segment))+3*(NUMPIXELS/4)+1) % NUMPIXELS);
	ardprintf("XOrig %d YOrig %d XAdj %d yAdj %d XSquare %f YSquare %f XCircle %f YCircle %f diff %d degree %f segmentWidth %f Pin %d", xOrig, yOrig, (xOrig - xDefPos), (yOrig - yDefPos), xSquare, ySquare, xCircle, yCircle, diff, deg, segment, pin);

	return pin;
}

void setColorMapBasedOnPin(uint8_t pin) {

	switch(colorMode) {
		case 0:
			//red
			for(uint8_t i=0; i<NUMPIXELS; ++i) {
				colorMap[i][0] = 1;
				colorMap[i][1] = 0;
				colorMap[i][2] = 0;
			}
			break;
		case 1:
			//red green blue in succession
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
		//adjust maxColorMode when adding modes
	}
}

uint8_t distanceToPin(uint8_t first, uint8_t second) {
	return min(
			min(abs((NUMPIXELS - first) + second),abs((NUMPIXELS - second) + first)),
			abs(second - first));
}

void checkModeButton() {
	modeButtonStatus = digitalRead(MODE_BUTTON_PIN);

	if(modeButtonStatus == LOW && prevModeButtonStatus == HIGH) {
		prevModeButtonStatus = LOW;
		uint8_t curPin = getCurrentPin();

		Serial.println("Current Pin");
		Serial.println(getCurrentPin());

		if(curPin >= 33 && curPin <= 39) { 
			// right - next brightness mode

			if(brightnessMode == maxBrightnessMode) {
				brightnessMode = 0;
			}
			else {
				brightnessMode += 1;
			}
		}
		if(curPin >= 9 && curPin <= 14) { 
			// left - prev brightness mode

			if(brightnessMode == 0) {
				brightnessMode = maxBrightnessMode;
			}
			else {
				brightnessMode -= 1;
			}
		}
		if(curPin >= 22 && curPin <= 27) { 
			// up - next color mode

			if(colorMode == maxColorMode) {
				colorMode = 0;
			}
			else {
				colorMode += 1;
			}
		}
		if(curPin >= 47 && curPin <= NUMPIXELS || curPin >= 0 && curPin <= 2) { 
			// down - prev color mode

			if(colorMode == 0) {
				colorMode = maxColorMode;
			}
			else {
				colorMode -= 1;
			}
		}
		return;
	}

	if(modeButtonStatus = HIGH) {
		prevModeButtonStatus = HIGH;
		return;
	}
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
			//one led full
			for(uint8_t i=0; i<NUMPIXELS; ++i) {
				if(i == pin) {
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
			//Decreasing brightness across circle
			for(uint8_t i=0; i<NUMPIXELS; ++i) {
				uint8_t d = distanceToPin(i, pin);
				uint8_t val = (uint8_t)(((float)d/(float)NUMPIXELS)*500.0);
				//ardprintf("i: %d, pin: %d, dist: %d, val: %d", i, pin, d, val);
				brightnessMap[i] = val;//(uint8_t)(((float)d/(float)NUMPIXELS)*250.0);
			}
			break;
		case 3:
			//every 10 leds full
			for(uint8_t i=0; i<NUMPIXELS; ++i) {
				if(i%10 == pin%10) {
					brightnessMap[i] = 250;
				}
				else {
					brightnessMap[i] = 0;
				}
			}
			break;
		//adjust maxBrightnessMode when adding modes
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

			checkModeButton();

			if(digitalRead(REC_BUTTON_PIN) == HIGH) {

				// wait until button released
				while(digitalRead(REC_BUTTON_PIN) == HIGH) {}

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

		if(digitalRead(REC_BUTTON_PIN) == LOW) {
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

			checkModeButton();

			if(digitalRead(REC_BUTTON_PIN) == HIGH) {
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
