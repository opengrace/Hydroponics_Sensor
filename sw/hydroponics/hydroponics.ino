#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include <Time.h>
#include <TimeAlarms.h>
#include <Wire.h>
#include <DS1307RTC.h>

#define RF_SETUP 0x17

RF24 radio(9,10);
const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0x7365727631LL };
char receivePayload[32];
uint8_t counter=0;


const int lightPin = 7; //D7
const int pumpPin = 6; //D6
const int errorLED = 8; //D8
const int blinkLED = A1; //A1
const int eTapeRef = A7; //A7
const int eTape = A6; //A6
const int pumpSwitch = A0; //A0
const int lightSwitch = A5; //A5
const int lowWater = A4;//A4
const int tankOne = A2;//A2
const int tankTwo = A3;//A3

const float minTape = 761;
const float maxTape = 118.92;

int pumpState = 0;
int lightState = 0;
int tankOneState = 0;
int tankTwoState = 0;
int lowWaterState = 0;
int lastpumpSwitchState = 0;
int lastLightSwitchState = 0;

int notEnoughWater = 0;

void setup(void) {
  Serial.begin(9600);
  
  setupRadio();
  if(getDate()) {
   // create the alarms 
    Alarm.alarmRepeat(6,0,0, MorningAlarm);  // 0600 every day
    Alarm.alarmRepeat(22,00,0, EveningAlarm);  // 2200 every day 
    if(hour() > 6 || hour() < 22) {
      MorningAlarm();
    } else {
      EveningAlarm();
    }
  }
  
  pinMode(pumpSwitch, INPUT);
  pinMode(lightSwitch, INPUT);
  pinMode(lowWater, INPUT);
  pinMode(tankOne, INPUT);
  pinMode(tankTwo, INPUT);
  
  pinMode(pumpPin, OUTPUT);
  pinMode(lightPin, OUTPUT);
  pinMode(errorLED, OUTPUT);
  pinMode(blinkLED, OUTPUT);
  
  digitalWrite(pumpSwitch, HIGH);
  digitalWrite(lightSwitch, HIGH);
  digitalWrite(lowWater, HIGH);
  digitalWrite(tankOne, HIGH);
  digitalWrite(tankTwo, HIGH);
  digitalWrite(blinkLED, HIGH);
  
  digitalWrite(lightPin, HIGH);
  digitalWrite(pumpPin, HIGH);
}

void loop(void) {
  pumpProcesses();
  lightProcess();
  Alarm.delay(100);
  
}

void MorningAlarm() {
  Serial.println("Morning Alarm");
  digitalWrite(lightPin, LOW);
  digitalWrite(pumpPin, LOW);
  // make sure we have the right time;
  getDate();
}

void EveningAlarm() {
  Serial.println("Evening Alarm");
  digitalWrite(lightPin, HIGH);
  // make sure we have the right time;
  getDate();
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
  digitalWrite(blinkLED, LOW);
  if ( radio.write( outBuffer, strlen(outBuffer)) ) {
    Serial.println("Send Time Request: successful\n\r"); 
  }
  else {
    Serial.println("Send Time Request: failed\n\r"); 
  }
  digitalWrite(blinkLED, HIGH);
  
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
  Serial.print(cHour);
  Serial.print(" Min : ");
  Serial.print(cMin);
  Serial.print(" Sec : ");
  Serial.print(cSec);
  Serial.print(" Day :  ");
  Serial.print(cDay);
  Serial.print(" Month : ");
  Serial.print(cMonth);
  Serial.print(" Year : ");
  Serial.println(cYear);
  
  setTime(atoi(cHour), atoi(cMin), atoi(cSec), atoi(cDay), atoi(cMonth), atoi(cYear));
}

void lightProcess(){
	lightState = digitalRead(lightSwitch);
	int lightChanged = (lastLightSwitchState != digitalRead(lightSwitch));
	lastLightSwitchState = digitalRead(lightSwitch);
	
	if(checkTankState() == true) {
		if(digitalRead(lightPin) == HIGH) {
			if(digitalRead(lightSwitch) == LOW
			&& lightChanged == HIGH) {
				digitalWrite(lightPin, LOW);
			}
			} else {
			if (lightChanged == HIGH
			&& lastLightSwitchState == LOW) {
				digitalWrite(lightPin, HIGH);
			}
		}
		} else {
		digitalWrite(lightPin, HIGH);
	}
}
void pumpProcesses() {
  pumpState = digitalRead(pumpPin);
  
  tankOneState = digitalRead(tankOne);
  tankTwoState = digitalRead(tankTwo);
  lowWaterState = digitalRead(lowWater);
  
  int pumpChanged = (lastpumpSwitchState != digitalRead(pumpSwitch));
  lastpumpSwitchState = digitalRead(pumpSwitch);
  /*
  Serial.print("Switch state : ");
  Serial.print(pumpState);
  Serial.print(lightState);
  Serial.print(tankOneState);
  Serial.print(tankTwoState);
  Serial.print(lowWaterState);
  Serial.println(pumpChanged);
  */
  if(checkTankState() == true) {
    if(digitalRead(pumpPin) == HIGH) {
      if(digitalRead(pumpSwitch) == LOW 
      && pumpChanged == HIGH) {
        digitalWrite(pumpPin, LOW);
      }
    } else {
      if (pumpChanged == HIGH 
      && lastpumpSwitchState == LOW) {
        digitalWrite(pumpPin, HIGH);
      }
    }
  } else {
    digitalWrite(pumpPin, HIGH);
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
