/*
 PulseSensorBeatColor
Mods by Chris Jefferies, 12, 2012
Revamped original code from Pulse Sensor (see notes below)
This version uses a Fio, a 3.3V system, with xbee radios for 2 way communication. 

The circuit uses 2 RGB LEDs for the setup.  They are common anode with 1K resistors in line
with the cathodes of each RGB color.  This still needs some refinement for matching the brightness/color
by adjusting the resistors.

XBee settings (use X-TCU). They mirror each other and have complimentary settings for MY and DL.
XBee 1:
PAN ID: 1111
DH: 0
DL: 12
MY: 11 (radio 17)

XBee 2:
PAN ID: 1111
DH: 0
DL: 11
MY: 12 (radio 18)

*/


/*
>> Pulse Sensor Amped 1.1 <<
 This code is for Pulse Sensor Amped by Joel Murphy and Yury Gitman
 www.pulsesensor.com 
 >>> Pulse Sensor purple wire goes to Analog Pin 0 <<<
 Pulse Sensor sample aquisition and processing happens in the background via Timer 2 interrupt. 2mS sample rate.
 PWM on pins 3 and 11 will not work when using this code, because we are using Timer 2!
 The following variables are automatically updated:
 Signal :    int that holds the analog signal data straight from the sensor. updated every 2mS.
 IBI  :      int that holds the time interval between beats. 2mS resolution.
 BPM  :      int that holds the heart rate value, derived every beat, from averaging previous 10 IBI values.
 QS  :       boolean that is made true whenever Pulse is found and BPM is updated. User must reset.
 Pulse :     boolean that is true when a heartbeat is sensed then false in time with pin13 LED going out.
 
 This code is designed with output serial data to Processing sketch "PulseSensorAmped_Processing-xx"
 The Processing sketch is a simple data visualizer. 
 All the work to find the heartbeat and determine the heartrate happens in the code below.
 Pin 13 LED will blink with heartbeat.
 If you want to use pin 13 for something else, adjust the interrupt handler
 It will also fade an LED on pin fadePin with every beat. Put an LED and series resistor from fadePin to GND.
 Check here for detailed code walkthrough:
 http://pulsesensor.myshopify.com/pages/pulse-sensor-amped-arduino-v1dot1
 
 Code Version 02 by Joel Murphy & Yury Gitman  Fall 2012
 This update changes the HRV variable name to IBI, which stands for Inter-Beat Interval, for clarity.
 Switched the interrupt to Timer2.  500Hz sample rate, 2mS resolution IBI value.
 Fade LED pin moved to pin 5 (use of Timer2 disables PWM on pins 3 & 11).
 Tidied up inefficiencies since the last version. 
 */
 


#include<string.h>

// version setup
int majorVersion = 1;
int minorVersion = 1;
int buildVersion = 2;
String versString = "";
String thisVersion = versString + majorVersion + "." + minorVersion +"." + buildVersion;

//  Pulse Sensor variables
int pulsePin = 0;                 // Pulse Sensor purple wire connected to analog pin 0
int blinkPin = 13;                // pin to blink led at each beat
int fadePin = 5;                  // pin to do fancy classy fading blink at each beat
int fadeRate = 255;                 // de3fault = 0, used to fade LED on with PWM on fadePin

// RGB variables for remote LED
int grnLedPinRem = 3;  // Green LED, connected to digital pin 5
int bluLedPinRem = 4;  // Blue LED,  connected to digital pin 6
int redLedPinRem = 5;   // Red LED,   connected to digital pin 4

// RGB variables for local LED
int grnLedPinLoc = 6;  // Green LED, connected to digital pin 6
int bluLedPinLoc = 7;  // Blue LED,  connected to digital pin 7
int redLedPinLoc = 8;   // Red LED,   connected to digital pin 8

// Color arrays, RGB
int black[3]  = { 0, 0, 0 };
int white[3]  = { 80, 100, 100 };
int red[3]    = { 100, 0, 0 };
int green[3]  = { 0, 100, 0 };
int blue[3]   = { 0, 0, 100 };
int bluegreen[3]   = { 0, 100, 20 };
int dimwhite[3] = { 55, 55, 55 };
int purple[3] = { 80, 0, 80 };
int pink[3] = { 55, 0, 70 };
int orange[3] = { 55, 40, 0 };
int yellow[3] = { 100, 90, 0 };
  
// Color arrays, RGB - common anode
//int black[3]  = { 100, 100, 100 };
//int white[3]  = { 20, 0, 0 };
//int red[3]    = { 0, 100, 100 };
//int green[3]  = { 100, 0, 100 };
//int blue[3]   = { 100, 100, 0 };
//int bluegreen[3]   = { 100, 0, 80 };
//int dimwhite[3] = { 45, 45, 45 };
//int purple[3] = { 20, 100, 20 };
//int pink[3] = { 45, 100, 30 };
//int orange[3] = { 45, 60, 100 };
//int yellow[3] = { 0, 10, 100 };


const int terminatingChar = 13; // Terminate lines with CR
const int syncRange = 15; // the range of pulses within which we'll call it synchronized
int rBPM = 0; // remote beats per minute

// these variables are volatile because they are used during the interrupt service routine!
volatile int BPM;                   // used to hold the pulse rate
volatile int Signal;                // holds the incoming raw data
volatile int IBI = 600;             // holds the time between beats, the Inter-Beat Interval
volatile boolean Pulse = false;     // true when pulse wave is high, false when it's low
volatile boolean QS = false;        // becomes true when Arduoino finds a beat.

// ==================================================================
void setup(){
  pinMode(blinkPin,OUTPUT);         // pin that will blink to your heartbeat!
  pinMode(fadePin,OUTPUT);          // pin that will fade to your heartbeat!

  pinMode(redLedPinRem, OUTPUT); 
  pinMode(grnLedPinRem, OUTPUT);   
  pinMode(bluLedPinRem, OUTPUT); 

  pinMode(redLedPinLoc, OUTPUT); 
  pinMode(grnLedPinLoc, OUTPUT);   
  pinMode(bluLedPinLoc, OUTPUT); 

  Serial.begin(57600);             // we agree to talk fast!
  interruptSetup();                 // sets up to read Pulse Sensor signal every 2mS 
  // UN-COMMENT THE NEXT LINE IF YOU ARE POWERING The Pulse Sensor AT LOW VOLTAGE, 
  // AND APPLY THAT VOLTAGE TO THE A-REF PIN
  analogReference(EXTERNAL);


  // show version by LED
  // showVersionLED();
  Serial.print("started... BeatColor version: ");
  Serial.println(thisVersion);
  
  runTests();

}


// ==================================================================
void loop(){

    setLedColorRemote(red);
    delay(500);
    setLedColorRemote(green);
    delay(500);

    setLedColorLocal(red);
    delay(500);
    setLedColorLocal(blue);
    delay(500);
 
  // ledFadeToBeat();
  delay(20);                             //  take a break
}



// ==================================================================
/* various supporting methods */

// set the color of the remote RGB LED
void setRemoteLED(int beatValue){
  Serial.print("setRemoteLED: ");
  Serial.println(beatValue);

    setLedColorRemote(white);
    return;

  if (inSync(beatValue, rBPM)) {
        setLedColorRemote(white);
        return;
  }

  if (beatValue < 70) {
    setLedColorRemote(dimwhite);
    Serial.print("set: white, recd: ");
    Serial.println(beatValue);
  } 
  else if (beatValue >= 71 && beatValue <= 80) {
    setLedColorRemote(yellow);
    Serial.print("set: yellow, recd: ");
    Serial.println(beatValue);
  } 
  else if (beatValue >= 81 && beatValue <= 90) {
    setLedColorRemote(green);
    Serial.print("set: green, recd: ");
    Serial.println(beatValue);
  } 
  else if (beatValue >= 91 && beatValue <= 100) {
    setLedColorRemote(blue);
    Serial.print("set: blue, recd: ");
    Serial.println(beatValue);
  } 
  else if (beatValue >= 101 && beatValue <= 120) {
    setLedColorRemote(purple);
    Serial.print("set: purple, recd: ");
    Serial.println(beatValue);
  } 
  else if (beatValue >= 121 && beatValue <= 150) {
    setLedColorRemote(red);
    Serial.print("set: red, recd: ");
    Serial.println(beatValue);
  } 
  else {
    setLedColorRemote(white);
    Serial.print("set: white high, recd: ");
    Serial.println(beatValue);
  }
    delay(30);
    setLedColorLocal(black);
}

// set the color of the remote RGB LED
void setLocalLED(int bpm){
  int beatValue = bpm;
  
  Serial.print("setLocalLED: ");
  Serial.println(beatValue);

    setLedColorRemote(white);
    return;


    if (inSync(beatValue, BPM)) {
        setLedColorLocal(white);
        return;
  }

  if (beatValue < 70) {
    setLedColorLocal(dimwhite);
    // Serial.print("set: white, recd: ");
    // Serial.println(beatValue);
  } 
  else if (beatValue >= 71 && beatValue <= 80) {
    setLedColorLocal(yellow);
    // Serial.print("set: yellow, recd: ");
    // Serial.println(beatValue);
  } 
  else if (beatValue >= 81 && beatValue <= 90) {
    setLedColorLocal(green);
    // Serial.print("set: green, recd: ");
    // Serial.println(beatValue);
  } 
  else if (beatValue >= 91 && beatValue <= 100) {
    setLedColorLocal(blue);
    // Serial.print("set: blue, recd: ");
    // Serial.println(beatValue);
  } 
  else if (beatValue >= 101 && beatValue <= 120) {
    setLedColorLocal(purple);
    // Serial.print("set: purple, recd: ");
    // Serial.println(beatValue);
  } 
  else if (beatValue >= 121 && beatValue <= 150) {
    setLedColorLocal(red);
    // Serial.print("set: red, recd: ");
    // Serial.println(beatValue);
  } 
  else {
    setLedColorLocal(white);
    // Serial.print("set: white high, recd: ");
    // Serial.println(beatValue);
  }
    delay(30);
    setLedColorLocal(black);
}

boolean inSync(int bpm1, int bpm2){
  int minRange = min(bpm1,bpm2);
  int maxRange = max(bpm1,bpm2);
  if (maxRange - minRange <= syncRange){
    return true;
  }
  return false;
}

void ledFadeToBeat(){
  fadeRate -= 15;                         //  set LED fade value
  fadeRate = constrain(fadeRate,0,255);   //  keep LED fade value from going into negative numbers!
  analogWrite(fadePin,fadeRate);          //  fade LED
}

void sendDataToProcessing(char symbol, int data ){
  Serial.print(symbol);                // symbol prefix tells Processing what type of data is coming
  Serial.println(data);                // the data to send culminating in a carriage return
}

// send data to remote/mirror system
void sendDataToMirror(char symbol, int data ){
  // Serial.print("sending to mirror: "); // show what we're sending
  // Serial.println(data);                // the data to send culminating in a carriage return

  Serial.print(symbol);                // symbol prefix tells the mirror what type of data is coming
  Serial.println(data);                // the data to send culminating in a carriage return
}

void setLedColorRemote(int color[3]){
  // Convert to 0-255
  int R = (color[0] * 255) / 100;
  int G = (color[1] * 255) / 100;
  int B = (color[2] * 255) / 100;

  analogWrite(redLedPinRem, 255 - R);   // Write current values to LED pins
  analogWrite(grnLedPinRem, 255 - G);   // subtract from 255 for LED wired up as common anode
  analogWrite(bluLedPinRem, 255 - B);   // no subtraction for LED wired up as common cathode
  // Serial.print("color: ");  Serial.print(R); Serial.print(G); Serial.println(B);

}

void setLedColorLocal(int color[3]){
  // Convert to 0-255
  int R = (color[0] * 255) / 100;
  int G = (color[1] * 255) / 100;
  int B = (color[2] * 255) / 100;

  analogWrite(redLedPinLoc, 255 - R);   // Write current values to LED pins
  analogWrite(grnLedPinLoc, 255 - G);   // subtract from 255 for LED wired up as common anode
  analogWrite(bluLedPinLoc, 255 - B);   // no subtraction for LED wired up as common cathode
  // Serial.print("color: ");  Serial.print(R); Serial.print(G); Serial.println(B);

}


// ---------------------------------------------
// read serial port
char* serialReader(){
  static char serialReadString[50] = "";
  int index=0;
  int inByte = Serial.read();

  if (inByte > 0 && inByte != terminatingChar) { // If we see data (inByte > 0) and that data isn't a carriage return
    delay(50); //Allow serial data time to collect (I think. All I know is it doesn't work without this.)

    while (Serial.available() > 0 && inByte != terminatingChar && index <= 49){ // As long as EOL not found and there's more to read, keep reading
      serialReadString[index] = inByte; // Save the data in a character array
      index++; // increment position in array
      inByte = Serial.read(); // Read next byte
    }

    if (index >= 49){
      return "b"; // return this when the buffer is full
    }

    // If we terminated properly
    if (inByte == terminatingChar) {
      serialReadString[index] = 0; //Null terminate the serialReadString (Overwrites last position char (terminating char) with 0
      return serialReadString;
    }
  }
  return "n"; // return this when nothing is recieved...
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

// show the version through blinking LEDs
// red = major version
// green = minor version
// blue = buld number
void showVersionLED() {
  for (int i=0; i <= majorVersion-1; i++) {
    setLedColorLocal(red);
    delay(300);
    setLedColorLocal(black);
    delay(200);
  }
  for (int i=0; i <= minorVersion-1; i++) {
    setLedColorLocal(green);
    delay(300);
    setLedColorLocal(black);
    delay(200);
  }
  for (int i=0; i <= buildVersion-1; i++) {
    setLedColorLocal(blue);
    delay(300);
    setLedColorLocal(black);
    delay(200);
  }
  setLedColorLocal(white);
}

// spin the colors in the RGB LED
void spinLED() {
  for (int i=0; i <= 10; i++) {
    setLedColorLocal(red);
    delay(50);
    setLedColorLocal(purple);
    delay(50);
    setLedColorLocal(green);
    delay(50);
    setLedColorLocal(blue);
    delay(50);
  }
  setLedColorLocal(black);
}

void runTests(){
  setLedColorLocal(black);
  setLedColorRemote(black);
  delay(2000);

  setLedColorLocal(red);
  delay(1000);
  setLedColorLocal(green);
  delay(1000);
  setLedColorLocal(blue);
  delay(2000);
  setLedColorLocal(black);

  setLedColorRemote(red);
  delay(1000);
  setLedColorRemote(green);
  delay(1000);
  setLedColorRemote(blue);
  delay(2000);
  setLedColorRemote(black);
  delay(2000);
}



