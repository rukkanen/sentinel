#ifndef IRRECEIVER_H
#define IRRECEIVER_H

#include <Arduino.h>

class IRReceiverLL {
public:
    IRReceiverLL(uint8_t irPin = 4, uint8_t ledPin = 2);
    void begin();
    void handleIR();

private:
    uint8_t _irPin;
    uint8_t _ledPin;
    void handleCommand(uint8_t command);
    void handleUnknownRAWMessage();
};

#endif // IRRECEIVER_H
