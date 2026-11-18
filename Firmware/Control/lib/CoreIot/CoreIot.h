// CoreIot – Kết nối WiFi + MQTT tới CoreIOT bằng ThingsBoard SDK.
//  - RPC: setFan, setLight (bật/tắt), getFan, getLight (cho widget switch đọc trạng thái).
//  - Telemetry: fan, light (khi thay đổi và mỗi STATE_PERIOD_MS).
//  - OTA (T14): báo current_fw_title/version, nhận gói mới, tải theo chunk,
//    kiểm checksum, ghi vào phân vùng OTA còn lại rồi khởi động lại.
// Các hàm coreiot_* chỉ được gọi từ một task duy nhất (netTask).
#pragma once
#include <Arduino.h>

// Bắt đầu kết nối WiFi (tự kết nối lại khi rớt).
void coreiot_begin();

// Gọi lặp trong task: kết nối lại MQTT nếu cần, xử lý RPC/OTA, gửi trạng thái.
void coreiot_loop();
