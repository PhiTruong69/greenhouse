#include "Relay.h"
#include "control_config.h"

static const uint8_t pins[2] = {PIN_RELAY_FAN, PIN_RELAY_LIGHT};
static bool state[2] = {false, false};

void relay_begin() {
  for (uint8_t pin : pins) {
    digitalWrite(pin, !RELAY_ON_LEVEL);
    pinMode(pin, OUTPUT);
  }
}

void relay_set(RelayId id, bool on) {
  state[id] = on;
  digitalWrite(pins[id], on ? RELAY_ON_LEVEL : !RELAY_ON_LEVEL);
  Serial.printf("[RELAY] %s %s\n", id == RELAY_FAN ? "fan" : "light", on ? "ON" : "OFF");
}

bool relay_get(RelayId id) { return state[id]; }
