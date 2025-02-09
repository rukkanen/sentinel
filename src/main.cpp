#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

const int radarPin = D1; // Pin connected to the microwave radar
const int irPin = D2; // Pin connected to the IR sensor
const char* fileName = "/radar_data.json";

int oldMicValue = 0;
int tick = 0;

void writeToFlash(int value) {
    File file = LittleFS.open(fileName, "w");
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }

    StaticJsonDocument<200> doc;
    doc["radarValue"] = value;

    //Serial.begin(74880);
    pinMode(radarPin, INPUT);

    if (serializeJson(doc, file) == 0) {
        Serial.println("Failed to write to file");
    }
    file.close();
}

void setup() {
    Serial.begin(74880);
    LittleFS.begin();
}

void loop() {
    int radarValue = digitalRead(radarPin);
    //writeToFlash(radarValue);
    //Serial.println("Data written to flash: " + String(radarValue));

    // get and werite data from IR sensor
    int IRValue = digitalRead(irPin);
    int isValue = digitalRead(D2);
    Serial.println("IR digi data: " + String(isValue));
    Serial.println("Radar data: " + String(radarValue));
    Serial.println("Tick: " + String(++tick));

    /* if (oldMicValue != micValue) {
        Serial.println("Microphone data changed: " + String(micValue));
        oldMicValue = micValue;
    } */
/*     File micFile = LittleFS.open("/mic_data.json", "w");
    if (!micFile) {
        Serial.println("Failed to open file for writing");
        return;
    }

    StaticJsonDocument<200> micDoc;
    micDoc["micValue"] = micValue;

   

    if (serializeJson(micDoc, micFile) == 0) {
        Serial.println("Failed to write to file");
    } 
    micFile.close();*/

    //Serial.println("Microphone data written to flash: " + String(micValue));
    delay(500); // Wait for 1 second before reading again
}