#include "CoreIot.h"
#include "control_config.h"
#include "secrets.h"
#include "Relay.h"
#include <WiFi.h>
#include <Arduino_MQTT_Client.h>
#include <Server_Side_RPC.h>
#include <OTA_Firmware_Update.h>
#include <Espressif_Updater.h>
#include <ThingsBoard.h>

static constexpr uint16_t MAX_MSG_SIZE = 512;  // buffer MQTT; SDK tự nới khi tải chunk OTA
static constexpr uint8_t  RPC_COUNT = 4;

static WiFiClient          wifiClient;
static Arduino_MQTT_Client mqttClient(wifiClient);
static Server_Side_RPC<RPC_COUNT, RPC_COUNT> rpc;
static OTA_Firmware_Update<> ota;
static Espressif_Updater<>   updater;  // ghi firmware vào phân vùng OTA còn lại
static const std::array<IAPI_Implementation *, 2U> apis = {&rpc, &ota};
static ThingsBoard tb(mqttClient, MAX_MSG_SIZE, MAX_MSG_SIZE, Default_Max_Stack_Size, apis);

static bool     subscribed = false;  // SDK tự đăng ký lại topic khi kết nối lại, chỉ cần làm 1 lần
static bool     stateDirty = true;
static uint32_t lastStateMs = 0;
static uint32_t lastConnectTryMs = 0;

// ---------- RPC ----------

// Đọc giá trị bật/tắt từ params RPC. Chấp nhận: true/false, 1/0, {"value":..} hoặc {"enabled":..}.
static bool parseOnOff(const JsonVariantConst &data) {
  if (data.is<bool>()) return data.as<bool>();
  if (data.is<int>()) return data.as<int>() != 0;
  if (data.containsKey("value")) return parseOnOff(data["value"]);
  if (data.containsKey("enabled")) return parseOnOff(data["enabled"]);
  return false;
}

// setFan: bật/tắt quạt, trả về trạng thái mới.
static void onSetFan(const JsonVariantConst &data, JsonDocument &response) {
  relay_set(RELAY_FAN, parseOnOff(data));
  stateDirty = true;
  response.set(relay_get(RELAY_FAN));
}

// setLight: bật/tắt đèn, trả về trạng thái mới.
static void onSetLight(const JsonVariantConst &data, JsonDocument &response) {
  relay_set(RELAY_LIGHT, parseOnOff(data));
  stateDirty = true;
  response.set(relay_get(RELAY_LIGHT));
}

// getFan / getLight: widget switch trên dashboard gọi để hiển thị đúng trạng thái.
static void onGetFan(const JsonVariantConst &, JsonDocument &response) { response.set(relay_get(RELAY_FAN)); }
static void onGetLight(const JsonVariantConst &, JsonDocument &response) { response.set(relay_get(RELAY_LIGHT)); }

// ---------- OTA ----------

// Bắt đầu tải firmware mới.
static void onOtaStart() { Serial.println("[OTA] start download"); }

// Tiến độ tải (byte đã nhận / tổng).
static void onOtaProgress(const size_t &current, const size_t &total) {
  Serial.printf("[OTA] %u / %u (%.0f%%)\n", (unsigned)current, (unsigned)total, current * 100.0f / total);
}

// Tải xong: success=true nghĩa là checksum đúng và đã ghi xong, khởi động lại để chạy bản mới.
static void onOtaFinished(const bool &success) {
  if (success) {
    Serial.println("[OTA] done, reboot");
    delay(500);
    ESP.restart();
  }
  Serial.println("[OTA] failed");
}

// ---------- Kết nối ----------

// Kết nối MQTT bằng access token; lần đầu thì đăng ký RPC và OTA.
static bool connectMqtt() {
  Serial.printf("[MQTT] connecting %s:%d\n", COREIOT_HOST, COREIOT_PORT);
  if (!tb.connect(COREIOT_HOST, DEVICE_TOKEN, COREIOT_PORT)) {
    Serial.println("[MQTT] connect failed");
    return false;
  }
  Serial.println("[MQTT] connected");
  stateDirty = true;

  if (!subscribed) {
    const std::array<RPC_Callback, RPC_COUNT> callbacks = {
        RPC_Callback{"setFan", onSetFan},
        RPC_Callback{"setLight", onSetLight},
        RPC_Callback{"getFan", onGetFan},
        RPC_Callback{"getLight", onGetLight},
    };
    const OTA_Update_Callback otaCb(FW_TITLE, FW_VERSION, &updater, &onOtaFinished, &onOtaProgress,
                                    &onOtaStart, OTA_FAIL_RETRIES, OTA_PACKET_SIZE);

    // Subscribe: nhận gói mới khi được gán lúc đang chạy.
    // Start: kiểm tra gói đã được gán lúc node tắt; nếu trùng phiên bản thì báo fw_state = UPDATED.
    subscribed = rpc.RPC_Subscribe(callbacks.cbegin(), callbacks.cend()) &&
                 ota.Subscribe_Firmware_Update(otaCb) &&
                 ota.Start_Firmware_Update(otaCb);
    Serial.printf("[MQTT] subscribe %s, fw %s %s\n", subscribed ? "ok" : "FAILED", FW_TITLE, FW_VERSION);
  }
  return true;
}

void coreiot_begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[WIFI] connecting to %s\n", WIFI_SSID);
}

void coreiot_loop() {
  // In log mỗi khi WiFi đổi trạng thái (kết nối / rớt)
  static bool wifiWasUp = false;
  bool wifiUp = WiFi.status() == WL_CONNECTED;
  if (wifiUp != wifiWasUp) {
    wifiWasUp = wifiUp;
    if (wifiUp) Serial.printf("[WIFI] Is connected, IP %s, RSSI %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    else Serial.println("[WIFI] disconnected");
  }
  if (!wifiUp) return;  // WiFi tự kết nối lại

  if (!tb.connected()) {
    if (millis() - lastConnectTryMs < RECONNECT_PERIOD_MS) return;
    lastConnectTryMs = millis();
    if (!connectMqtt()) return;
  }

  tb.loop();  // nhận RPC, chunk OTA

  if (stateDirty || millis() - lastStateMs >= STATE_PERIOD_MS) {
    stateDirty = false;
    lastStateMs = millis();
    tb.sendTelemetryData("fan", relay_get(RELAY_FAN));
    tb.sendTelemetryData("light", relay_get(RELAY_LIGHT));
  }
}
