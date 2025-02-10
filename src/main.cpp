#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

/*
Example sensor_logs.json format:
{
  "logs": [
    {
      "timestamp": "2021-09-01 12:00:00",
      "event": "sound_detected"
    },
    {
      "timestamp": "2021-09-01 12:00:01",
      "event": "radar_detected"
    }
  ]
}
*/

// flash file system
File file;
volatile bool fileOpened = false;

// sensor and flash flags
volatile bool sensorsOn = false;
volatile bool flashOn = false;
volatile bool serialOn = false;

// time since last command
unsigned long lastCommand = 0;
// time since last data flush
unsigned long lastFlush = 0;

// time since last sound detected
unsigned long lastSound = 0;
// time since last radar detected
unsigned long lastRadar = 0;

// loop tick
unsigned long lTick = 0;

// HW PINS
#define SOUND_SENSOR_PIN D5  // D0 connected to D5 (GPIO14)
#define RADAR_SENSOR_PIN D1  // D0 connected to D5 (GPIO14)
//volatile bool soundDetected = false;

// a dummy datetimestring function
String getDateTimeString() {
  return "2021-09-01 12:00:00";
}

void openJSON() {
  // Seek to the end of the file
  file.seek(file.size() - 1);

  // Read backwards to find the closing brackets
  while (file.position() > 0) {
    file.seek(file.position() - 1);
    char c = file.read();
    if (c == ']') {
      // Found the closing bracket of the array
      file.seek(file.position() - 1);
      break;
    }
  }

  // Now the file pointer is at the position just before the closing bracket
  // Write a comma to separate the new entry
  file.print(",");
}

void appendToJsonFile(const char* event) {
  // Append the new JSON entry
  file.println("{");
  file.println("\"timestamp\": \"" + getDateTimeString() + "\",");
  file.println("\"event\": \"" + String(event) + "\"");
  file.print("}");
}

void IRAM_ATTR soundISR() {
  if (!sensorsOn) return;
  
  if (millis() - lastSound > 200) {  // 200ms debounce time
    if (serialOn) {
      Serial.println("sound_detected");
    }
    if (flashOn && file) {
      appendToJsonFile("sound");
    }
    lastSound = millis();
  }
}

void IRAM_ATTR radarISR() {
  if (!sensorsOn) return;
  if (millis() - lastRadar > 200) {  // 200ms debounce time
    if (serialOn) {
      Serial.println("radar_detected");
    }
    if (flashOn && file) {
      appendToJsonFile("radar");
    }
    lastRadar = millis();
  }
  Serial.println("radar_detected");
}

// method for emptying data from flash over serial
void flushData() {
  if (flashOn && file) {
    file.seek(0);
    while (file.available()) {
      Serial.write(file.read());
    }
    file.print("{\"logs\":[]}");
    file.flush();
    Serial.println("Data flushed");
  }
}

void setupFlash() {
  if (!flashOn) {
    // Initialize LittleFS and open the JSON file
    bool flashOk = LittleFS.begin();
    if (!flashOk) {
      Serial.println("!flash_mount_failed");
    } else {    
      Serial.println("!flash_mounted");
    }

    file = LittleFS.open("/sensor_logs.json", "r+");
    if (!file || file.size() == 0) {
      Serial.println("!file_open_failed");
      file = LittleFS.open("/sensor_logs.json", "w+");
      if (!file) {
        Serial.println("!file_creation_failed");
        return;
      }
      // Write an empty JSON array to the file
      file.println("[]");
      file.flush();
    } else {
      openJSON();
      Serial.println("!file_opened");
    }
    flashOn = true;
  }
}

void connectSensors() {
  pinMode(SOUND_SENSOR_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(SOUND_SENSOR_PIN), soundISR, RISING);

  pinMode(RADAR_SENSOR_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(RADAR_SENSOR_PIN), radarISR, RISING);
}

// fcuntion listens to serial 
// sensors on/off
// use flash to store data on/off
// use serial to send data on/off
// flush data from flash
// time since last sound detected
// time since last radar detected
// time since last command
// time since last data flush
void handleCommand() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    if (command.startsWith("!")) {
      command = command.substring(1);
    } else {
      Serial.println("--Message-- " + command);
      return;
    }
    //Serial.println("Command received: " + command);

    if (command == "sensors_on") {
      connectSensors();
      sensorsOn = true;
      Serial.println("Turning sensors on...");
    } else if (command == "hello") {
      Serial.println("!hello");
    } else if (command == "sensors_off") {
      sensorsOn = false;
      Serial.println("Turning sensors off...");
    } else if (command == "flash_on") {
      flashOn = true;
      setupFlash();      
      Serial.println("Turning flash on...");
    } else if (command == "flash_off") {
      if (file) {
        file.flush();
        file.close();        
      }
      Serial.println("!flash_closed");
      flashOn = false;
      Serial.println("Turning flash off...");
    } else if (command == "serial_data_on") {
      serialOn = true;
      Serial.println("Turning serial on...");
    } else if (command == "serial_data_off") {
      serialOn = false;
      Serial.println("Turning serial off...");
    } else if (command == "flush_flash") {
      Serial.println("Flushing data...");
      // print the whole file to serial
    } if (command == "clear_flash") {
      Serial.println("Clearing flash");
      flushData();
      // print the whole file to serial
    } else {
      Serial.println("Unknown command: " + command);
    }
  }
}

void setup() {
  Serial.begin(74880);
  Serial.println("!starting");
}

void loop() {
  Serial.println("Looping... tick: " + String(++lTick));
  handleCommand();
  delay(1000);
}