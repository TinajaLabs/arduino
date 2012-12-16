// PulseSensor2Way
// the code is a preliminary test to hooking up the PulseSensor
// it shows how to hook up a switch to a local input
// turn on a local LED
// send the local signal to the other Fio 
// blink the remote Fio LED

// code was derived from the PulseSensorAmped_Arduino_01.ino, pulsesensor.com


// XBee settings (use X-TCU)
// XBee 1:
// PAN ID: 1111
// DH: 0
// DL: 12
// MY: 11 (=radio #17)

// XBee 2:
// PAN ID: 1111
// DH: 0
// DL: 11
// MY: 12 (=radio #18)


// pulse sensor variables
// int pulsePin = 0;          // pulse sensor purple wire connected to analog pin 0
int fadeRate = 0;          // used to fade LED on PWM pin 11
                                    // these are volatile because they are used during the interrupt!
volatile int BPM;                   // used to hold the pulse rate
volatile int Signal;                // holds the incoming raw data
volatile int HRV;                   // holds the time between beats
volatile boolean Pulse = false;     // true when pulse wave is high, false when it's low
volatile boolean QS = false;        // becomes true when pulse rate is determined. every 20 pulses


int pulsePin = 2; // button with 10k to gnd, this simulates the pulse from the pulse sensor
int localHbLed = 13; // LED to gnd
int remoteHbLed = 12; // LED to gnd

int index = 0;
int pulseState = 0;
int lastState = 0;

const int terminatingChar = 13; // Terminate lines with CR


// ---------------------------------------------
void setup(){

  pinMode(localHbLed,OUTPUT);    
  pinMode(remoteHbLed,OUTPUT);
  pinMode(pulsePin, INPUT);  

  Serial.begin(57600);
  Serial.print(255);  
  
  interruptSetup();      // sets up to read Pulse Sensor signal every 1mS 

  analogReference(EXTERNAL); // set up for 3.3v usage

  delay(2000);
  blink(3); // show we're on...
}

// ---------------------------------------------
void loop(){
  
  // sendDataToProcessing('S', Signal);   // send Processing the raw Pulse Sensor data

 if (QS == true){                     // Quantified Self flag is true when arduino finds a heartbeat
    fadeRate = 255;                    // Set 'fadeRate' Variable to 255 to fade LED with pulse
    // sendDataToProcessing('B',BPM);     // send the time between beats with a 'B' prefix
    // sendDataToProcessing('Q',HRV);     // send heart rate with a 'Q' prefix
    QS = false;                        // reset the Quantified Self flag for next time    
   }


  pulseState = digitalRead(pulsePin);

  if (lastState != pulseState) {
    if (pulseState == HIGH) {     
      // turn LED on:    
      digitalWrite(localHbLed, HIGH);
      sendDataToOther("pulseUp");
      lastState = HIGH;
    }

    if (pulseState == LOW) {
      // turn LED off:
      digitalWrite(localHbLed, LOW); 
      sendDataToOther("pulseDn");
      lastState = LOW;
    }
  }

  getDataFromOther();
  delay(50);    //  take a break
}


// ---------------------------------------------
void sendDataToOther(String pulseVal){
  // send info to the other xbee
  if (pulseVal == "pulseUp"){
    Serial.print("pulseUp\r");
  }
  else {
    Serial.print("pulseDn\r");
  }
}


// ---------------------------------------------
void getDataFromOther() {

  char* theOtherPulse = serialReader();

  if (strcmp(theOtherPulse, "pulseUp") == 0) {
    digitalWrite(remoteHbLed,HIGH);
    Serial.println("recd... HIGH ");
  }

  if (strcmp(theOtherPulse, "pulseDn") == 0) {
    digitalWrite(remoteHbLed,LOW);
    Serial.println("recd... LOW ");
  }
}


// ---------------------------------------------
// read serial port
char* serialReader(){
  static char serialReadString[50] = "";
  index=0;
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

// ---------------------------------------------
// blink n times 
void blink(int howManyTimes){
  digitalWrite(localHbLed, LOW);
  digitalWrite(remoteHbLed, LOW);
  delay(200);
  for (int i=0; i< howManyTimes; i++){
    digitalWrite(localHbLed, HIGH);
    digitalWrite(remoteHbLed, LOW);
    delay(200);
    digitalWrite(localHbLed, LOW);
    digitalWrite(remoteHbLed, HIGH);
    delay(200);
  }
  digitalWrite(remoteHbLed, LOW);
}






