// protocol.h – Gói tin ESP-NOW dùng chung giữa Node 1 và Bridge (T06).
// Mọi gói: header + payload + CRC16 (Modbus) ở 2 byte cuối, struct packed.
#pragma once
#include <stdint.h>
#include <stddef.h>

#define PROTO_VERSION   1
#define ESPNOW_CHANNEL  1   // Node và Bridge phải cùng kênh

enum MsgType : uint8_t {
  MSG_TELEMETRY = 1,  // Node -> Bridge, Bridge trả MSG_ACK
  MSG_ACK       = 2,  // hai chiều, xác nhận gói theo ackSeq
  MSG_CMD       = 3,  // Bridge -> Node, Node trả MSG_ACK
};

enum CmdId : uint8_t {
  CMD_SET_PUMP = 1,   // value: 0 tắt, 1 bật; durationS: 0 = mặc định tối đa
};

enum AckResult : uint8_t {
  ACK_OK          = 0,
  ACK_ERR_UNKNOWN = 1,  // lệnh không hỗ trợ
};

// Cờ trong TelemetryPkt.status
#define STATUS_MODBUS_OK  0x01  // đọc SHT20 RS485 thành công; nếu 0 thì bỏ qua airTemp/airHum

struct __attribute__((packed)) PktHeader {
  uint8_t  version;
  uint8_t  type;     // MsgType
  uint8_t  nodeId;   // node mà gói thuộc về (nguồn hoặc đích)
  uint16_t seq;      // tăng dần theo từng bên gửi
};

struct __attribute__((packed)) TelemetryPkt {
  PktHeader h;
  int16_t  airTemp_x10;  // °C x10
  uint16_t airHum_x10;   // %RH x10
  uint16_t soilRaw;      // ADC 12-bit
  uint8_t  soilPct;      // 0..100
  uint8_t  pumpOn;       // 0/1
  uint8_t  status;       // STATUS_*
  uint16_t crc;
};

struct __attribute__((packed)) AckPkt {
  PktHeader h;
  uint16_t ackSeq;  // seq của gói được xác nhận
  uint8_t  result;  // AckResult
  uint16_t crc;
};

struct __attribute__((packed)) CmdPkt {
  PktHeader h;
  uint8_t  cmd;        // CmdId
  uint8_t  value;
  uint16_t durationS;
  uint16_t crc;
};

// CRC16 Modbus (poly 0xA001, init 0xFFFF) – dùng cho cả gói ESP-NOW lẫn Modbus RTU
inline uint16_t proto_crc16(const uint8_t *buf, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= buf[i];
    for (int b = 0; b < 8; b++) crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
  }
  return crc;
}

template <typename T> inline void proto_seal(T &p) {
  p.crc = proto_crc16(reinterpret_cast<const uint8_t *>(&p), sizeof(T) - 2);
}

template <typename T> inline bool proto_check(const T &p) {
  return p.crc == proto_crc16(reinterpret_cast<const uint8_t *>(&p), sizeof(T) - 2);
}
