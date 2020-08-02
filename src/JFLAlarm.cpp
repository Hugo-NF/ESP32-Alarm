#include "../include/JFLAlarm.hpp"

// Static members
uint8_t JFLAlarm::pinLED;
uint8_t JFLAlarm::pinSIN;
uint8_t JFLAlarm::pinLIGA;


// Static methods
void JFLAlarm::setup(uint8_t pinLED, uint8_t pinSIN, uint8_t pinLIGA) {

    JFLAlarm::pinLED = pinLED;        
    JFLAlarm::pinSIN = pinSIN;
    JFLAlarm::pinLIGA = pinLIGA;

    // pinModes
    pinMode(JFLAlarm::pinLIGA, OUTPUT);
    pinMode(JFLAlarm::pinLED, INPUT_PULLDOWN);
    pinMode(JFLAlarm::pinSIN, INPUT_PULLDOWN);

    // pinLIGA is active on LOW state
    digitalWrite(JFLAlarm::pinLIGA, HIGH);
}


void JFLAlarm::setAlarm(bool set) {

    // Reading LED pin
    int currentState = analogRead(JFLAlarm::pinLED);
    
    // Changing state (enabled and want to disable) or (disabled and and want to enable)
    if((currentState >= INPUT_THRESHOLD && !set) || (currentState < INPUT_THRESHOLD && set)) {
        // 1 seg pulse
        digitalWrite(JFLAlarm::pinLIGA, LOW);

        delay(1000);

        digitalWrite(JFLAlarm::pinLIGA, HIGH);
    }
}

void JFLAlarm::writeStatusMessage(char *buf, size_t len) {

    int currentEnabled = digitalRead(JFLAlarm::pinLED);
    int currentTriggered = digitalRead(JFLAlarm::pinSIN);

    snprintf(buf, len, "Status atual:\r\nArmado: %d\r\nDisparo: %d", currentEnabled, currentTriggered);
}