// SHT20 – Đọc nhiệt độ, độ ẩm từ cảm biến SHT20 RS485 qua Modbus RTU (module TTL-RS485 tự đảo chiều).
// Mã hàm và địa chỉ thanh ghi khác nhau tuỳ hãng, chỉnh trong node_config.h.
#pragma once
#include <Arduino.h>

// Khởi tạo UART (Serial1) nối với module TTL-RS485.
void sht20_begin();

// Đọc thanh ghi nhiệt độ và độ ẩm. Trả về true nếu cả hai lần đọc đều có phản hồi đúng CRC.
// Giá trị trả ra nhân 10 (273 = 27.3).
bool sht20_read(int16_t &temp_x10, uint16_t &hum_x10);
