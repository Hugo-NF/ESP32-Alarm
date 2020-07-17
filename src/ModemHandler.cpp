#include "../include/ModemHandler.hpp"
#include "../include/JFLAlarm.hpp"

constexpr char ModemHandler::numbersAllowed[NUM_IDS_ALLOWED][CALLER_ID_LEN];
  
void ModemHandler::connectModemToNetwork(int baudrate) {
    pinMode(SIM800L_POWER, OUTPUT);
    digitalWrite(SIM800L_POWER, HIGH);

    Serial.println("Turning on SIM Modem, this may take up to 10 seconds");
    delay(10000);

    this->sim800lSerial->begin(baudrate, SERIAL_8N1, SIM800L_TX, SIM800L_RX);
    if (!this->sim800l.begin(*sim800lSerial)) {
        Serial.println(F("Couldn't find GSM SIM800L"));
        while (1);
    }
    Serial.println(F("GSM SIM800L found"));

    this->modemIMEI[IMEI_LEN] = {0}; // MUST use a 16 character buffer for IMEI!
    uint8_t imeiLen = this->sim800l.getIMEI(modemIMEI);
    if (imeiLen > 0) {
        Serial.print("SIM card IMEI: "); Serial.println(modemIMEI);
    }

    // Set up the FONA to send a +CMTI notification
    // when an SMS is received
    this->sim800lSerial->print("AT+CNMI=2,1\r\n");

    Serial.println("GSM SIM800L Ready");
}

void ModemHandler::listenForMessages() {
    char* bufPtr = this->sim800lNotificationsBuf;    //handy buffer pointer

    if (this->sim800l.available()) {
        int slot = 0; // this will be the slot number of the SMS
        int charCount = 0;
        String smsContent;

        // Read the notification into fonaInBuffer
        do {
            *bufPtr = this->sim800l.read();
            Serial.write(*bufPtr);
        } 
        while ((*bufPtr++ != '\n') && (this->sim800l.available()) && (++charCount < (sizeof(sim800lNotificationsBuf)-1)));
        
        //Add a terminal NULL to the notification string
        *bufPtr = 0;

        //Scan the notification string for an SMS received notification.
        //  If it's an SMS message, we'll get the slot number in 'slot'
        if (1 == sscanf(sim800lNotificationsBuf, "+CMTI: \"SM\",%d", &slot)) {
            Serial.print("slot: "); Serial.println(slot);
        
            // Retrieve SMS sender address/phone number.
            if (!this->sim800l.getSMSSender(slot, this->callerIDbuffer, 31)) {
                Serial.println("Didn't find SMS message in slot!");
            }
            Serial.print(F("FROM: ")); Serial.println(this->callerIDbuffer);

            if(ModemHandler::isNumberAllowed(this->callerIDbuffer)){
                // Retrieve SMS value.
                uint16_t smslen;
                // Pass in buffer and max len!
                if (this->sim800l.readSMS(slot, this->inboundSMSBuf, BUFFER_SIZE, &smslen)) {
                    smsContent = String(this->inboundSMSBuf);
                    Serial.println(smsContent);
                }

                // Uppercase command
                smsContent.toUpperCase();

                if (smsContent == "STATUS") {
                    Serial.println("Inbound request for alarm status");
                    this->sendStatusTo(this->callerIDbuffer);
                }
                else if (smsContent == "ARMAR") {
                    Serial.println("Inbound request for alarm arm");

                    JFLAlarm::setAlarm(true);
                    // wait for alarm response
                    delay(1000);
                }
                else if (smsContent == "DESARMAR") {
                    Serial.println("Inbound request for alarm disarm");

                    JFLAlarm::setAlarm(false);
                    // wait for alarm response
                    delay(1000);
                }

                while (1) {
                    if (this->sim800l.deleteSMS(slot)) {
                        Serial.print(F("Slot clear: ")); Serial.println(slot);
                        break;
                    }
                    else {
                        Serial.print(F("Couldn't delete SMS in slot ")); Serial.println(slot);
                        this->sim800l.print(F("AT+CMGD=?\r\n"));
                    }
                }
            }
            else {
                this->sendSMSMessage(this->callerIDbuffer, "This number is not registered to use this system");
            }
        }
    }
}

bool ModemHandler::sendSMSMessage(char *numberID, char *text) {
    return this->sim800l.sendSMS(numberID, text);
}

bool ModemHandler::makeCall(char *numberID) {
    return this->sim800l.callPhone(numberID);
}

bool ModemHandler::sendStatusTo(char *numberID) {
    JFLAlarm::writeStatusMessage(this->outboundSMSBuf, BUFFER_SIZE);

    // Send SMS for status
    return this->sendSMSMessage(numberID, this->outboundSMSBuf);
}

bool ModemHandler::isNumberAllowed(char *callerID) {
    for(int numIndex = 0; numIndex < NUM_IDS_ALLOWED; numIndex++) {
        if(strcmp(ModemHandler::numbersAllowed[numIndex], callerID) == 0) {
            return true;
        }
    }

    return false;
}
