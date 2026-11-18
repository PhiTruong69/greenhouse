// Relay – Điều khiển relay 2 kênh: quạt và đèn.
#pragma once
#include <Arduino.h>

enum RelayId : uint8_t {
  RELAY_FAN = 0,
  RELAY_LIGHT = 1,
};

// Đặt cả 2 relay về trạng thái tắt rồi mới cấu hình OUTPUT (tránh kích lúc khởi động).
void relay_begin();

// Bật/tắt một relay.
void relay_set(RelayId id, bool on);

// Trạng thái hiện tại của một relay.
bool relay_get(RelayId id);
