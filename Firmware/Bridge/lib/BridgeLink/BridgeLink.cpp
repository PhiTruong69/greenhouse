#include "BridgeLink.h"
#include "bridge_config.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// Một gói nhận được, chuyển từ callback ESP-NOW sang loop() qua queue
struct RxItem {
  uint8_t mac[6];
  uint8_t len;
  uint8_t data[32];
};

// Thông tin của từng node, đánh chỉ số theo nodeId
struct NodeInfo {
  bool    known;
  uint8_t mac[6];
  int32_t lastTelSeq;  // seq telemetry gần nhất, dùng để bỏ gói trùng
};

static QueueHandle_t rxQueue;
static NodeInfo      nodes[MAX_NODES + 1];
static uint16_t      txSeq = 0;

// Lệnh đang chờ ACK (mỗi lúc chỉ một lệnh)
static bool     pending = false;
static CmdPkt   pendingCmd;
static uint32_t pendingId = 0;
static uint8_t  retries = 0;
static uint32_t lastTxMs = 0;

// Thêm một peer ESP-NOW (nếu chưa có).
static void addPeer(const uint8_t *mac) {
  if (esp_now_is_peer_exist(mac)) return;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
}

// Ghi nhớ (hoặc cập nhật) MAC của node.
static void learnNode(uint8_t nodeId, const uint8_t *mac) {
  NodeInfo &n = nodes[nodeId];
  if (n.known && memcmp(n.mac, mac, 6) == 0) return;
  memcpy(n.mac, mac, 6);
  n.known = true;
  addPeer(mac);
}

// Callback chạy trong task WiFi: chỉ copy gói vào queue.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
static void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  const uint8_t *mac = info->src_addr;
#else
static void onRecv(const uint8_t *mac, const uint8_t *data, int len) {
#endif
  if (len <= 0 || len > (int)sizeof(RxItem::data)) return;
  RxItem it;
  memcpy(it.mac, mac, 6);
  it.len = len;
  memcpy(it.data, data, len);
  xQueueSend(rxQueue, &it, 0);
}

// Gửi ACK cho telemetry của node.
static void sendAck(uint8_t nodeId, uint16_t ackSeq) {
  AckPkt a = {};
  a.h = {PROTO_VERSION, MSG_ACK, nodeId, ++txSeq};
  a.ackSeq = ackSeq;
  a.result = ACK_OK;
  proto_seal(a);
  esp_now_send(nodes[nodeId].mac, (const uint8_t *)&a, sizeof(a));
}

// Xử lý một gói nhận được. Trả về true nếu tạo ra sự kiện.
static bool handlePacket(const RxItem &it, LinkEvent &ev) {
  if (it.len < sizeof(PktHeader)) return false;
  PktHeader h;
  memcpy(&h, it.data, sizeof(h));
  if (h.version != PROTO_VERSION || h.nodeId < 1 || h.nodeId > MAX_NODES) return false;

  // Telemetry từ node: luôn ACK, chỉ báo lên nếu không trùng
  if (h.type == MSG_TELEMETRY && it.len == sizeof(TelemetryPkt)) {
    TelemetryPkt t;
    memcpy(&t, it.data, sizeof(t));
    if (!proto_check(t)) return false;
    learnNode(h.nodeId, it.mac);
    sendAck(h.nodeId, t.h.seq);

    NodeInfo &n = nodes[h.nodeId];
    if ((int32_t)t.h.seq == n.lastTelSeq) return false;
    n.lastTelSeq = t.h.seq;

    ev = {};
    ev.type = EV_TELEMETRY;
    ev.nodeId = h.nodeId;
    ev.tel = t;
    return true;
  }

  // ACK của node cho lệnh đang chờ
  if (h.type == MSG_ACK && it.len == sizeof(AckPkt)) {
    AckPkt a;
    memcpy(&a, it.data, sizeof(a));
    if (!proto_check(a)) return false;
    if (!pending || h.nodeId != pendingCmd.h.nodeId || a.ackSeq != pendingCmd.h.seq) return false;
    pending = false;

    ev = {};
    ev.type = EV_CMD_RESULT;
    ev.nodeId = h.nodeId;
    ev.requestId = pendingId;
    ev.ok = true;
    ev.result = a.result;
    return true;
  }
  return false;
}

void link_begin() {
  rxQueue = xQueueCreate(16, sizeof(RxItem));
  for (auto &n : nodes) n = {false, {0}, -1};

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) {
    delay(1000);
    ESP.restart();
  }
  esp_now_register_recv_cb(onRecv);
}

LinkSendResult link_sendCommand(uint8_t nodeId, uint8_t cmd, uint8_t value, uint16_t durationS, uint32_t requestId) {
  if (nodeId < 1 || nodeId > MAX_NODES || !nodes[nodeId].known) return LINK_ERR_UNKNOWN_NODE;
  if (pending) return LINK_ERR_BUSY;

  pendingCmd = {};
  pendingCmd.h = {PROTO_VERSION, MSG_CMD, nodeId, ++txSeq};
  pendingCmd.cmd = cmd;
  pendingCmd.value = value;
  pendingCmd.durationS = durationS;
  proto_seal(pendingCmd);

  pending = true;
  pendingId = requestId;
  retries = 0;
  lastTxMs = millis();
  esp_now_send(nodes[nodeId].mac, (const uint8_t *)&pendingCmd, sizeof(pendingCmd));
  return LINK_SENT;
}

bool link_poll(LinkEvent &ev, TickType_t wait) {
  // 1. Gói nhận được
  RxItem it;
  while (xQueueReceive(rxQueue, &it, wait) == pdTRUE) {
    wait = 0;  // chỉ chờ gói đầu tiên, các gói sau lấy ngay nếu có
    if (handlePacket(it, ev)) return true;
  }

  // 2. Lệnh quá hạn chờ ACK: gửi lại hoặc báo thất bại
  if (pending && millis() - lastTxMs >= CMD_ACK_TIMEOUT_MS) {
    uint8_t nodeId = pendingCmd.h.nodeId;
    if (retries < CMD_MAX_RETRIES) {
      retries++;
      lastTxMs = millis();
      esp_now_send(nodes[nodeId].mac, (const uint8_t *)&pendingCmd, sizeof(pendingCmd));
    } else {
      pending = false;
      ev = {};
      ev.type = EV_CMD_RESULT;
      ev.nodeId = nodeId;
      ev.requestId = pendingId;
      ev.ok = false;
      return true;
    }
  }
  return false;
}
