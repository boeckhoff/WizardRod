/* 
   This code should be pasted within the files where this function is needed.
   This function will not create any code conflicts.

   The function call is similar to printf: ardprintf("Test %d %s", 25, "string");

   To print the '%' character, use '%%'

   This code was first posted on http://arduino.stackexchange.com/a/201
   */
/*
#ifndef ARDPRINTF
#define ARDPRINTF
#define ARDBUFFER 16 //Buffer for storing intermediate strings. Performance may vary depending on size.
#include <stdarg.h>
#include <Arduino.h> //To allow function to run from any file in a project

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
*/

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif


#include <LinkedList.h>
#include <math.h>

const int X_PIN = A3;
const int Y_PIN = A5;
const int BUTTON_PIN = 9;
const int POSITION_MAX = 1000;

#define PI 3.14159265;

//Led Strip values
#define DATA_PIN   5
#define NUMPIXELS  50
#define STEP_DELAY 30
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, DATA_PIN, NEO_GRB + NEO_KHZ800);

int prevPin = 0;
int newPin = 0; 
int newPinNum = 0;

int justPlayed = 0;

int prevButtonState = LOW;
int newButtonState = LOW;

int xPosition = 0;
int yPosition = 0;

LinkedList<int> sequence = LinkedList<int>();

void setup() {

  pinMode(X_PIN, INPUT);
  pinMode(Y_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT);
  pixels.begin(); // This initializes the NeoPixel library.
  Serial.begin(9600);
}

int getPinForPostition(int xPosition, int yPosition) {
  int xOrig, yOrig;
  xOrig = xPosition;
  yOrig = yPosition;
  xPosition -= 512;
  yPosition -= 512;
  double param, at, atOrig;
  int pin;
  param = (float)xPosition/(float)yPosition;
  at = atan(param) * 180 / PI;
  atOrig = at;

  if(yPosition > 0) {
    at = 90 - at;
  }
  if(xPosition < 0 && yPosition < 0) {
    at = 270 - at;
  }
  if(xPosition > 0 && yPosition < 0) {
    at = 270 - at;
  }

  pin = (int)at/7;
  //if(pin != prevPin)
    //ardprintf("XPos %d YPos %d XPosAdj %d YPosAdj %d AtanOrig %f AtanAdj %f Pin %d", xOrig, yOrig, xPosition, yPosition, atOrig, at, pin);
  return pin;
}

void playSequence() {

	while(true) {
		int i = 0;
		while(i < sequence.size()) {
			if(digitalRead(BUTTON_PIN) == HIGH) {
				Serial.println("Button canceled playSequence");
				sequence.clear();
				justPlayed = 1;
				return;
			}

			newPin = sequence.get(i);
			newPinNum = sequence.get(i+1);
			
			pixels.setPixelColor(prevPin, pixels.Color(0,0,0));
			pixels.setPixelColor(newPin, pixels.Color(0,150,0));
			pixels.show();

			Serial.println("newPIN playback");
			Serial.println(newPin);

			for(int j = 0; j < newPinNum; j++) {
				delay(STEP_DELAY);
			}

			prevPin = newPin;

			i += 2;
		}
	}
}

void recordSequence(int newPin) {

	if(newPin == prevPin && sequence.size() != 0) {
		int prevRecordedNum = sequence.get(sequence.size() - 1);
		sequence.set(sequence.size() - 1, prevRecordedNum + 1);
	}
	else {
		sequence.add(newPin);
		sequence.add(1);
		Serial.println("newPIN recording");
		Serial.println(newPin);
	}
}

void loop() {

	xPosition = analogRead(X_PIN);
	yPosition = analogRead(Y_PIN);

	newPin = getPinForPostition(xPosition, yPosition);

	if (prevPin != newPin) {
		pixels.setPixelColor(prevPin, pixels.Color(0,0,0));
		pixels.setPixelColor(newPin, pixels.Color(0,150,0));
		pixels.show();
		Serial.println("newPin live");
		Serial.println(newPin);
	}
	pixels.setPixelColor(newPin, pixels.Color(0,150,0));

	newButtonState = digitalRead(BUTTON_PIN);

	if (newButtonState == LOW && justPlayed == 1) {
		prevButtonState = LOW;
		justPlayed = 0;
		Serial.println("Button LOW and justPlayed 1");
	}

	if (newButtonState == HIGH && justPlayed != 1) {
		Serial.println("Button HIGH and justPlayed 0. Will record");
		recordSequence(getPinForPostition(xPosition, yPosition));
	}

	if (newButtonState == LOW && prevButtonState == HIGH) {
		Serial.println("Button LOW and prevButton HIGH. will play sequence");
		playSequence();
	}

	prevButtonState = newButtonState;
	prevPin = newPin;

	delay(STEP_DELAY);
}
