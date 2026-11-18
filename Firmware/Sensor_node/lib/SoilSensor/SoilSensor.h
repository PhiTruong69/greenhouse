// SoilSensor – Cảm biến độ ẩm đất điện dung v2.0 (ngõ ra analog, đất càng khô giá trị càng cao).
#pragma once
#include <Arduino.h>

// Cấu hình ADC 12-bit cho chân cảm biến.
void soil_begin();

// Đọc giá trị ADC thô (trung bình 8 lần để giảm nhiễu).
uint16_t soil_readRaw();

// Đổi giá trị thô sang % độ ẩm (0..100) theo SOIL_RAW_DRY / SOIL_RAW_WET trong node_config.h.
uint8_t soil_toPercent(uint16_t raw);
