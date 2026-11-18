// NodeLink – Liên lạc ESP-NOW với Bridge theo protocol.h (không kết nối WiFi).
//  - Gửi telemetry, chờ ACK, gửi lại tối đa MAX_RETRIES lần.
//  - Nhận lệnh từ Bridge, bỏ lệnh trùng (Bridge gửi lại khi mất ACK).
//  - Ban đầu gửi broadcast, tự học MAC của Bridge từ gói đầu tiên nhận được.
// Các hàm link_* (trừ link_lostCount) chỉ được gọi từ một task duy nhất (linkTask).
#pragma once
#include <Arduino.h>
#include "protocol.h"

// Bật radio ở chế độ STA, đặt kênh ESPNOW_CHANNEL, khởi tạo ESP-NOW.
void link_begin();

// Gửi telemetry. Hàm tự điền header (seq) và CRC, chỉ cần điền phần dữ liệu.
void link_sendTelemetry(TelemetryPkt &pkt);

// Xử lý các gói đã nhận, chờ tối đa `wait` tick nếu queue đang trống.
// Trả về true và điền cmd nếu có lệnh mới cần thực hiện.
// Sau khi thực hiện, gọi link_sendAck(cmd.h.seq, kết quả).
bool link_receiveCommand(CmdPkt &cmd, TickType_t wait = 0);

// Gửi ACK cho một lệnh đã nhận.
void link_sendAck(uint16_t seq, uint8_t result);

// Gọi trong loop(): gửi lại telemetry nếu quá ACK_TIMEOUT_MS chưa có ACK.
void link_update();

// Số gói telemetry bị mất (hết số lần gửi lại mà không có ACK).
uint32_t link_lostCount();
