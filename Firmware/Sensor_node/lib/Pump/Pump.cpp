#include "Pump.h"
#include "node_config.h"

static volatile bool isOn = false;  // pumpTask ghi, sensorTask đọc
static uint32_t startMs = 0;
static uint32_t runMs = 0;

void pump_begin() {
  digitalWrite(PIN_PUMP, !PUMP_ON_LEVEL);
  pinMode(PIN_PUMP, OUTPUT);
}

void pump_set(bool on, uint32_t durationMs) {
  if (durationMs == 0 || durationMs > PUMP_MAX_ON_MS) durationMs = PUMP_MAX_ON_MS;
  isOn = on;
  startMs = millis();
  runMs = durationMs;
  digitalWrite(PIN_PUMP, on ? PUMP_ON_LEVEL : !PUMP_ON_LEVEL);
  Serial.printf("[PUMP] %s\n", on ? "ON" : "OFF");
}

bool pump_update() {
  if (isOn && millis() - startMs >= runMs) {
    pump_set(false);
    return true;
  }
  return false;
}

bool pump_isOn() { return isOn; }
