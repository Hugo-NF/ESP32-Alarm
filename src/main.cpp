#include "../include/ModemHandler.hpp"
#include "../include/JFLAlarm.hpp"

/**
 * Pinout:
 * JFLAlarm::pinLED = GPIO_NUM_25;        
 * JFLAlarm::pinSIN = GPIO_NUM_19;
 * JFLAlarm::pinLIGA = GPIO_NUM_34;
 * 
*/

// Global vars
ModemHandler simModem;

unsigned long currentTimestamp;

int lastLED;
unsigned long lastLEDEvent;
int lastSIN;
unsigned long lastSINEvent;

unsigned long eventsInterval = 1000;


void setup() {
    // Open serial port
    Serial.begin(9600);

    // Turn on SIM Modem
    simModem.connectModemToNetwork(9600);
    
    // Setup Input pins
    JFLAlarm::setup(GPIO_NUM_25, GPIO_NUM_19, GPIO_NUM_26);

    // Get initial state
    lastLED = digitalRead(JFLAlarm::pinLED);
    lastSIN = digitalRead(JFLAlarm::pinSIN);

    lastLEDEvent = millis();
    lastSINEvent = millis();
}


void loop() {
    
    simModem.listenForMessages();
    
    // Pulling changes on alarm
    currentTimestamp = millis();
    int currentSIN = digitalRead(JFLAlarm::pinSIN);
    int currentLED = digitalRead(JFLAlarm::pinLED);

    // Calculate transitions for LED
    // State changed && (Debouncing elapsed || Timer overflow)
    if((currentLED != lastLED) && ((((currentTimestamp - lastLEDEvent) > eventsInterval)) || (currentTimestamp < lastLEDEvent))) {    
        if(lastLED == 0 && currentLED > 0) {
            simModem.sendSMSMessage("+5561991101515", "Alarme armado");
        }
        else if(lastLED > 0 && currentLED == 0) {
            simModem.sendSMSMessage("+5561991101515", "Alarme desarmado");
        }
        lastLED = currentLED;
        lastLEDEvent = currentTimestamp;
    }

    // Calculate transitions for SIN
    // State changed && (Debouncing elapsed || Timer overflow)
    if((currentSIN != lastSIN) && ((((currentTimestamp - lastSINEvent) > eventsInterval)) || (currentTimestamp < lastSINEvent))) {
        if(lastSIN == 0 && currentSIN > 0) {
            simModem.makeCall("+5561991101515");
        }
        else if(lastSIN > 0 && currentSIN == 0) {
            simModem.sendSMSMessage("+5561991101515", "Sirene desligada");
        }
        lastSIN = currentSIN;
        lastSINEvent = currentTimestamp;
    }
}