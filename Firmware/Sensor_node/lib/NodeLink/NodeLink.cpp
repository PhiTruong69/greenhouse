#include "NodeLink.h"
#include "node_config.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// Một gói nhận được, chuyển từ callback ESP-NOW sang loop() qua queue
struct RxItem {
  uint8_t mac[6];
  uint8_t len;
  uint8_t data[32];
};

static QueueHandle_t rxQueue;
static uint8_t  bridgeMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  // broadcast cho tới khi biết Bridge
static bool     bridgeKnown = false;

static uint16_t     txSeq = 0;
static TelemetryPkt pendingPkt;   // gói telemetry đang chờ ACK
static bool         pending = false;
static uint8_t      retries = 0;
static uint32_t     lastTxMs = 0;
static uint32_t     lost = 0;
static int32_t      lastCmdSeq = -1;

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

// Ghi nhớ MAC Bridge từ gói hợp lệ đầu tiên, sau đó gửi unicast.
static void learnBridge(const uint8_t *mac) {
  if (bridgeKnown) return;
  memcpy(bridgeMac, mac, 6);
  addPeer(bridgeMac);
  bridgeKnown = true;
  Serial.printf("[LINK] Bridge %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
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

void link_begin() {
  rxQueue = xQueueCreate(8, sizeof(RxItem));

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) {
    Serial.println("[LINK] ESP-NOW init failed, restart");
    delay(1000);
    ESP.restart();
  }
  esp_now_register_recv_cb(onRecv);
  addPeer(bridgeMac);

  Serial.printf("[LINK] Node %d, MAC %s, channel %d\n", NODE_ID, WiFi.macAddress().c_str(), ESPNOW_CHANNEL);
}

void link_sendTelemetry(TelemetryPkt &pkt) {
  if (pending) lost++;  // gói trước chưa có ACK, bị thay bằng gói mới

  pkt.h = {PROTO_VERSION, MSG_TELEMETRY, NODE_ID, ++txSeq};
  proto_seal(pkt);

  pendingPkt = pkt;
  pending = true;
  retries = 0;
  lastTxMs = millis();
  esp_now_send(bridgeMac, (const uint8_t *)&pkt, sizeof(pkt));
}

void link_sendAck(uint16_t seq, uint8_t result) {
  AckPkt a = {};
  a.h = {PROTO_VERSION, MSG_ACK, NODE_ID, ++txSeq};
  a.ackSeq = seq;
  a.result = result;
  proto_seal(a);
  esp_now_send(bridgeMac, (const uint8_t *)&a, sizeof(a));
}

bool link_receiveCommand(CmdPkt &cmd, TickType_t wait) {
  RxItem it;
  while (xQueueReceive(rxQueue, &it, wait) == pdTRUE) {
    wait = 0;  // chỉ chờ gói đầu tiên, các gói sau lấy ngay nếu có
    if (it.len < sizeof(PktHeader)) continue;
    PktHeader h;
    memcpy(&h, it.data, sizeof(h));
    if (h.version != PROTO_VERSION || h.nodeId != NODE_ID) continue;

    // ACK cho telemetry
    if (h.type == MSG_ACK && it.len == sizeof(AckPkt)) {
      AckPkt a;
      memcpy(&a, it.data, sizeof(a));
      if (!proto_check(a)) continue;
      learnBridge(it.mac);
      if (pending && a.ackSeq == pendingPkt.h.seq) pending = false;
    }

    // Lệnh từ Bridge
    else if (h.type == MSG_CMD && it.len == sizeof(CmdPkt)) {
      memcpy(&cmd, it.data, sizeof(cmd));
      if (!proto_check(cmd)) continue;
      learnBridge(it.mac);
      if ((int32_t)cmd.h.seq == lastCmdSeq) {  // lệnh trùng: chỉ ACK lại
        link_sendAck(cmd.h.seq, ACK_OK);
        continue;
      }
      lastCmdSeq = cmd.h.seq;
      return true;
    }
  }
  return false;
}

void link_update() {
  if (!pending || millis() - lastTxMs < ACK_TIMEOUT_MS) return;

  if (retries < MAX_RETRIES) {
    retries++;
    lastTxMs = millis();
    esp_now_send(bridgeMac, (const uint8_t *)&pendingPkt, sizeof(pendingPkt));
  } else {
    pending = false;
    lost++;
    Serial.printf("[LINK] seq=%u no ACK\n", pendingPkt.h.seq);
  }
}

uint32_t link_lostCount() { return lost; }
