#include "LPD8806.h"
#include "SPI.h"

// Code for simple pulse sensing 
// reflected in pulsing LEDs that change color with the BPM
// FIO V3 from SparkFun, Pulse Sensor Amped 1.1, Digital LED strip from Adafruit (as used on the LED belt)
// mashing code from Adafruit's LED belt and Pulse Sensor Amped 1.1
/*****************************************************************************/

// for led strip
# if defined(USB_SERIAL) || defined(USB_SERIAL_ADAFRUIT)
// this is for teensyduino support
int dataPin = 2;
int clockPin = 1;
#else 
// these are the pins we use for the LED belt kit using the Leonardo pinouts
int dataPin = 16;
int clockPin = 15;
#endif

// Set the first variable to the NUMBER of pixels. 32 = 32 pixels in a row
// The LED strips are 32 LEDs per meter but you can extend/cut the strip
// LPD8806 strip = LPD8806(32, dataPin, clockPin);
LPD8806 strip = LPD8806(32, dataPin, clockPin);

//  Pulse VARIABLES
int pulsePin = 0;                 // Pulse Sensor purple wire connected to analog pin 0
int blinkPin = 13;                // pin to blink led at each beat

// Pulse - these variables are volatile because they are used during the interrupt service routine!
volatile int BPM;                   // used to hold the pulse rate
volatile int Signal;                // holds the incoming raw data
volatile int IBI = 600;             // holds the time between beats, the Inter-Beat Interval
volatile boolean Pulse = false;     // true when pulse wave is high, false when it's low
volatile boolean QS = false;        // becomes true when Arduoino finds a beat.

// sometimes required as prototype methods
void startUpTest();
void interruptSetup();
uint32_t setColor(int bpm);

// Pulse rate timing
int ledState = LOW;             // ledState used to set the LED
long previousMillis = 0;        // will store last time LED was updated

int pulseRateMin = 60;
int pulseRateMax = 120;
int pulseRangePercent = 10;
boolean checkMirrorBPM = true;

const int terminatingChar = 13;
int rBPM = 0; // remote beats per minute


/********* SETUP ********************************************************************/
void setup() {

  pinMode(blinkPin,OUTPUT);         // pin that will blink to your heartbeat!

  Serial.begin(57600);
  Serial1.begin(9600);

  interruptSetup();                 // sets up to read Pulse Sensor signal every 2mS 

  // for 3.3V systems/FIO
  analogReference(EXTERNAL);

  // Start up the LED strip
  strip.begin();

  // startUpTest();

  // Update the strip, to start they are all 'off'
  strip.show();
}

// function prototypes, do not remove these!
void colorChase(uint32_t c, uint8_t wait);
void pulseWipe(uint32_t c, uint8_t wait);
void colorWipe(uint32_t c, uint8_t wait);
void dither(uint32_t c, uint8_t wait);
void scanner(uint8_t r, uint8_t g, uint8_t b, uint8_t wait);
void wave(uint32_t c, int cycles, uint8_t wait);
void rainbowCycle(uint8_t wait);
uint32_t Wheel(uint16_t WheelPos);
void sendToXbee(int BPM);
void sendDataToMirror(int data );
void serialReader();
int stringToInt(String thisString);


/********* LOOP ********************************************************************/
void loop() {

  serialReader();

  if (QS == true){
    Serial.print("BPM: "); 
    Serial.print(BPM); 
    Serial.print(" - rBPM: "); 
    Serial.println(rBPM); 
 
    sendDataToMirror(BPM);

    if (Pulse == true){
      if (rBPM >= BPM-5 && rBPM <= BPM+5 && checkMirrorBPM) {
        // set up the sync pattern
        // rainbowCycle(0);
        wave(strip.Color(127,0,0), 1, 10); // red/white

      }
      else {
        // set up the unique pattern
        uint32_t stripColor = setColor(BPM);
        pulseWipe(stripColor, 0);
      }
    }
    QS = false;
  }
  
  delay(20);

  // Clear strip data before start of next effect
  for (int i=0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, 0);
  }
  
}


/*********** METHODS ******************************************************************/

// set colors based on pulse rate
uint32_t setColor(int bpm) {

  int range = map(bpm, pulseRateMin, pulseRateMax, 0, 8);

  switch (range) {
  case 0:
    return strip.Color(0,0,127);     // blue
    break;
  case 1:
    return strip.Color(0,127,127);   // cyan
    break;
  case 2:
    return strip.Color(0,127,0);     // green
    break;
  case 3:
    return strip.Color(127,127,0);   // yellow
    break;
  case 4:
    return strip.Color(127,60,0);     // orange
    break;
  case 5:
    return strip.Color(127,0,0);     // red
    break;
  case 6:
    return strip.Color(127,0,127);     // magenta
    break;
  case 7:
    return strip.Color(127,0,100);     // magenta, kinda
    break;
  case 8:
    return strip.Color(127,60,60);     // ???
    break;
  default: 
    // if nothing else matches, do the default
    return strip.Color(40,40,40);
  }
}

// send data to remote/mirror system
void sendDataToMirror(int data ) {
  Serial1.println(data);                // the data to send culminating in a carriage return
}


// Wipe the LEDs with each pulse
void pulseWipe(uint32_t c, uint8_t wait) {
  int i;

  // run up the strip quickly; delay = 0
  for (i=0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(0);
  }
  
  // marker LED for testing...
  strip.setPixelColor(0, strip.Color(30,30,30));
  strip.show();
  delay(0);

  // blank the LEDs in reverse order, but do it slower than the pulse
  // to indicate a slower decay
  for (i=strip.numPixels(); i >= 0; i=i-1) {
    strip.setPixelColor(i, 0); // clear pixel
    strip.show();
    delay(30);
  }
}


// ---------------------------------------------
// read serial port
void serialReader(){
  int makeSerialStringPosition;
  int inByte;
  char serialReadString[50];
  const int terminatingChar = 13; //Terminate lines with CR

  inByte = Serial1.read();
  makeSerialStringPosition=0;

  if (inByte > 0 && inByte != terminatingChar) { //If we see data (inByte > 0) and that data isn't a carriage return
    delay(100); //Allow serial data time to collect (I think. All I know is it doesn't work without this.)

    while (inByte != terminatingChar && Serial1.available() > 0){ // As long as EOL not found and there's more to read, keep reading
      serialReadString[makeSerialStringPosition] = inByte; // Save the data in a character array
      makeSerialStringPosition++; //Increment position in array
      //if (inByte > 0) Serial.println(inByte); // Debug line that prints the charcodes one per line for everything recieved over serial
      inByte = Serial1.read(); // Read next byte
    }

    if (inByte == terminatingChar) //If we terminated properly
    {
      serialReadString[makeSerialStringPosition] = 0; //Null terminate the serialReadString (Overwrites last position char (terminating char) with 0

      String strRecd(serialReadString);

      // Serial.print("recd String: ");
      // Serial.println(strRecd);
      
      rBPM = stringToInt(strRecd);
      // Serial.print("recd int: ");
      // Serial.println(rBPM);

      // return serialReadString;
      // if (strcmp(serialReadString, "LEDOn") == 0) digitalWrite(13, HIGH);
      // if (strcmp(serialReadString, "LEDOff") == 0) digitalWrite(13, LOW);
      
    }
  } 
}


// ---------------------------------------------
// convert string to int
int stringToInt(String thisString) {
  int i, value, length;
  length = thisString.length();
  char blah[(length+1)];
  for(i=0; i<length; i++) {
    blah[i] = thisString.charAt(i);
  }
  blah[i]=0;
  value = atoi(blah);
  return value;
}



/*********** MISC METHODS ******************************************************************/

// Cycle through the color wheel, equally spaced around the belt
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for (j=0; j < 384 * 5; j++) {     // 5 cycles of all 384 colors in the wheel
    for (i=0; i < strip.numPixels(); i++) {
      // tricky math! we use each pixel as a fraction of the full 384-color
      // wheel (thats the i / strip.numPixels() part)
      // Then add in j which makes the colors go around per pixel
      // the % 384 is to make the wheel cycle around
      strip.setPixelColor(i, Wheel(((i * 384 / strip.numPixels()) + j) % 384));
    }
    strip.show();   // write all the pixels out
    delay(wait);
  }
}

// fill the dots one after the other with said color
// good for testing purposes
void colorWipe(uint32_t c, uint8_t wait) {
  int i;

  for (i=0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

// Chase a dot down the strip
// good for testing purposes
void colorChase(uint32_t c, uint8_t wait) {
  int i;

  for (i=0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, 0);  // turn all pixels off
  }

  for (i=0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, c); // set one pixel
    strip.show();              // refresh strip display
    delay(wait);               // hold image for a moment
    strip.setPixelColor(i, 0); // erase pixel (but don't refresh yet)
  }
  strip.show(); // for last erased pixel
}

// An "ordered dither" fills every pixel in a sequence that looks
// sparkly and almost random, but actually follows a specific order.
void dither(uint32_t c, uint8_t wait) {

  // Determine highest bit needed to represent pixel index
  int hiBit = 0;
  int n = strip.numPixels() - 1;
  for(int bit=1; bit < 0x8000; bit <<= 1) {
    if(n & bit) hiBit = bit;
  }

  int bit, reverse;
  for(int i=0; i<(hiBit << 1); i++) {
    // Reverse the bits in i to create ordered dither:
    reverse = 0;
    for(bit=1; bit <= hiBit; bit <<= 1) {
      reverse <<= 1;
      if(i & bit) reverse |= 1;
    }
    strip.setPixelColor(reverse, c);
    strip.show();
    delay(wait);
  }
  delay(250); // Hold image for 1/4 sec
}

// "Larson scanner" = Cylon/KITT bouncing light effect
void scanner(uint8_t r, uint8_t g, uint8_t b, uint8_t wait) {
  int i, j, pos, dir;

  pos = 0;
  dir = 1;

  for(i=0; i<((strip.numPixels()-1) * 8); i++) {
    // Draw 5 pixels centered on pos.  setPixelColor() will clip
    // any pixels off the ends of the strip, no worries there.
    // we'll make the colors dimmer at the edges for a nice pulse
    // look
    strip.setPixelColor(pos - 2, strip.Color(r/4, g/4, b/4));
    strip.setPixelColor(pos - 1, strip.Color(r/2, g/2, b/2));
    strip.setPixelColor(pos, strip.Color(r, g, b));
    strip.setPixelColor(pos + 1, strip.Color(r/2, g/2, b/2));
    strip.setPixelColor(pos + 2, strip.Color(r/4, g/4, b/4));

    strip.show();
    delay(wait);
    // If we wanted to be sneaky we could erase just the tail end
    // pixel, but it's much easier just to erase the whole thing
    // and draw a new one next time.
    for(j=-2; j<= 2; j++) 
      strip.setPixelColor(pos+j, strip.Color(0,0,0));
    // Bounce off ends of strip
    pos += dir;
    if(pos < 0) {
      pos = 1;
      dir = -dir;
    } 
    else if(pos >= strip.numPixels()) {
      pos = strip.numPixels() - 2;
      dir = -dir;
    }
  }
}

// Sine wave effect
#define PI 3.14159265
void wave(uint32_t c, int cycles, uint8_t wait) {
  float y;
  byte  r, g, b, r2, g2, b2;

  // Need to decompose color into its r, g, b elements
  g = (c >> 16) & 0x7f;
  r = (c >>  8) & 0x7f;
  b =  c        & 0x7f; 

  for(int x=0; x<(strip.numPixels()*5); x++)
  {
    for(int i=0; i<strip.numPixels(); i++) {
      y = sin(PI * (float)cycles * (float)(x + i) / (float)strip.numPixels());
      if(y >= 0.0) {
        // Peaks of sine wave are white
        y  = 1.0 - y; // Translate Y to 0.0 (top) to 1.0 (center)
        r2 = 127 - (byte)((float)(127 - r) * y);
        g2 = 127 - (byte)((float)(127 - g) * y);
        b2 = 127 - (byte)((float)(127 - b) * y);
      } 
      else {
        // Troughs of sine wave are black
        y += 1.0; // Translate Y to 0.0 (bottom) to 1.0 (center)
        r2 = (byte)((float)r * y);
        g2 = (byte)((float)g * y);
        b2 = (byte)((float)b * y);
      }
      strip.setPixelColor(i, r2, g2, b2);
    }
    strip.show();
    delay(wait);
  }
}

/* Helper functions */

//Input a value 0 to 384 to get a color value.
//The colours are a transition r - g - b - back to r

uint32_t Wheel(uint16_t WheelPos)
{
  byte r, g, b;
  switch(WheelPos / 128)
  {
  case 0:
    r = 127 - WheelPos % 128; // red down
    g = WheelPos % 128;       // green up
    b = 0;                    // blue off
    break;
  case 1:
    g = 127 - WheelPos % 128; // green down
    b = WheelPos % 128;       // blue up
    r = 0;                    // red off
    break;
  case 2:
    b = 127 - WheelPos % 128; // blue down
    r = WheelPos % 128;       // red up
    g = 0;                    // green off
    break;
  }
  return(strip.Color(r,g,b));
}

void startUpTest() {
  strip.setPixelColor(16, strip.Color(127,0,0));
  strip.show();
  delay(50);
  strip.setPixelColor(16, strip.Color(0,127,0));
  strip.show();
  delay(50);
  strip.setPixelColor(16, strip.Color(0,0,127));
  strip.show();
  delay(50);
  strip.setPixelColor(16, strip.Color(0,0,0));
  strip.show();
  delay(50);
}

void doPulse(int setPulse){
  if (setPulse == 1){
    uint32_t stripColor = setColor(BPM);
    pulseWipe(stripColor, 0);
  }
  else
  {
    uint32_t stripColor = strip.Color(0,0,0);
    pulseWipe(stripColor, 0);

  }
}




// Send a simple pixel chase in...
// colorChase(strip.Color(127,127,127), 20); // white
// colorChase(strip.Color(127,0,0), 20);     // red
// colorChase(strip.Color(127,127,0), 20);   // yellow
// colorChase(strip.Color(0,127,0), 20);     // green
// colorChase(strip.Color(0,127,127), 20);   // cyan
// colorChase(strip.Color(0,0,127), 20);     // blue
// colorChase(strip.Color(127,0,127), 20);   // magenta

// Fill the entire strip with...
// colorWipe(strip.Color(127,0,0), 20);      // red
// colorWipe(strip.Color(0, 127,0), 20);     // green
// colorWipe(strip.Color(0,0,127), 20);      // blue
// colorWipe(strip.Color(0,0,0), 20);        // black

// Color sparkles
// dither(strip.Color(0,127,127), 50);       // cyan, slow
// dither(strip.Color(0,0,0), 15);           // black, fast
// dither(strip.Color(127,0,127), 50);       // magenta, slow
// dither(strip.Color(0,0,0), 15);           // black, fast
// dither(strip.Color(127,127,0), 50);       // yellow, slow
// dither(strip.Color(0,0,0), 15);           // black, fast

// Back-and-forth lights
// scanner(127,0,0, 30);        // red, slow
// scanner(0,0,127, 15);        // blue, fast

// Wavy ripple effects
// wave(strip.Color(127,0,0), 4, 20);        // candy cane
// wave(strip.Color(0,0,100), 2, 40);        // icy

// make a pretty rainbow cycle!
// rainbowCycle(3);  // make it go through the cycle fairly fast



