/*
  Control JFLAlarm central with SMS
  
  Board:
  - TTGO T-Call ESP32 with SIM800L GPRS Module
  
  External libraries:
  - Adafruit Fona Library by Adafruit Version 1.3.5

  Pinout:
    JFLAlarm::pinLED = GPIO_NUM_25 = preto;        
    JFLAlarm::pinSIN = GPIO_NUM_15 = branco;
    JFLAlarm::pinLIGA = GPIO_NUM_12 = cinza;
    GND = verde
    +5V = amarelo
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
#define SMS_SLOTS 20
#define AMT_NUMBERS 3
#define SMS_BUF_LEN 255
#define NOTIFICATION_BUF_LEN 64
#define CALLER_ID_BUF_LEN 32
#define IMEI_BUF_LEN 16

// Timing variables
unsigned long currentTimestamp;
unsigned long eventsInterval = 1000;

uint16_t lastLED;
unsigned long lastLEDEvent;
uint16_t lastSIN;
unsigned long lastSINEvent;

// Alarm state control
hw_timer_t *timer = NULL;
bool isArming = false;
volatile bool pendingAlarm = false;
volatile int nextNotify = 0;

// Buffers
char allowedNumbers[AMT_NUMBERS][CALLER_ID_BUF_LEN] = { "+5561999267740", "+5561991101515", "+5561992404979" };                   //numbers allowed to use
char replybuffer[SMS_BUF_LEN];
char sim800lNotificationBuffer[NOTIFICATION_BUF_LEN];          //for notifications from the FONA
char smsBuffer[SMS_BUF_LEN];
char imei[IMEI_BUF_LEN] = {0};                                 // MUST use a 16 character buffer for IMEI!
char callerIDbuffer[CALLER_ID_BUF_LEN];                        //we'll store the SMS sender number in here

String smsString = "";

// SIM800L variables
HardwareSerial *sim800lSerial = &Serial1;
Adafruit_FONA sim800l = Adafruit_FONA(SIM800L_PWRKEY);

// Function prototypes (compiler look-ahead)
uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout = 0);

bool isAllowed(char *number);
void sendSMS(char *numberID, char *smsText);
void makeCall(char *numberID);
void clearSMSSlots(uint16_t amount);

bool isAllowed(char *number) {
  for(int index = 0; index < AMT_NUMBERS; index++) {
    if(strcmp(number, allowedNumbers[index]) == 0) {
      return true;
    }
  }
  return false;
}

void sendSMS(char *numberID, char *smsText) {
  // Send SMS for status
  if (!sim800l.sendSMS(numberID, smsText)) {
    Serial.println(F("SMS Failed"));
  } 
  else {
    Serial.println(F("SMS Sent!"));
  }
  // SIM 800L Modem has 8 slots
  clearSMSSlots(SMS_SLOTS);
}

void makeCall(char *numberID) {
  // Send SMS for status
  if (!sim800l.callPhone(numberID)) {
    Serial.println(F("Call Failed"));
  } 
  else {
    Serial.println(F("Call Completed!"));
  }
}

void clearSMSSlots(uint16_t amount) {
  for(uint16_t index = 0; index < amount; index++) {
    if (!sim800l.deleteSMS(index)) {
      Serial.printf("SMS slot %d delete FAILED", index);
    } 
    else {
      Serial.printf("SMS slot %d deleted!", index);
    }
  }
}

// Timer interrupt service routine
void IRAM_ATTR timerISR() {
  Serial.println("2 minutes elapsed. Calling next number");
  if(pendingAlarm) {
    makeCall(allowedNumbers[nextNotify]);
    if(nextNotify < AMT_NUMBERS - 1) {
      nextNotify += 1;
    }
    else {
      nextNotify = 0;
    }
  }
}

void setup() {
  // Start serial uart
  Serial.begin(9600);

  // Setup timer
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &timerISR, true);
  
  // Setup Alarm pins
  JFLAlarm::setup(GPIO_NUM_25, GPIO_NUM_15, GPIO_NUM_12);
  
  // Get initial state
  lastLED = analogRead(JFLAlarm::pinLED);
  lastSIN = analogRead(JFLAlarm::pinSIN);

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

  // Before starting, delete all SMS slots from storage
  clearSMSSlots(SMS_SLOTS);
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
      Serial.printf("slot: %d\n", slot);
      
      // Retrieve SMS sender address/phone number.
      if (!sim800l.getSMSSender(slot, callerIDbuffer, CALLER_ID_BUF_LEN-1)) {
        Serial.println("Didn't find SMS message in slot!");
      }
      Serial.print(F("FROM: ")); Serial.println(callerIDbuffer);

      if(isAllowed(callerIDbuffer)) {
        // Retrieve SMS value.
        uint16_t smslen;
        // Pass in buffer and max len!
        if (sim800l.readSMS(slot, smsBuffer, SMS_BUF_LEN, &smslen)) {
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
        else if (smsString == "RESTART") {
          Serial.println("Restart requested");
          
          snprintf(replybuffer, SMS_BUF_LEN, "O ESP32 será reiniciado, esse processo pode levar até 2 minutos.");
          sendSMS(callerIDbuffer, replybuffer);
          
          delay(100);
          ESP.restart();
        }
        else if (smsString == "ARMAR") {
          Serial.println("Enable alarm requested");

          delay(100);
          JFLAlarm::setAlarm(true);
          isArming = true;
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
        else if(smsString == "LISTAR") {
          Serial.println("Number list requested");

          delay(100);
          snprintf(replybuffer, SMS_BUF_LEN, "Numeros:\n0. %s\n1. %s\n2. %s\n", 
            allowedNumbers[0], allowedNumbers[1], allowedNumbers[2]);
          sendSMS(callerIDbuffer, replybuffer);
        }
        else if(smsString.startsWith("REG")) {
          Serial.println("Registering new number");

          int index = smsString.lastIndexOf(' ');
          int numReplNumber = smsString[index-1] - '0';
          Serial.println(numReplNumber);
          Serial.println((numReplNumber < AMT_NUMBERS - 1 && numReplNumber >= 0));

          if(numReplNumber < AMT_NUMBERS && numReplNumber >= 0) {
            Serial.printf("Replacing number on position %d\n", numReplNumber);
            String newNumber = smsString.substring(index);
            
            strcpy(allowedNumbers[numReplNumber], newNumber.c_str());
          }
          
          delay(100);
          snprintf(replybuffer, SMS_BUF_LEN, "Numeros:\n0. %s\n1. %s\n2. %s\n", 
            allowedNumbers[0], allowedNumbers[1], allowedNumbers[2]);
          sendSMS(callerIDbuffer, replybuffer);
        }
        else if (smsString == "AJUDA") {
          Serial.println("Help text requested");

          delay(100);
          sendSMS(callerIDbuffer, "Comandos disponiveis: STATUS, RESTART, ARMAR, DESARMAR, IMEI, LISTAR, REG, AJUDA");
        }
        else {
          delay(100);
          sendSMS(callerIDbuffer, "Comando desconhecido, envie AJUDA para ver os comandos disponiveis.");
        }

        while (1) {
          if (sim800l.deleteSMS(slot)) {
            Serial.printf("Slot %d clear\n", slot);
            break;
          }
          else {
            Serial.print(F("Couldn't delete SMS in slot ")); Serial.println(slot);
            sim800l.print(F("AT+CMGD=?\r\n"));
          }
        }
      }
    }
  }

  // Pulling changes on alarm
  currentTimestamp = millis();
  uint16_t currentSIN = analogRead(JFLAlarm::pinSIN);
  uint16_t currentLED = analogRead(JFLAlarm::pinLED);

  // Calculate transitions for LED
  // State changed && (Debouncing elapsed || Timer overflow)
  if(((((currentTimestamp - lastLEDEvent) > eventsInterval)) || (currentTimestamp < lastLEDEvent))) {    
      if(lastLED < INPUT_THRESHOLD && currentLED >= INPUT_THRESHOLD) {
        sendSMS(allowedNumbers[0], "Alarme armado");
      }
      else if(lastLED >= INPUT_THRESHOLD && currentLED < INPUT_THRESHOLD) {
        sendSMS(allowedNumbers[0], "Alarme desarmado");
      }
      lastLED = currentLED;
      lastLEDEvent = currentTimestamp;
  }

  // Calculate transitions for SIN
  // State changed && (Debouncing elapsed || Timer overflow)
  if(((((currentTimestamp - lastSINEvent) > eventsInterval)) || (currentTimestamp < lastSINEvent))) {
      if(lastSIN < INPUT_THRESHOLD && currentSIN >= INPUT_THRESHOLD) {
        if(isArming){
          isArming = false;
          delay(1000);
        }
        else {
          makeCall(allowedNumbers[nextNotify]);
          if(nextNotify < AMT_NUMBERS - 1) {
            nextNotify += 1;
          }
          else {
            nextNotify = 0;
          }
          pendingAlarm = true;

          // 1 minute interval
          timerAlarmWrite(timer, 120000000, true);
          timerAlarmEnable(timer);
        }
      }
      else if(lastSIN >= INPUT_THRESHOLD && currentSIN < INPUT_THRESHOLD) {
        // Disables the timer (prevents calling the next number)
        timerAlarmDisable(timer);
        pendingAlarm = false;
        nextNotify = 0;

        sendSMS(allowedNumbers[0], "Sirene desligada");
      }
      lastSIN = currentSIN;
      lastSINEvent = currentTimestamp;
  }
}