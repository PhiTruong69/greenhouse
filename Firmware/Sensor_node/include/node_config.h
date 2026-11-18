// node_config.h – Cấu hình Node 1 (Vườn). Chỉnh tại đây, không cần sửa trong lib.
#pragma once
#include <Arduino.h>

// ---------- Định danh ----------
#define NODE_ID 1

// ---------- Chân (GPIO ESP32-S3 DevKitC-1 N16R8; tránh GPIO35–37 vì PSRAM octal dùng) ----------
#define PIN_RS485_RX 18   // nối chân TXD của module TTL-RS485
#define PIN_RS485_TX 17   // nối chân RXD của module TTL-RS485
#define PIN_SOIL     1    // phải là chân ADC1 (GPIO1–10), ADC2 không dùng được khi bật radio
#define PIN_PUMP     5
#define PUMP_ON_LEVEL HIGH  // đổi thành LOW nếu relay kích mức thấp

// ---------- SHT20 RS485 (Modbus RTU) – kiểm tra bảng thanh ghi trong tài liệu của module ----------
#define SHT20_MODBUS_ADDR  1
#define SHT20_MODBUS_BAUD  9600
#define SHT20_MODBUS_FUNC  0x04    // 0x04 input register (loại XY-MD02); nhiều module khác dùng 0x03
#define SHT20_REG_TEMP     0x0001  // nhiệt độ x10, có dấu
#define SHT20_REG_HUM      0x0002  // độ ẩm x10
#define SHT20_TIMEOUT_MS   200

// ---------- Cảm biến độ ẩm đất (hiệu chỉnh theo giá trị soilRaw in ra Serial) ----------
#define SOIL_RAW_DRY 2900  // để ngoài không khí
#define SOIL_RAW_WET 1200  // cắm vào cốc nước

// ---------- Chu kỳ, ESP-NOW ----------
#define TELEMETRY_PERIOD_MS 10000
#define ACK_TIMEOUT_MS      300
#define MAX_RETRIES         3

// ---------- An toàn bơm ----------
#define PUMP_MAX_ON_MS 60000  // bơm tự tắt sau thời gian này nếu không có lệnh tắt

// ---------- FreeRTOS ----------
#define APP_CORE      1     // core 1; core 0 dành cho WiFi/ESP-NOW
#define LINK_POLL_MS  10    // chu kỳ tối đa linkTask kiểm tra gửi lại
#define PUMP_POLL_MS  100   // chu kỳ pumpTask kiểm tra hết giờ bơm

#define STACK_LINK    4096
#define STACK_PUMP    3072
#define STACK_SENSOR  4096

#define PRIO_LINK     3     // cao nhất: ACK/gửi lại phải đúng hạn
#define PRIO_PUMP     2
#define PRIO_SENSOR   1     // đọc Modbus có thể chặn tới 2 x SHT20_TIMEOUT_MS
