// guard — a fault canary for wiring bring-up. Emits a fast heartbeat and, on every boot,
// the RESET REASON. A short that sags the 3.3 V rail trips the ESP32 brownout detector →
// it resets and re-boots printing "reset=BROWNOUT" (and the IDF also prints "Brownout
// detector was triggered"). A gross 5V→GND short trips the Pi's USB over-current → the
// device drops off the bus entirely. The Pi-side watcher (tools/fault_watch.py) turns all
// three — brownout, unexpected reboot, USB drop — into a loud alert. Reads no sensor pins.
#include <Arduino.h>
#include "esp_system.h"

static const char *reason_str(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:  return "POWERON";
    case ESP_RST_BROWNOUT: return "BROWNOUT";   // <- the smoking gun for a rail-sag short
    case ESP_RST_SW:       return "SW";
    case ESP_RST_PANIC:    return "PANIC";
    case ESP_RST_INT_WDT:  return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT:      return "WDT";
    case ESP_RST_EXT:      return "EXT";
    default:               return "OTHER";
  }
}

static uint32_t last = 0, seq = 0;

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.printf("GUARD boot  reset=%s\n", reason_str(esp_reset_reason()));
}

void loop() {
  const uint32_t now = millis();
  if (now - last >= 200) {           // 5 Hz heartbeat — a gap is obvious fast
    last = now;
    Serial.printf("HB %lu up=%lus\n", (unsigned long)seq++, (unsigned long)(now / 1000));
  }
}
