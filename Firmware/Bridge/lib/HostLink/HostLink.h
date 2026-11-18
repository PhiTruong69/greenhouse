// HostLink – Giao tiếp với Gateway PC qua USB Serial, mỗi bản tin là một dòng JSON.
// Các hàm host_* chỉ được gọi từ một task duy nhất (hostTask) để các dòng JSON không bị lẫn.
//
// Bridge -> PC:
//   {"type":"boot","mac":"AA:BB:..","channel":1}
//   {"type":"telemetry","node":1,"seq":12,"temperature":27.3,"humidity":65.1,
//    "soil_raw":2100,"soil":47,"pump":0}          (temperature/humidity vắng mặt nếu Modbus lỗi)
//   {"type":"cmd_result","node":1,"id":42,"ok":true,"result":0}
//   {"type":"error","id":42,"msg":"unknown node"}
// PC -> Bridge:
//   {"cmd":"setPump","node":1,"value":1,"duration":30,"id":42}
#pragma once
#include <Arduino.h>
#include "protocol.h"

// Lệnh nhận từ Gateway, đã đổi sang mã của protocol.h
struct HostCommand {
  uint8_t  nodeId;
  uint8_t  cmd;        // CmdId
  uint8_t  value;
  uint16_t durationS;
  uint32_t requestId;  // "id" của Gateway, được trả lại trong cmd_result/error
};

// Mở Serial tới Gateway.
void host_begin();

// Đọc Serial không chặn. Trả về true khi đã nhận đủ một dòng JSON lệnh hợp lệ.
// Dòng sai định dạng sẽ được báo bằng bản tin "error".
bool host_readCommand(HostCommand &c);

// Báo Bridge vừa khởi động (kèm MAC và kênh ESP-NOW).
void host_printBoot();

// In telemetry của node thành một dòng JSON.
void host_printTelemetry(uint8_t nodeId, const TelemetryPkt &t);

// In kết quả lệnh: ok=false nghĩa là node không trả ACK sau khi gửi lại.
void host_printCmdResult(uint8_t nodeId, uint32_t requestId, bool ok, uint8_t result);

// In bản tin lỗi.
void host_printError(uint32_t requestId, const char *msg);
