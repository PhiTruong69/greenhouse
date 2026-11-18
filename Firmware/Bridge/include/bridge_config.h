// bridge_config.h – Cấu hình Bridge (ESP-NOW <-> USB Serial).
#pragma once
#include <Arduino.h>

// ---------- Serial tới Gateway PC ----------
#define HOST_BAUD     115200
#define HOST_LINE_MAX 200   // độ dài tối đa một dòng JSON lệnh

// ---------- ESP-NOW ----------
#define MAX_NODES          4    // nodeId hợp lệ: 1..MAX_NODES
#define CMD_ACK_TIMEOUT_MS 300  // chờ ACK của node cho mỗi lần gửi lệnh
#define CMD_MAX_RETRIES    3    // số lần gửi lại lệnh trước khi báo thất bại

// ---------- FreeRTOS ----------
#define APP_CORE      1    // core 1; core 0 dành cho WiFi/ESP-NOW
#define LINK_POLL_MS  10   // chu kỳ tối đa linkTask kiểm tra lệnh mới / gửi lại
#define HOST_POLL_MS  10   // chu kỳ tối đa hostTask đọc Serial

#define STACK_LINK    4096
#define STACK_HOST    6144 // ArduinoJson cần nhiều stack hơn

#define PRIO_LINK     3    // ACK/gửi lại phải đúng hạn
#define PRIO_HOST     2
#define EVENT_QUEUE_LEN 16
#define CMD_QUEUE_LEN   4
