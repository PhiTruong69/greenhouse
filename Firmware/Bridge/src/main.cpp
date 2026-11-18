// Bridge (NodeMCU ESP32-S3 N16R8, cắm USB vào PC Gateway)
// Chuyển telemetry ESP-NOW từ node thành JSON qua Serial, và lệnh JSON từ Gateway xuống node.
//
// FreeRTOS – 2 task, mỗi phần cứng chỉ do một task dùng:
//   hostTask : sở hữu Serial; đọc lệnh JSON -> cmdQueue, in sự kiện từ eventQueue
//   linkTask : sở hữu ESP-NOW; lấy lệnh từ cmdQueue gửi xuống node, đẩy sự kiện -> eventQueue
#include <Arduino.h>
#include "bridge_config.h"
#include "BridgeLink.h"
#include "HostLink.h"

static QueueHandle_t cmdQueue;    // hostTask -> linkTask (HostCommand)
static QueueHandle_t eventQueue;  // linkTask -> hostTask (LinkEvent)

// Đọc lệnh từ Gateway và in mọi sự kiện ra Serial (task duy nhất ghi Serial).
static void hostTask(void *) {
  for (;;) {
    HostCommand c;
    while (host_readCommand(c)) xQueueSend(cmdQueue, &c, 0);

    LinkEvent ev;
    if (xQueueReceive(eventQueue, &ev, pdMS_TO_TICKS(HOST_POLL_MS)) != pdTRUE) continue;

    if (ev.type == EV_TELEMETRY) host_printTelemetry(ev.nodeId, ev.tel);
    if (ev.type == EV_CMD_RESULT) host_printCmdResult(ev.nodeId, ev.requestId, ev.ok, ev.result);
    if (ev.type == EV_CMD_REJECTED)
      host_printError(ev.requestId, ev.result == LINK_ERR_BUSY ? "busy" : "unknown node");
  }
}

// Gửi lệnh xuống node và chuyển telemetry / kết quả lệnh lên hostTask.
static void linkTask(void *) {
  for (;;) {
    HostCommand c;
    if (xQueueReceive(cmdQueue, &c, 0) == pdTRUE) {
      LinkSendResult r = link_sendCommand(c.nodeId, c.cmd, c.value, c.durationS, c.requestId);
      if (r != LINK_SENT) {
        LinkEvent ev = {};
        ev.type = EV_CMD_REJECTED;
        ev.nodeId = c.nodeId;
        ev.requestId = c.requestId;
        ev.result = r;
        xQueueSend(eventQueue, &ev, 0);
      }
    }

    LinkEvent ev;
    if (link_poll(ev, pdMS_TO_TICKS(LINK_POLL_MS))) xQueueSend(eventQueue, &ev, 0);
  }
}

void setup() {
  host_begin();
  link_begin();
  host_printBoot();

  cmdQueue = xQueueCreate(CMD_QUEUE_LEN, sizeof(HostCommand));
  eventQueue = xQueueCreate(EVENT_QUEUE_LEN, sizeof(LinkEvent));

  xTaskCreatePinnedToCore(linkTask, "link", STACK_LINK, nullptr, PRIO_LINK, nullptr, APP_CORE);
  xTaskCreatePinnedToCore(hostTask, "host", STACK_HOST, nullptr, PRIO_HOST, nullptr, APP_CORE);
}

// Không dùng loop(): xoá task loop của Arduino, mọi việc chạy trong các task trên.
void loop() {
  vTaskDelete(nullptr);
}
