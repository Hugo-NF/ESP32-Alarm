/*
  Control JFLAlarm central with SMS
  
  Board:
  - TTGO T-Call ESP32 with SIM800L GPRS Module
  
  External libraries:
  - Adafruit Fona Library by Adafruit Version 1.3.5

  Pinout:
    JFLAlarm::pinLED = GPIO_NUM_25;        
    JFLAlarm::pinSIN = GPIO_NUM_19;
    JFLAlarm::pinLIGA = GPIO_NUM_34;
*/

// Project files 
#include "../include/JFLAlarm.hpp"

// Library deps
#include "Adafruit_FONA.h"

// Pin defs
#define SIM800L_RX     27
#define SIM800L_TX     26
#define SIM800L_PWRKEY 4
#define SIM800L_RST    5
#define SIM800L_POWER  23

// Size defs
#define SMS_BUF_LEN 255
#define NOTIFICATION_BUF_LEN 64
#define CALLER_ID_BUF_LEN 32
#define IMEI_BUF_LEN 16

// Timing variables
unsigned long currentTimestamp;
unsigned long eventsInterval = 1000;

int lastLED;
unsigned long lastLEDEvent;
int lastSIN;
unsigned long lastSINEvent;


// Buffers
char replybuffer[SMS_BUF_LEN];
char sim800lNotificationBuffer[NOTIFICATION_BUF_LEN];          //for notifications from the FONA
char smsBuffer[SMS_BUF_LEN];
char imei[IMEI_BUF_LEN] = {0};                                 // MUST use a 16 character buffer for IMEI!
char callerIDbuffer[CALLER_ID_BUF_LEN];                        //we'll store the SMS sender number in here

String smsString = "";

// SIM800L variables
HardwareSerial *sim800lSerial = &Serial1;
Adafruit_FONA sim800l = Adafruit_FONA(SIM800L_PWRKEY);

uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout = 0);

void sendSMS(char *numberID, char *smsText) {
  // Send SMS for status
  if (!sim800l.sendSMS(numberID, smsText)) {
    Serial.println(F("SMS Failed"));
  } else {
    Serial.println(F("SMS Sent!"));
  }
}

void makeCall(char *numberID) {
  // Send SMS for status
  if (!sim800l.callPhone(numberID)) {
    Serial.println(F("Call Failed"));
  } else {
    Serial.println(F("Call Completed!"));
  }
}


void setup() {
  Serial.begin(9600);
  // Setup Alarm pins
  JFLAlarm::setup(GPIO_NUM_25, GPIO_NUM_19, GPIO_NUM_26);
  
  // Get initial state
  lastLED = digitalRead(JFLAlarm::pinLED);
  lastSIN = digitalRead(JFLAlarm::pinSIN);

  lastLEDEvent = millis();
  lastSINEvent = millis();
  
  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(SIM800L_POWER, OUTPUT);

  digitalWrite(BUILTIN_LED, HIGH);
  digitalWrite(SIM800L_POWER, HIGH);

  Serial.println(F("ESP32 with GSM SIM800L"));
  Serial.println(F("Initializing....(May take more than 10 seconds)"));
  
  delay(10000);

  sim800lSerial->begin(4800, SERIAL_8N1, SIM800L_TX, SIM800L_RX);
  if (!sim800l.begin(*sim800lSerial)) {
    Serial.println(F("Couldn't find GSM SIM800L"));
    while (1);
  }
  Serial.println(F("GSM SIM800L is OK"));

  uint8_t imeiLen = sim800l.getIMEI(imei);
  if (imeiLen > 0) {
    Serial.print("SIM card IMEI: "); Serial.println(imei);
  }

  // Set up the FONA to send a +CMTI notification
  // when an SMS is received
  sim800lSerial->print("AT+CNMI=2,1\r\n");

  Serial.println("GSM SIM800L Ready");
}


void loop() {
  char* bufPtr = sim800lNotificationBuffer;    //handy buffer pointer

  if (sim800l.available()) {
    int slot = 0; // this will be the slot number of the SMS
    int charCount = 0;

    // Read the notification into fonaInBuffer
    do {
      *bufPtr = sim800l.read();
      Serial.write(*bufPtr);
      delay(1);
    } while ((*bufPtr++ != '\n') && (sim800l.available()) && (++charCount < (sizeof(sim800lNotificationBuffer)-1)));
    
    //Add a terminal NULL to the notification string
    *bufPtr = 0;

    //Scan the notification string for an SMS received notification.
    //  If it's an SMS message, we'll get the slot number in 'slot'
    if (1 == sscanf(sim800lNotificationBuffer, "+CMTI: \"SM\",%d", &slot)) {
      Serial.print("slot: "); Serial.println(slot);
      
      // Retrieve SMS sender address/phone number.
      if (!sim800l.getSMSSender(slot, callerIDbuffer, CALLER_ID_BUF_LEN-1)) {
          Serial.println("Didn't find SMS message in slot!");
      }
      Serial.print(F("FROM: ")); Serial.println(callerIDbuffer);

      // Retrieve SMS value.
      uint16_t smslen;
      // Pass in buffer and max len!
      if (sim800l.readSMS(slot, smsBuffer, 250, &smslen)) {
        smsString = String(smsBuffer);
        Serial.println(smsString);
      }

      // Uppercase command
      smsString.toUpperCase();

      if (smsString == "STATUS") {
        Serial.println("Alarm status requested");

        delay(100);
        JFLAlarm::writeStatusMessage(replybuffer, SMS_BUF_LEN);

        sendSMS(callerIDbuffer, replybuffer);
      }
      else if (smsString == "ARMAR") {
        Serial.println("Enable alarm requested");

        delay(100);
        JFLAlarm::setAlarm(true);
      }
      else if (smsString == "DESARMAR") {
        Serial.println("Disable alarm requested");

        delay(100);
        JFLAlarm::setAlarm(false);
      }
      else if (smsString == "IMEI") {
        Serial.println("IMEI number requested");

        delay(100);
        sendSMS(callerIDbuffer, imei);
      }
      else if (smsString == "AJUDA") {
        Serial.println("Help text requested");

        delay(100);
        sendSMS(callerIDbuffer, "Comandos disponiveis: STATUS, ARMAR, DESARMAR, IMEI, AJUDA");
      }

      while (1) {
        if (sim800l.deleteSMS(slot)) {
          Serial.println(F("OK!"));
          break;
        }
        else {
          Serial.print(F("Couldn't delete SMS in slot ")); Serial.println(slot);
          sim800l.print(F("AT+CMGD=?\r\n"));
        }
      }
    }
  }

  // Pulling changes on alarm
  currentTimestamp = millis();
  int currentSIN = digitalRead(JFLAlarm::pinSIN);
  int currentLED = digitalRead(JFLAlarm::pinLED);

  // Calculate transitions for LED
  // State changed && (Debouncing elapsed || Timer overflow)
  if((currentLED != lastLED) && ((((currentTimestamp - lastLEDEvent) > eventsInterval)) || (currentTimestamp < lastLEDEvent))) {    
      if(lastLED == 0 && currentLED > 0) {
          sendSMS("+5561991101515", "Alarme armado");
      }
      else if(lastLED > 0 && currentLED == 0) {
          sendSMS("+5561991101515", "Alarme desarmado");
      }
      lastLED = currentLED;
      lastLEDEvent = currentTimestamp;
  }

  // Calculate transitions for SIN
  // State changed && (Debouncing elapsed || Timer overflow)
  if((currentSIN != lastSIN) && ((((currentTimestamp - lastSINEvent) > eventsInterval)) || (currentTimestamp < lastSINEvent))) {
      if(lastSIN == 0 && currentSIN > 0) {
          makeCall("+5561991101515");
      }
      else if(lastSIN > 0 && currentSIN == 0) {
          sendSMS("+5561991101515", "Sirene desligada");
      }
      lastSIN = currentSIN;
      lastSINEvent = currentTimestamp;
  }
}