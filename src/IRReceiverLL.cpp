#include "IRReceiverLL.h"

#define DECODE_NEC  // Define the protocol before including the library
#include <IRremote.hpp>

IRReceiverLL::IRReceiverLL(uint8_t irPin, uint8_t ledPin) : _irPin(irPin), _ledPin(ledPin) {}

void IRReceiverLL::begin() {
    Serial.begin(115200);
    pinMode(_ledPin, OUTPUT);
    Serial.println(F("START " __FILE__ " from " __DATE__ "\r\nUsing library version " VERSION_IRREMOTE));
    IrReceiver.begin(_irPin, ENABLE_LED_FEEDBACK);
    Serial.print(F("Ready to receive IR signals of protocols: "));
    printActiveIRProtocols(&Serial);
    Serial.print(F("at pin "));
    Serial.println(_irPin);
}


void IRReceiverLL::handleIR() {
    if (IrReceiver.decode()) {
        digitalWrite(_ledPin, HIGH); // Turn on LED to indicate a message was received
        IrReceiver.printIRResultShort(&Serial);
        IrReceiver.printIRSendUsage(&Serial);
        if (IrReceiver.decodedIRData.protocol == UNKNOWN) {
            Serial.println(F("Received noise or an unknown (or not yet enabled) protocol"));
            IrReceiver.printIRResultRawFormatted(&Serial, true);
        }
        Serial.println();
        handleCommand(IrReceiver.decodedIRData.command);
        IrReceiver.resume();  // Enable receiving of the next value
        digitalWrite(_ledPin, LOW); // Turn off LED
    }
}

void IRReceiverLL::handleCommand(uint8_t command) {
    switch (command) {
        case 0x45:
            Serial.println("Button: Power");
            // Call your function here
            break;
        case 0x46:
            Serial.println("Button: Mode");
            // Call your function here
            break;
        case 0x47:
            Serial.println("Button: Mute");
            // Call your function here
            break;
        case 0x44:
            Serial.println("Button: Play/Stop");
            // Call your function here
            break;
        case 0x40:
            Serial.println("Button: <<");
            // Call your function here
            break;
        case 0x43:
            Serial.println("Button: >>");
            // Call your function here
            break;
        case 0x07:
            Serial.println("Button: EQ");
            // Call your function here
            break;
        case 0x15:
            Serial.println("Button: -");
            // Call your function here
            break;
        case 0x09:
            Serial.println("Button: +");
            // Call your function here
            break;
        case 0x16:
            Serial.println("Button: 0");
            // Call your function here
            break;
        case 0x19:
            Serial.println("Button: Reload");
            // Call your function here
            break;
        case 0x0D:
            Serial.println("Button: U/SD");
            // Call your function here
            break;
        case 0x0C:
            Serial.println("Button: 1");
            // Call your function here
            break;
        case 0x18:
            Serial.println("Button: 2");
            // Call your function here
            break;
        case 0x5E:
            Serial.println("Button: 3");
            // Call your function here
            break;
        case 0x08:
            Serial.println("Button: 4");
            // Call your function here
            break;
        case 0x1C:
            Serial.println("Button: 5");
            // Call your function here
            break;
        case 0x5A:
            Serial.println("Button: 6");
            // Call your function here
            break;
        case 0x42:
            Serial.println("Button: 7");
            // Call your function here
            break;
        case 0x52:
            Serial.println("Button: 8");
            // Call your function here
            break;
        case 0x4A:
            Serial.println("Button: 9");
            // Call your function here
            break;
        default:
            Serial.println("Unknown command");
            break;
    }
}