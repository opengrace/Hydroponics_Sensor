#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include <Time.h>

#define RF_SETUP 0x17

RF24 radio(9,10);
const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0x7365727631LL };
char receivePayload[32];
uint8_t counter=0;


const int lightPin = 7; //D7
const int pumpPin = 6; //D6
const int errorLED = 8; //D8
//const int blinkLED = 0; //A7
const int eTapeRef = 15; //A1
const int eTape = A0; //A0
const int pumpSwitch = 19; //A6 Using the light switch due to board error A6/A7 can't be used as digital pins.
const int lightSwitch = 19; //A5
const int lowWater = 18;//A4
const int tankOne = 16;//A2
const int tankTwo = 17;//A3

const int maxPumpRun = 32000;
unsigned long pumpStartTime;
unsigned long lastsync;

const float minTape = 761;
const float maxTape = 118.92;

int pumpState = 0;
int lightState = 0;
int tankOneState = 0;
int tankTwoState = 0;
int lowWaterState = 0;
int lastpumpSwitchState = 0;

int duskdawnflag = 0;

int notEnoughWater = 0;

void setup(void) {
  Serial.begin(9600);
  Serial.println("Restarted");
  
  pinMode(pumpSwitch, INPUT);
  pinMode(lightSwitch, INPUT);
  pinMode(lowWater, INPUT);
  pinMode(tankOne, INPUT);
  pinMode(tankTwo, INPUT);
  
  pinMode(pumpPin, OUTPUT);
  pinMode(lightPin, OUTPUT);
  pinMode(errorLED, OUTPUT);
  //pinMode(blinkLED, OUTPUT);
  
  digitalWrite(pumpSwitch, HIGH);
  digitalWrite(lightSwitch, HIGH);
  digitalWrite(lowWater, HIGH);
  digitalWrite(tankOne, HIGH);
  digitalWrite(tankTwo, HIGH);
  
  digitalWrite(lightPin, HIGH);
  digitalWrite(pumpPin, HIGH);
  
  setupRadio();
  if(getDate()) {
    lastsync = millis();
    if(hour() > 5 && hour() < 21) {
      duskdawnflag = 0;
      DayCycle();
    } else {
      duskdawnflag = 1;
      NightCycle();
    }
  }
}

void loop(void) {
  pumpProcesses();
  
  if(hour() > 5 && hour() < 21) {
      DayCycle();
    } else {
      NightCycle();
    }
  
  delay(100); // using Alarm.delay, yo allow the alarms to be triggered.
  
}

void DayCycle() {
  if(duskdawnflag == 0) {
    Serial.println("Day Mode");
    digitalWrite(lightPin, LOW);
    startPump();
    // make sure we have the right time;
    getDate();
    duskdawnflag = 1;
  }
}

void NightCycle() {
  if(duskdawnflag == 1) {
    Serial.println("Night Mode");
    digitalWrite(lightPin, HIGH);
    // make sure we have the right time;
    getDate();
    duskdawnflag = 0;
  }
}

void setupRadio() {
  radio.begin();

  // Enable this seems to work better
  radio.enableDynamicPayloads();

  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_MAX);
  radio.setChannel(76);
  radio.setRetries(15,15);

  radio.openWritingPipe(pipes[0]); 
  radio.openReadingPipe(1,pipes[1]); 

      
  // Send only, ignore listening mode
  //radio.startListening();

  // Dump the configuration of the rf unit for debugging
  radio.printDetails(); 
}

boolean getDate() {
  char outBuffer[32]=""; // Clear the outBuffer before every loop
  unsigned long send_time, rtt = 0;
  bool timeout=0;
  send_time = millis();
  strcat(outBuffer,"Get Time");
  // Stop listening and write to radio 
  radio.stopListening();
    
  // Send to hub
  if ( radio.write( outBuffer, strlen(outBuffer)) ) {
    Serial.println("Send Time Request: successful\n\r"); 
  }
  else {
    Serial.println("Send Time Request: failed\n\r"); 
    
  }
  
  radio.startListening();
  delay(20);
  
  while ( radio.available() && !timeout ) {
         uint8_t len = radio.getDynamicPayloadSize();
         radio.read( receivePayload, len); 
         
         receivePayload[len] = 0;
         if (len == 19) {
           setTimeFromChar(receivePayload);
                  
         rtt = millis() - send_time;
         Serial.println("Time set");
         
           return true;
           
         } else {
           Serial.print("Send Time Request: unexpected response: ");
           Serial.println(receivePayload);
         }
         //Serial.print("inBuffer --> rtt: ");
         //Serial.println(rtt);          
    
    // Check for timeout and exit the while loop
    if ( millis() - send_time > radio.getMaxTimeout() ) {
         Serial.println("Timeout!!!");
         timeout = 1;
     }   
     delay(10);
   } // End while
  
  return false;   
}

void setTimeFromChar(char* date) {
  char cHour[] = { date[11], date[12], 0 };
  char cMin[] = { date[14], date[15], 0 };
  char cSec[] = { date[17], date[18], 0 };
  char cDay[] = { date[8], date[9], 0 };
  char cMonth[] = { date[5], date[6], 0 };
  char cYear[] = { date[2], date[3], 0 };
  
  Serial.println(date);
  
  Serial.print(" Hour : ");
  Serial.println(cHour);
  Serial.print(" Min : ");
  Serial.println(cMin);
  Serial.print(" Sec : ");
  Serial.println(cSec);
  Serial.print(" Day :  ");
  Serial.println(cDay);
  Serial.print(" Month : ");
  Serial.println(cMonth);
  Serial.print(" Year : ");
  Serial.println(cYear);
  
  setTime(atoi(cHour), atoi(cMin), atoi(cSec), atoi(cDay), atoi(cMonth), atoi(cYear));
}

void startPump() {
  pumpStartTime = millis();
  digitalWrite(pumpPin, LOW);
}
void stopPump() {
  pumpStartTime = 0;
  digitalWrite(pumpPin, HIGH);
}
boolean pumpOverrun() {
  // overrun, just kill this cycle
  if(pumpStartTime == 0) {
    return false;
  }
  if (millis() < pumpStartTime) {
    return true;
  }
  return (millis() - pumpStartTime) > maxPumpRun;
}
void pumpProcesses() {
  if(pumpOverrun() == true) {
    stopPump();
    return;
  }
  pumpState = digitalRead(pumpPin);
  lightState = digitalRead(lightSwitch);
  tankOneState = digitalRead(tankOne);
  tankTwoState = digitalRead(tankTwo);
  lowWaterState = digitalRead(lowWater);
  
  
  /*
  Serial.print("Switch state : ");
  Serial.print(pumpState);
  Serial.print(lightState);
  Serial.print(tankOneState);
  Serial.print(tankTwoState);
  Serial.println(lowWaterState);
  */
  int pumpChanged = (lastpumpSwitchState != digitalRead(pumpSwitch));
  lastpumpSwitchState = digitalRead(pumpSwitch);
  
  
  if(checkTankState() == true) {
    if(digitalRead(pumpPin) == HIGH) {
      if(digitalRead(pumpSwitch) == LOW 
      && pumpChanged == HIGH) {
        Serial.println("Pump button pressed. - Manually started");
        startPump();
      }
    } else {
      if (pumpChanged == HIGH 
      && lastpumpSwitchState == LOW) {
        Serial.println("Pump button pressed. - Manually stopped");
        stopPump();
      }
    }
  } else {
    stopPump();
  }
  
  if(notEnoughWater == 1) {
    digitalWrite(errorLED, !digitalRead(errorLED));
  } else {
    digitalWrite(errorLED, HIGH);
  }
  
}

boolean checkTankState(){
  if(lowWaterState == HIGH) {
    if(digitalRead(pumpPin) == LOW) {
      Serial.println("WARNING: Not enough water to complete the pump cycle.");
    }
    notEnoughWater = 1;
    return false;
  }
  
  if(tankOneState == LOW 
  || tankTwoState == LOW) {
    notEnoughWater = 0;
    if(digitalRead(pumpPin) == LOW) {
      Serial.println("Watercycle complete.");
    }
    return false;
  }
  return true;
}
/*
  float reading;
  float ref;
  
  reading = analogRead(A0);
  ref = analogRead(A1);
  Serial.print("Analog read: ");
  Serial.print(reading);
  Serial.print(" : ");
  Serial.println(ref);
  
  reading = (1023 / reading) -1;
  reading = 540 / reading;
  ref = (1023 / ref) -1;
  ref = 540 / ref;
  
  Serial.print("Sensor resistance: ");
  Serial.print(reading);
  Serial.print(" : ");
  Serial.println(ref);
  
  reading = reading - 300;
  reading = 22.8-  (reading / 56) ;
  Serial.print("Distance in cm: ");
  Serial.println(reading);
  
  delay(1000);
*/
