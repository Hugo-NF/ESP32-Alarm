#ifndef MODEM_HANDLER_H
#define MODEM_HANDLER_H

/**
 * SMS helpers
 * External libraries:
 * - Adafruit Fona Library by Adafruit Version 1.3.5
 * Board:
 * - TTGO T-Call ESP32 with SIM800L GPRS Module
 *   https://my.cytron.io/p-ttgo-t-call-esp32-with-sim800l-gprs-module
*/

#include "Adafruit_FONA.h"

// SIM800L Pinout (current based on TTGO T-Call board)
#define SIM800L_RX     27
#define SIM800L_TX     26
#define SIM800L_PWRKEY 4
#define SIM800L_RST    5
#define SIM800L_POWER  23

#define IMEI_LEN 16
#define NOTIFICATIONS_LEN 64
#define NUM_IDS_ALLOWED 3
#define CALLER_ID_LEN 32
#define BUFFER_SIZE 255


class ModemHandler {

    private:
        // SIM800L GSM Modem
        HardwareSerial *sim800lSerial = &Serial1;
        Adafruit_FONA sim800l = Adafruit_FONA(SIM800L_PWRKEY);

        // Buffers
        char modemIMEI[IMEI_LEN];

        char sim800lNotificationsBuf[NOTIFICATIONS_LEN];
        
        char callerIDbuffer[CALLER_ID_LEN];
        char inboundSMSBuf[BUFFER_SIZE];
        char outboundSMSBuf[BUFFER_SIZE];

    public:
        constexpr static char numbersAllowed[NUM_IDS_ALLOWED][CALLER_ID_LEN] = {
            "+5561991101515",
            "+5561999267740",
            "+5561992404979"
        };
        
        // Default constructor and destructor
        explicit ModemHandler() = default;
        virtual ~ModemHandler() = default;

        void connectModemToNetwork(int baudrate);
        void listenForMessages();
        bool sendSMSMessage(char *numberID, char *text);
        bool makeCall(char *numberID); 
        bool sendStatusTo(char *numberID);

        static bool isNumberAllowed(char *callerID);

};

#endif //MODEM_HANDLER_H