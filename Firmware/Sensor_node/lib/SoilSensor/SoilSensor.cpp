#include "SoilSensor.h"
#include "node_config.h"

void soil_begin() {
  analogReadResolution(12);
  pinMode(PIN_SOIL, INPUT);
}

uint16_t soil_readRaw() {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) {
    sum += analogRead(PIN_SOIL);
    delay(2);
  }
  return sum / 8;
}

uint8_t soil_toPercent(uint16_t raw) {
  long pct = map(raw, SOIL_RAW_DRY, SOIL_RAW_WET, 0, 100);
  return (uint8_t)constrain(pct, 0, 100);
}
