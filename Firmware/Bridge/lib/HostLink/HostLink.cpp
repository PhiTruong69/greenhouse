#include "HostLink.h"
#include "bridge_config.h"
#include <ArduinoJson.h>
#include <WiFi.h>

static char   lineBuf[HOST_LINE_MAX + 1];
static size_t lineLen = 0;

// Gửi một document JSON thành một dòng.
static void printLine(JsonDocument &doc) {
  serializeJson(doc, Serial);
  Serial.println();
}

// Đổi một dòng JSON thành HostCommand. Trả về false (và in lỗi) nếu không hợp lệ.
static bool parseLine(const char *line, HostCommand &c) {
  JsonDocument doc;
  if (deserializeJson(doc, line)) {
    host_printError(0, "invalid json");
    return false;
  }

  c = {};
  c.requestId = doc["id"] | 0;
  c.nodeId = doc["node"] | 0;
  const char *name = doc["cmd"] | "";

  if (strcmp(name, "setPump") == 0) {
    JsonVariant v = doc["value"];
    c.cmd = CMD_SET_PUMP;
    c.value = v.is<bool>() ? v.as<bool>() : (v.as<int>() != 0);
    c.durationS = doc["duration"] | 0;
    return true;
  }
  host_printError(c.requestId, "unknown cmd");
  return false;
}

void host_begin() {
  Serial.begin(HOST_BAUD);
}

bool host_readCommand(HostCommand &c) {
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\r') continue;
    if (ch != '\n') {
      if (lineLen < HOST_LINE_MAX) lineBuf[lineLen++] = ch;
      continue;
    }
    // Hết một dòng
    lineBuf[lineLen] = '\0';
    bool ok = lineLen > 0 && parseLine(lineBuf, c);
    lineLen = 0;
    if (ok) return true;
  }
  return false;
}

void host_printBoot() {
  JsonDocument doc;
  doc["type"] = "boot";
  doc["mac"] = WiFi.macAddress();
  doc["channel"] = ESPNOW_CHANNEL;
  printLine(doc);
}

void host_printTelemetry(uint8_t nodeId, const TelemetryPkt &t) {
  JsonDocument doc;
  doc["type"] = "telemetry";
  doc["node"] = nodeId;
  doc["seq"] = t.h.seq;
  if (t.status & STATUS_MODBUS_OK) {
    doc["temperature"] = t.airTemp_x10 / 10.0;
    doc["humidity"] = t.airHum_x10 / 10.0;
  }
  doc["soil_raw"] = t.soilRaw;
  doc["soil"] = t.soilPct;
  doc["pump"] = t.pumpOn;
  printLine(doc);
}

void host_printCmdResult(uint8_t nodeId, uint32_t requestId, bool ok, uint8_t result) {
  JsonDocument doc;
  doc["type"] = "cmd_result";
  doc["node"] = nodeId;
  doc["id"] = requestId;
  doc["ok"] = ok;
  if (ok) doc["result"] = result;
  printLine(doc);
}

void host_printError(uint32_t requestId, const char *msg) {
  JsonDocument doc;
  doc["type"] = "error";
  doc["id"] = requestId;
  doc["msg"] = msg;
  printLine(doc);
}
