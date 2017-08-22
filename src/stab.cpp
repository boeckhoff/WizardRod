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
#define REC_BUTTON_PIN 9
#define MODE_BUTTON_PIN 3

#define POSITION_MAX 1000
#define SEQUENCE_MEMORY_SIZE 1000

// Modes
#define IDLE 0
#define RECORDING 1
#define PLAYBACK 2

#define maxBrightnessMode 4
#define maxColorMode 5

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

uint8_t prevModeButtonStatus = LOW;
uint8_t modeButtonStatus = LOW;

// Allocate memory
uint8_t sequenceMemory[SEQUENCE_MEMORY_SIZE] = {0};
uint8_t colorMap[NUMPIXELS][3];
uint8_t brightnessMap[NUMPIXELS];

// Initialize emtpy sequence
uint8_t *sequenceEnd = sequenceMemory;

int brightnessMode = 0;
int colorMode = 0;

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

struct RGB
{
	unsigned char R;
	unsigned char G;
	unsigned char B;
};

struct HSL
{
	int H;
	float S;
	float L;
};

float HueToRGB(float v1, float v2, float vH)
{
	if (vH < 0)
		vH += 1;

	if (vH > 1)
		vH -= 1;

	if ((6 * vH) < 1)
		return (v1 + (v2 - v1) * 6 * vH);

	if ((2 * vH) < 1)
		return v2;

	if ((3 * vH) < 2)
		return (v1 + (v2 - v1) * ((2.0f / 3) - vH) * 6);

	return v1;
}

struct RGB HSLToRGB(struct HSL hsl) {
	struct RGB rgb;

	if (hsl.S == 0)
	{
		rgb.R = rgb.G = rgb.B = (unsigned char)(hsl.L * 255);
	}
	else
	{
		float v1, v2;
		float hue = (float)hsl.H / 360;

		v2 = (hsl.L < 0.5) ? (hsl.L * (1 + hsl.S)) : ((hsl.L + hsl.S) - (hsl.L * hsl.S));
		v1 = 2 * hsl.L - v2;

		rgb.R = (unsigned char)(255 * HueToRGB(v1, v2, hue + (1.0f / 3)));
		rgb.G = (unsigned char)(255 * HueToRGB(v1, v2, hue));
		rgb.B = (unsigned char)(255 * HueToRGB(v1, v2, hue - (1.0f / 3)));
	}

	return rgb;
}

uint8_t getCurrentPin(bool checkTreshold) {

	int xOrig = analogRead(X_PIN);
	int yOrig = analogRead(Y_PIN);

	int xPosition = xOrig - 420;
	int yPosition = yOrig - 420;

	int diff = abs(xOrig - xDefPos) + abs(yOrig - yDefPos);
	
	if(diff < 200 && checkTreshold) {
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

uint8_t distanceToPin(uint8_t first, uint8_t second) {
	return min(
			min(abs((NUMPIXELS - first) + second),abs((NUMPIXELS - second) + first)),
			abs(second - first));
}

uint8_t circleDistanceToPin(uint8_t first, uint8_t second) {
	if(second > first) {
		return(second - first);
	}
	else {
		return(NUMPIXELS - 1 - first + second);
	}
}

uint8_t pinWithOffset(uint8_t pin, uint8_t offset) {
	if(pin + offset < (NUMPIXELS - 1)) {
		return(pin + offset);
	}
	else {
		uint8_t overlap = ((NUMPIXELS - 1) - pin);
		return (offset - overlap);
	}
}

void setColorMapBasedOnPin(uint8_t pin) {

	switch(colorMode) {
		case 0:
			//red
			for(uint8_t i=0; i<NUMPIXELS; ++i) {
				colorMap[i][0] = 1.0;
				colorMap[i][1] = 0.0;
				colorMap[i][2] = 0.0;
			}
			break;
		case 1:

			// blue and red
			for(uint8_t i=0; i<NUMPIXELS; ++i) {
				if(i%2 == 0) {
					colorMap[i][0] = 1.0;
					colorMap[i][1] = 0.0;
					colorMap[i][2] = 0.0;
				}
				else {
					colorMap[i][0] = 0.0;
					colorMap[i][1] = 0.0;
					colorMap[i][2] = 1.0;
				}
			}
			break;
		case 2:
			
			// HSL color ring
			for(uint8_t i=0; i<NUMPIXELS; ++i) {
				float H = ((float)i/(NUMPIXELS-1))*360.0;
				struct HSL data = {H, 1.0, 0.5};
				struct RGB value = HSLToRGB(data);
				colorMap[i][0] = (uint8_t)value.R/255;
				colorMap[i][1] = (uint8_t)value.G/255;
				colorMap[i][2] = (uint8_t)value.B/255;
			}
			break;

		case 3:
			// 3 moving color areas
			for(uint8_t i=0; i<NUMPIXELS; ++i) {
				uint8_t dis = circleDistanceToPin(i, pin);
				if(dis >= NUMPIXELS/6 && dis < 3*(NUMPIXELS/6)) {
					colorMap[i][0] = 1.0;
					colorMap[i][1] = 0.0;
					colorMap[i][2] = 0.0;
				}
				if(dis >= 3*(NUMPIXELS/6) && dis < 5*(NUMPIXELS/6)) {
					colorMap[i][0] = 0.0;
					colorMap[i][1] = 1.0;
					colorMap[i][2] = 0.0;
				}
				if((dis >= 5*(NUMPIXELS/6) && dis < NUMPIXELS) || dis < NUMPIXELS/6) {
					colorMap[i][0] = 0.0;
					colorMap[i][1] = 0.0;
					colorMap[i][2] = 1.0;
				}
			}
			break;

		case 4:
			//red green blue in succession
			for(uint8_t i=0; i<NUMPIXELS; ++i) {
				uint8_t diff = pin%3;
				switch(diff) {
					case 0:
						colorMap[i][0] = 1.0;
						colorMap[i][1] = 0.0;
						colorMap[i][2] = 0.0;
						break;
					case 1:
						colorMap[i][0] = 0.0;
						colorMap[i][1] = 1.0;
						colorMap[i][2] = 0.0;
						break;
					case 2:
						colorMap[i][0] = 0.0;
						colorMap[i][1] = 0.0;
						colorMap[i][2] = 1.0;
						break;
				}
			}
			break;

		case 5:
			//red*2 green*2 blue*2 static
			for(uint8_t i=0; i<NUMPIXELS; ++i) {
				uint8_t diff = i%6;
				switch(diff) {
					case 0:
					case 1:
						colorMap[i][0] = 1.0;
						colorMap[i][1] = 0.0;
						colorMap[i][2] = 0.0;
						break;
					case 2:
					case 3:
						colorMap[i][0] = 0.0;
						colorMap[i][1] = 1.0;
						colorMap[i][2] = 0.0;
						break;
					case 4:
					case 5:
						colorMap[i][0] = 0.0;
						colorMap[i][1] = 0.0;
						colorMap[i][2] = 1.0;
						break;
				}
			}
			break;
		//adjust maxColorMode when adding modes and don't forget break..
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
			// 1 heap of brightness
			for(uint8_t i=0; i<NUMPIXELS; ++i) {

				uint16_t penalty = distanceToPin(i, pin)*30;

				if(penalty > 250) {
					brightnessMap[i] = 0;
				}
				else {
					brightnessMap[i] = (uint8_t)(0.5*(250 - penalty));
				}
			}
			break;
		case 2:
			// 3 heaps of brightness
			for(uint8_t i=0; i<NUMPIXELS; ++i) {
				
				uint8_t dis = min(min(
							distanceToPin(i, pin), 
							distanceToPin(i, pinWithOffset(pin, NUMPIXELS/3))),
							distanceToPin(i, pinWithOffset(pin, 2*(NUMPIXELS/3))));
							
				uint16_t penalty = dis*50;

				if(penalty > 250) {
					brightnessMap[i] = 0;
				}
				else {
					brightnessMap[i] = (uint8_t)(0.5*(250 - penalty));
				}
			}
			break;
		case 3:
			// every 10 leds full
			for(uint8_t i=0; i<NUMPIXELS; ++i) {
				if(i%10 == pin%10) {
					brightnessMap[i] = 200;
				}
				else {
					brightnessMap[i] = 0;
				}
			}
			break;
		case 4:
			// decreasing brightness across circle
			for(uint8_t i=0; i<NUMPIXELS; ++i) {
				uint8_t d = distanceToPin(i, pin);
				uint8_t val = (uint8_t)(((float)d/(float)NUMPIXELS)*180.0);
				//ardprintf("i: %d, pin: %d, dist: %d, val: %d", i, pin, d, val);
				brightnessMap[i] = val;//(uint8_t)(((float)d/(float)NUMPIXELS)*250.0);
			}
			break;

		//adjust maxBrightnessMode when adding modes
	}
}


void switchMode(uint8_t curPin) {

	if(curPin >= 31 && curPin <= 41) { 
		// right - next brightness mode
		Serial.println("right - next brightness mode");

		if(brightnessMode == maxBrightnessMode) {
			brightnessMode = 0;
		}
		else {
			brightnessMode += 1;
		}
	}
	if(curPin >= 7 && curPin <= 16) { 
		// left - prev brightness mode
		Serial.println("left - prev brightness mode");

		if(brightnessMode == 0) {
			brightnessMode = maxBrightnessMode;
		}
		else {
			brightnessMode -= 1;
		}
	}
	if(curPin >= 20 && curPin <= 29) { 
		// up - next color mode
		Serial.println("up - next color mode");

		if(colorMode == maxColorMode) {
			colorMode = 0;
		}
		else {
			colorMode += 1;
		}
	}
	if((curPin >= 45 && curPin <= NUMPIXELS) || (curPin >= 0 && curPin <= 4)) { 
		// down - prev color mode
		Serial.println("down - prev color mode");

		if(colorMode == 0) {
			colorMode = maxColorMode;
		}
		else {
			colorMode -= 1;
		}
	}
}

void checkModeButton() {
	modeButtonStatus = digitalRead(MODE_BUTTON_PIN);

	if(modeButtonStatus == LOW && prevModeButtonStatus == HIGH) {
		prevModeButtonStatus = LOW;
		switchMode(getCurrentPin(true));

		return;
	}

	if(modeButtonStatus == HIGH) {
		prevModeButtonStatus = HIGH;
		return;
	}
}


void setPinOneStep(uint8_t pin) {
	//ardprintf("Setting pin %d for duration %d", pin, steps);
	setColorMapBasedOnPin(pin);
	setBrightnessMapBasedOnPin(pin);

	for(int i=0; i<NUMPIXELS; ++i) {
		pixels.setPixelColor(i, pixels.Color((uint8_t)(colorMap[i][0]*brightnessMap[i]),
					(uint8_t)(colorMap[i][1]*brightnessMap[i]),
					(uint8_t)(colorMap[i][2]*brightnessMap[i])));
	}
	pixels.show();

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

		uint8_t curPin = getCurrentPin(true);
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
			pin = getCurrentPin(true);
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
