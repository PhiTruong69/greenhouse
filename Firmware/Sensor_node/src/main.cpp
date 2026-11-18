// Node 1 – Vườn (ESP32-S3 DevKitC-1)
// Đọc SHT20 RS485 + độ ẩm đất, gửi lên Bridge qua ESP-NOW mỗi 10 s, nhận lệnh bật/tắt bơm.
//
// FreeRTOS – 3 task, mỗi phần cứng chỉ do một task dùng:
//   sensorTask : đọc cảm biến mỗi TELEMETRY_PERIOD_MS (hoặc khi được báo) -> telemetryQueue
//   linkTask   : sở hữu ESP-NOW; gửi telemetry, ACK, gửi lại, nhận lệnh   -> pumpQueue
//   pumpTask   : sở hữu relay; thực hiện lệnh, tự tắt khi hết giờ          -> báo sensorTask
#include <Arduino.h>
#include "node_config.h"
#include "SHT20.h"
#include "SoilSensor.h"
#include "Pump.h"
#include "NodeLink.h"

static QueueHandle_t telemetryQueue;  // sensorTask -> linkTask (TelemetryPkt)
static QueueHandle_t pumpQueue;       // linkTask   -> pumpTask (CmdPkt)
static TaskHandle_t  sensorTaskHandle;

// Đọc cảm biến theo chu kỳ, hoặc ngay khi bơm đổi trạng thái (task notification).
static void sensorTask(void *) {
  for (;;) {
    TelemetryPkt p = {};
    int16_t  t10 = 0;
    uint16_t h10 = 0;
    if (sht20_read(t10, h10)) {
      p.airTemp_x10 = t10;
      p.airHum_x10 = h10;
      p.status |= STATUS_MODBUS_OK;
    }
    p.soilRaw = soil_readRaw();
    p.soilPct = soil_toPercent(p.soilRaw);
    p.pumpOn = pump_isOn();
    xQueueOverwrite(telemetryQueue, &p);  // chỉ giữ số liệu mới nhất

    Serial.printf("[SENSOR] T=%.1f H=%.1f soilRaw=%u soil=%u%% pump=%u modbus=%s lost=%lu\n",
                  t10 / 10.0, h10 / 10.0, p.soilRaw, p.soilPct, p.pumpOn,
                  (p.status & STATUS_MODBUS_OK) ? "ok" : "FAIL", (unsigned long)link_lostCount());

    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(TELEMETRY_PERIOD_MS));
  }
}

// Giao tiếp ESP-NOW: gửi telemetry, nhận lệnh, trả ACK, gửi lại khi chưa có ACK.
static void linkTask(void *) {
  for (;;) {
    TelemetryPkt p;
    if (xQueueReceive(telemetryQueue, &p, 0) == pdTRUE) link_sendTelemetry(p);

    CmdPkt cmd;
    if (link_receiveCommand(cmd, pdMS_TO_TICKS(LINK_POLL_MS))) {
      Serial.printf("[RX] cmd=%u value=%u duration=%us\n", cmd.cmd, cmd.value, cmd.durationS);
      if (cmd.cmd == CMD_SET_PUMP) {
        xQueueSend(pumpQueue, &cmd, 0);
        link_sendAck(cmd.h.seq, ACK_OK);
      } else {
        link_sendAck(cmd.h.seq, ACK_ERR_UNKNOWN);
      }
    }

    link_update();
  }
}

// Điều khiển bơm theo lệnh, tự tắt khi hết giờ; báo sensorTask gửi trạng thái mới.
static void pumpTask(void *) {
  for (;;) {
    CmdPkt cmd;
    if (xQueueReceive(pumpQueue, &cmd, pdMS_TO_TICKS(PUMP_POLL_MS)) == pdTRUE) {
      pump_set(cmd.value != 0, (uint32_t)cmd.durationS * 1000);
      xTaskNotifyGive(sensorTaskHandle);
    }
    if (pump_update()) xTaskNotifyGive(sensorTaskHandle);
  }
}

void setup() {
  Serial.begin(115200);
  pump_begin();
  soil_begin();
  sht20_begin();
  link_begin();

  telemetryQueue = xQueueCreate(1, sizeof(TelemetryPkt));
  pumpQueue = xQueueCreate(4, sizeof(CmdPkt));

  // Tạo sensorTask trước để pumpTask luôn có handle hợp lệ khi gọi xTaskNotifyGive
  xTaskCreatePinnedToCore(sensorTask, "sensor", STACK_SENSOR, nullptr, PRIO_SENSOR, &sensorTaskHandle, APP_CORE);
  xTaskCreatePinnedToCore(pumpTask, "pump", STACK_PUMP, nullptr, PRIO_PUMP, nullptr, APP_CORE);
  xTaskCreatePinnedToCore(linkTask, "link", STACK_LINK, nullptr, PRIO_LINK, nullptr, APP_CORE);
}

// Không dùng loop(): xoá task loop của Arduino, mọi việc chạy trong các task trên.
void loop() {
  vTaskDelete(nullptr);
}
