#include <Arduino.h>
#include <LittleFS.h>
#include <TimeLib.h>  // Include the Time library
#include "driver/rtc_io.h"
#include "IRReceiverLL.h"

// The HW running this is ESP32-WROOM-32E

/*
sensor_logs.txt format:
date_time: timestamp: event_type: sensor/topic: message

yes... There is both the date:time and timestamp in that. I'm lazy
and don't want to convert the timestamp to date:time every time I
want to read the logs. 
*/

// flash file system
File file;
volatile bool fileOpened = false;

// sensor and flash flags
volatile bool sensorsOn = false;
volatile bool flashOn = false;
volatile bool serialOn = false;
volatile bool debugOn = false;  // Debug flag

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
#define SOUND_SENSOR_PIN 14  // GPIO14
#define RADAR_SENSOR_PIN 12  // GPIO12
//volatile bool soundDetected = false;

#define PATH "/sensor_logs.txt"

IRReceiverLL irReceiver;  // Use default pins: IR pin = GPIO4, LED pin = GPIO2

// a function to set the time based on the received timestamp
void setTimeFromTimestamp(const String& timestamp) {
  time_t t = timestamp.toInt();
  setTime(t);
}

// a function to get the current date and time as a string
String getDateTimeString() {
  char buffer[20];
  sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d", year(), month(), day(), hour(), minute(), second());
  return String(buffer);
}

// deprecated! Use just strings in the log!
// timestamp: event_type: topic: message (or something)
/*void openJSON() {

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
}*/

void addEventToLog(const char* event) {
if (!file) {
    Serial.println("+file_not_open");
    return;
  }
  file.println("\"timestamp\": \"" + getDateTimeString() + "\",");
  file.println("\"event\": \"" + String(event) + "\"");
  file.print("}");
}

void IRAM_ATTR soundISR() {
  if (!sensorsOn) return;
  
  if (millis() - lastSound > 200) {  // 200ms debounce time
    if (serialOn) {
      Serial.println("#sound");
    }
    if (flashOn && file) {
      addEventToLog("sound");
    }
    lastSound = millis();
  }
}

void IRAM_ATTR radarISR() {
  if (!sensorsOn) return;

  if (millis() - lastRadar > 200) {  // 200ms debounce time
    if (serialOn) {
      Serial.println("!radar_detected");
    }
    if (flashOn && file) {
      addEventToLog("radar");
    }
    lastRadar = millis();
  }
}

void clearFlash() {
  if (flashOn && file) {
    file.seek(0);
    file.print("{\"logs\":[]}");
    file.flush();
  }
  openJSON();
}

// method for emptying data from flash over serial
void flushDataToSerial() {
  if (flashOn && file) {
    Serial.println("?sensor_data_start");
    file.seek(0);
    while (file.available()) {
      String line = file.readStringUntil('\n');
      Serial.print(line);
    }
    clearFlash();
    Serial.println("?sensor_data_end");
  } else {
    Serial.println("?flash_not_in_use");
  }
}

void setupFlash() {
  if (!flashOn) {
    // Initialize LittleFS and open the JSON file
    bool flashOk = LittleFS.begin();
    yield();
    delay(500);
    if (!flashOk) {
      Serial.println("?flash_mount_failed");
    } else {    
      Serial.println("?flash_mounted");
    }

    file = LittleFS.open("/sensor_logs.json", "r+");
    Serial.println("-- DOING: LittleFS.open");
    if (!file) {
      Serial.println("?file_open_failed");
      file = LittleFS.open("/sensor_logs.json", "w+");
      if (!file) {
        Serial.println("?file_creation_failed");
        return;
      }
      // Write an empty JSON array to the file
      file.println("{\"logs\":[]}");
      file.flush();
    } else {
      openJSON();
      Serial.println("!file_opened");
    }
    Serial.println("-- DOING: LittleFS.open done. File exists: " + String(file));
    flashOn = true;
  } else {
    Serial.println("?flash_already_on");
  }
}

void connectSensors() {
  pinMode(SOUND_SENSOR_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(SOUND_SENSOR_PIN), soundISR, RISING);

  pinMode(RADAR_SENSOR_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(RADAR_SENSOR_PIN), radarISR, RISING);
  sensorsOn = true;
}

// function to put the ESP32 into deep sleep and wake up on noise or movement
void enterDeepSleep() {
  // Configure the wake-up sources
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_14, 1); // Wake up on HIGH signal from SOUND_SENSOR_PIN
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_12, 1); // Wake up on HIGH signal from RADAR_SENSOR_PIN

  // Print message before entering deep sleep
  Serial.println("Entering deep sleep...");

  // Enter deep sleep
  esp_deep_sleep_start();
}

// function listens to serial 
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
      Serial.println("--Message--: " + command);
      return;
    }
    //Serial.println("Command received: " + command);

    if (command == "sensors_on") {
      connectSensors();
      Serial.println("--Turning sensors on...");
    } else if (command == "hello") {
      Serial.println("hello_to_you");
    } else if (command.startsWith("timestamp:")) {
      // command is structured as timestamp:1324523452
      String timestamp = command.substring(10);
      setTimeFromTimestamp(timestamp);
      Serial.println("--Time set to: " + getDateTimeString());
    } else if (command == "sensors_off") {
      sensorsOn = false;
      Serial.println("--Turning sensors off...");
    } else if (command == "flash_on") {
      setupFlash();      
      Serial.println("--Turning flash on...");
    } else if (command == "flash_off") {
      if (file) {
        file.flush();
        file.close();        
      }
      Serial.println("flash_closed");
      flashOn = false;
      Serial.println("--Turning flash off...");
    } else if (command == "serial_data_on") {
      serialOn = true;
      Serial.println("--Turning serial on...");
    } else if (command == "serial_data_off") {
      serialOn = false;
      Serial.println("--Turning serial off...");
    } else if (command == "flush_data_to_serial") {
      Serial.println("--Flushing data...");
      flushDataToSerial();
      // print the whole file to serial
    } else if (command == "clear_flash") {
      Serial.println("clearing_flash");
      clearFlash();
      // print the whole file to serial
    } else if (command == "reboot") {
      Serial.println("?rebooting");
      // reboot nodemcuv2 
      ESP.restart();
    } else if (command == "ALL_ON") {
      Serial.println("?doing_full_on");
      // turn everything on
      connectSensors();
      yield();
      setupFlash();
      yield();
      serialOn = true;
    } else if (command == "sleep") {
      enterDeepSleep();
    } else if (command == "debug_on") {
      debugOn = true;
      Serial.println("--Debug mode on...");
    } else if (command == "debug_off") {
      debugOn = false;
      Serial.println("--Debug mode off...");
    } else {
      Serial.println("Unknown command: " + command);
    }
  }
}



void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("!starting");

  // Check if the wakeup was caused by an external wakeup source
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Woke up from deep sleep due to external wakeup source");
  }

  irReceiver.begin();
}

void loop() {
  if (debugOn) {
    Serial.println(String(++lTick));
  }
  handleCommand();
  yield();  // Allow the system to reset the watchdog timer
  delay(1000);

  irReceiver.handleIR();
}