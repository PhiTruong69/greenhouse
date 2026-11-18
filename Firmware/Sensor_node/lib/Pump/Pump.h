// Pump – Điều khiển relay bơm, có giới hạn thời gian chạy để an toàn.
#pragma once
#include <Arduino.h>

// Đặt relay về trạng thái tắt rồi mới cấu hình OUTPUT (tránh bơm chạy lúc khởi động).
void pump_begin();

// Bật/tắt bơm. Khi bật, bơm tự tắt sau durationMs (tối đa PUMP_MAX_ON_MS).
// durationMs = 0 nghĩa là dùng PUMP_MAX_ON_MS.
void pump_set(bool on, uint32_t durationMs = 0);

// Gọi trong loop(). Trả về true nếu vừa tự tắt bơm do hết thời gian.
bool pump_update();

// Trạng thái hiện tại của bơm.
bool pump_isOn();
