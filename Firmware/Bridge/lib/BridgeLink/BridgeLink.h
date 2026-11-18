// BridgeLink – Phía ESP-NOW của Bridge theo protocol.h (không kết nối WiFi).
//  - Nhận telemetry từ node, tự trả ACK, bỏ gói trùng (node gửi lại khi mất ACK).
//  - Tự học MAC của từng node từ gói telemetry đầu tiên.
//  - Gửi lệnh xuống node, chờ ACK, gửi lại tối đa CMD_MAX_RETRIES lần.
// Các hàm link_* chỉ được gọi từ một task duy nhất (linkTask).
#pragma once
#include <Arduino.h>
#include "protocol.h"

enum LinkEventType : uint8_t {
  EV_TELEMETRY,   // có telemetry mới từ node
  EV_CMD_RESULT,  // lệnh đã được node ACK, hoặc hết lần gửi lại
  EV_CMD_REJECTED // lệnh không gửi được (result = LinkSendResult); main tạo ra từ link_sendCommand
};

struct LinkEvent {
  LinkEventType type;
  uint8_t       nodeId;
  TelemetryPkt  tel;        // EV_TELEMETRY
  uint32_t      requestId;  // EV_CMD_RESULT: id do Gateway đặt
  bool          ok;         // EV_CMD_RESULT: true nếu node đã ACK
  uint8_t       result;     // EV_CMD_RESULT: AckResult của node (khi ok)
};

enum LinkSendResult : uint8_t {
  LINK_SENT = 0,
  LINK_ERR_UNKNOWN_NODE,  // node chưa gửi telemetry lần nào nên chưa biết MAC
  LINK_ERR_BUSY,          // đang chờ ACK của lệnh trước
};

// Bật radio ở chế độ STA, đặt kênh ESPNOW_CHANNEL, khởi tạo ESP-NOW.
void link_begin();

// Gửi lệnh xuống node. Kết quả cuối cùng trả về qua sự kiện EV_CMD_RESULT.
LinkSendResult link_sendCommand(uint8_t nodeId, uint8_t cmd, uint8_t value, uint16_t durationS, uint32_t requestId);

// Gọi lặp trong task. Chờ tối đa `wait` tick nếu chưa có gói nào.
// Trả về true và điền ev nếu có sự kiện mới.
bool link_poll(LinkEvent &ev, TickType_t wait = 0);
