#ifndef JFL_ALARM_H
#define JFL_ALARM_H

#include <Arduino.h>

#define INPUT_THRESHOLD 2460


/**
 * Pinout:
 * JFLAlarm::pinLED = GPIO_NUM_25;        
 * JFLAlarm::pinSIN = GPIO_NUM_19;
 * JFLAlarm::pinLIGA = GPIO_NUM_16;
 * 
*/

class JFLAlarm {

    public:
        // Static members
        static uint8_t pinLED;
        static uint8_t pinSIN;
        static uint8_t pinLIGA;

        // Static methods
        static void setup(uint8_t pinLED = GPIO_NUM_25, uint8_t pinSIN = GPIO_NUM_19, uint8_t pinLIGA = GPIO_NUM_16);
        static void setAlarm(bool set);
        static void writeStatusMessage(char *buf, size_t len);
};

#endif //JFL_ALARM_H