#include "SHT20.h"
#include "node_config.h"
#include "protocol.h"  // dùng chung proto_crc16 (CRC16 Modbus)

// Đọc một thanh ghi 16-bit bằng mã hàm SHT20_MODBUS_FUNC. Trả về false nếu lỗi/timeout.
static bool readRegister(uint16_t reg, uint16_t &value) {
  // Khung yêu cầu: addr, func, reg_hi, reg_lo, số reg (1), CRC (byte thấp trước)
  uint8_t req[8] = {SHT20_MODBUS_ADDR, SHT20_MODBUS_FUNC, (uint8_t)(reg >> 8), (uint8_t)reg, 0x00, 0x01, 0, 0};
  uint16_t crc = proto_crc16(req, 6);
  req[6] = crc & 0xFF;
  req[7] = crc >> 8;

  while (Serial1.available()) Serial1.read();  // bỏ dữ liệu cũ còn trong buffer
  Serial1.write(req, sizeof(req));
  Serial1.flush();

  // Khung phản hồi: addr, func, 2 byte data, V_hi, V_lo, CRC_lo, CRC_hi
  uint8_t resp[7];
  if (Serial1.readBytes(resp, sizeof(resp)) != sizeof(resp)) return false;
  if (resp[0] != SHT20_MODBUS_ADDR || resp[1] != SHT20_MODBUS_FUNC || resp[2] != 2) return false;
  if (proto_crc16(resp, 5) != (uint16_t)(resp[5] | (resp[6] << 8))) return false;

  value = (uint16_t)((resp[3] << 8) | resp[4]);
  return true;
}

void sht20_begin() {
  Serial1.begin(SHT20_MODBUS_BAUD, SERIAL_8N1, PIN_RS485_RX, PIN_RS485_TX);
  Serial1.setTimeout(SHT20_TIMEOUT_MS);
}

bool sht20_read(int16_t &temp_x10, uint16_t &hum_x10) {
  uint16_t t, h;
  if (!readRegister(SHT20_REG_TEMP, t)) return false;
  if (!readRegister(SHT20_REG_HUM, h)) return false;
  temp_x10 = (int16_t)t;  // nhiệt độ có thể âm (bù 2)
  hum_x10 = h;
  return true;
}
