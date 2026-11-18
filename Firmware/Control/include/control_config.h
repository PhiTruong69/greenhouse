// control_config.h – Cấu hình Node 2 (Điều khiển). Chỉnh tại đây, không cần sửa trong lib.
// WiFi và token nằm riêng trong secrets.h (không đưa lên git).
#pragma once
#include <Arduino.h>

// ---------- Firmware (OTA so sánh 2 giá trị này với gói trên CoreIOT) ----------
#define FW_TITLE   "node2-control"  // phải trùng "Title" của OTA package trên CoreIOT
#define FW_VERSION "1.1.0"          // tăng mỗi lần build bản mới để OTA

// ---------- CoreIOT (nền ThingsBoard) ----------
#define COREIOT_HOST "app.coreiot.io"
#define COREIOT_PORT 1883

// ---------- Chân relay (GPIO ESP32-S3 DevKitC-1 N16R8; tránh GPIO35–37 vì PSRAM octal) ----------
#define PIN_RELAY_FAN    4
#define PIN_RELAY_LIGHT  5
#define RELAY_ON_LEVEL   LOW   // relay opto 5V thường kích mức thấp; đổi thành HIGH nếu ngược

// ---------- Chu kỳ ----------
#define STATE_PERIOD_MS      30000  // gửi lại trạng thái quạt/đèn định kỳ (ngoài lúc thay đổi)
#define RECONNECT_PERIOD_MS  5000   // thời gian chờ giữa các lần thử kết nối MQTT

// ---------- OTA ----------
#define OTA_PACKET_SIZE    4096  // kích thước mỗi chunk tải về
#define OTA_FAIL_RETRIES   12    // số lần thử lại một chunk lỗi

// ---------- FreeRTOS ----------
#define APP_CORE     1     // core 1; core 0 dành cho WiFi
#define STACK_NET    8192  // MQTT + JSON + OTA
#define PRIO_NET     2
#define NET_POLL_MS  10
