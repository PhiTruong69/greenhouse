// Node 2 – Điều khiển (ESP32-S3 DevKitC-1 N16R8)
// Relay quạt + đèn, WiFi + MQTT trực tiếp tới CoreIOT, nhận RPC và OTA firmware (T14).
//
// FreeRTOS – 1 task:
//   netTask : sở hữu WiFi/MQTT; xử lý RPC (bật/tắt relay), OTA và gửi trạng thái.
//   Relay chỉ được đổi trong RPC callback, tức cũng chạy trong netTask, nên không cần queue/mutex.
#include <Arduino.h>
#include "control_config.h"
#include "Relay.h"
#include "CoreIot.h"

// Duy trì kết nối CoreIOT và xử lý mọi bản tin đến.
static void netTask(void *) {
  for (;;) {
    coreiot_loop();
    vTaskDelay(pdMS_TO_TICKS(NET_POLL_MS));
  }
}

void setup() {
  Serial.begin(115200);
  Serial.printf("[BOOT] %s %s\n", FW_TITLE, FW_VERSION);
  relay_begin();
  coreiot_begin();
  xTaskCreatePinnedToCore(netTask, "net", STACK_NET, nullptr, PRIO_NET, nullptr, APP_CORE);
}

// Không dùng loop(): xoá task loop của Arduino, mọi việc chạy trong netTask.
void loop() {
  vTaskDelete(nullptr);
}
