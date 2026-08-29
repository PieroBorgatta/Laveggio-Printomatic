// Laveggio Printomatic production gateway.
// SPDX-License-Identifier: CC-BY-4.0

#include <Arduino.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <esp_system.h>
#include <time.h>
#include <algorithm>
#include <vector>

#include "DeviceConfig.h"
#include "DisplayDriver.h"
#include "ScaleCore.h"
#include "WebAssets.h"

namespace {

constexpr char kFirmwareVersion[] = "1.0.1";
constexpr uint8_t kAs5600Address = 0x36;
constexpr uint8_t kSdCs = 4;
constexpr uint8_t kI2cSda = 1;
constexpr uint8_t kI2cScl = 2;
constexpr uint8_t kMuxReset = 3;
constexpr uint8_t kPowerSensePin = 20;
constexpr uint32_t kSensorIntervalMs = 20;
constexpr uint32_t kHealthIntervalMs = 250;
constexpr uint32_t kReconnectIntervalMs = 15000;
constexpr uint32_t kRescueApShutdownDelayMs = 120000;
constexpr uint32_t kDiagnosticLogIntervalMs = 60000;
constexpr uint32_t kSystemLogLimit = 4UL * 1024UL * 1024UL;
constexpr uint32_t kHistoryLogLimit = 12UL * 1024UL * 1024UL;
constexpr char kRescueSsid[] = "Laveggio-PW-casklogic";
constexpr char kRescuePassword[] = "casklogic";

struct OutboundMessage {
  char url[256];
  char token[160];
  char caCertificate[3072];
  char body[1152];
};

ConfigStore configStore;
DisplayDriver display;
WebServer webServer(80);
DNSServer dnsServer;
laveggio::SensorReading sensorReadings[laveggio::kChannelCount];
laveggio::StabilityTracker stabilityTracker;
laveggio::WeightSnapshot currentSnapshot;
QueueHandle_t outboundQueue = nullptr;

String deviceSuffix;
String bootId;
int8_t muxAddress = -1;
bool sdReady = false;
bool accessPointActive = false;
bool displayOn = false;
bool integrationLastOk = false;
bool otaSucceeded = false;
bool externalPowerPresent = true;
bool previousExternalPowerPresent = true;
volatile int integrationLastCode = 0;
uint32_t sequenceNumber = 0;
uint32_t scanCounter = 0;
uint32_t scansPerSecond = 0;
uint32_t lastSensorReadMs = 0;
uint32_t lastHealthReadMs = 0;
uint32_t lastScanCounterMs = 0;
uint32_t lastReconnectMs = 0;
uint32_t lastHeartbeatMs = 0;
uint32_t lastDiagnosticLogMs = 0;
uint32_t stationConnectedSinceMs = 0;
uint32_t scheduledRestartMs = 0;

String jsonEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (size_t index = 0; index < value.length(); ++index) {
    const char character = value[index];
    if (character == '\\' || character == '"') {
      escaped += '\\';
      escaped += character;
    } else if (character == '\n') {
      escaped += "\\n";
    } else if (static_cast<uint8_t>(character) >= 0x20) {
      escaped += character;
    }
  }
  return escaped;
}

String quoted(const String &value) {
  return String('"') + jsonEscape(value) + '"';
}

String boolJson(bool value) {
  return value ? "true" : "false";
}

bool parseBool(const String &value) {
  return value == "true" || value == "1" || value == "on";
}

String timestampIso() {
  struct tm timeInfo;
  if (!getLocalTime(&timeInfo, 10)) return "";
  char buffer[32];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S%z", &timeInfo);
  return buffer;
}

String resetReasonLabel() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "accensione";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "watchdog_interrupt";
    case ESP_RST_TASK_WDT: return "watchdog_task";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "brownout";
    default: return "altro";
  }
}

void ensureSdDirectories() {
  if (!sdReady) return;
  if (!SD.exists("/logs")) SD.mkdir("/logs");
  if (!SD.exists("/weights")) SD.mkdir("/weights");
}

String archivePath(const char *path) {
  const String source(path);
  const int separator = source.lastIndexOf('/');
  const int extension = source.lastIndexOf(".ndjson");
  const String directory = source.substring(0, separator);
  const String stem = source.substring(separator + 1, extension);
  char stamp[24];
  struct tm timeInfo;
  if (getLocalTime(&timeInfo, 10)) {
    strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &timeInfo);
  } else {
    snprintf(stamp, sizeof(stamp), "uptime-%010lu", static_cast<unsigned long>(millis()));
  }
  return directory + "/" + stem + "-" + stamp + "-" + String(millis()) + ".ndjson";
}

void rotateLogIfNeeded(const char *path, uint32_t maxBytes) {
  if (!sdReady || !SD.exists(path)) return;
  File file = SD.open(path, FILE_READ);
  if (!file) return;
  const size_t size = file.size();
  file.close();
  if (size <= maxBytes) return;
  SD.rename(path, archivePath(path));
}

std::vector<String> listNdjsonFiles(const char *directory, const char *prefix) {
  std::vector<String> paths;
  if (!sdReady) return paths;
  File folder = SD.open(directory);
  if (!folder || !folder.isDirectory()) return paths;
  File entry = folder.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      String name = entry.name();
      if (name.startsWith("/")) name = name.substring(name.lastIndexOf('/') + 1);
      if (name.startsWith(prefix) && name.endsWith(".ndjson")) {
        paths.push_back(String(directory) + "/" + name);
      }
    }
    entry.close();
    entry = folder.openNextFile();
  }
  folder.close();
  std::sort(paths.begin(), paths.end());
  return paths;
}

void appendLine(const char *path, const String &line, uint32_t maxBytes) {
  if (!sdReady) return;
  digitalWrite(14, HIGH);
  rotateLogIfNeeded(path, maxBytes);
  File file = SD.open(path, FILE_APPEND);
  if (!file) return;
  file.println(line);
  file.flush();
  file.close();
}

void logSystem(const String &level, const String &event, const String &detail = "") {
  String line;
  line.reserve(220 + detail.length());
  line += "{\"captured_at\":" + quoted(timestampIso());
  line += ",\"uptime_ms\":" + String(millis());
  line += ",\"boot_id\":" + quoted(bootId);
  line += ",\"level\":" + quoted(level);
  line += ",\"event\":" + quoted(event);
  if (!detail.isEmpty()) line += ",\"detail\":" + quoted(detail);
  line += "}";
  appendLine("/logs/system.ndjson", line, kSystemLogLimit);
  Serial.println(line);
}

void recordSensorDiagnostics() {
  String line;
  line.reserve(760);
  line += "{\"captured_at\":" + quoted(timestampIso());
  line += ",\"uptime_ms\":" + String(millis());
  line += ",\"boot_id\":" + quoted(bootId);
  line += ",\"event\":\"sensor_diagnostics\"";
  line += ",\"scan_rate_hz\":" + String(scansPerSecond);
  line += ",\"weight_kg\":" + String(currentSnapshot.weightKg);
  line += ",\"valid\":" + boolJson(currentSnapshot.valid);
  line += ",\"stable\":" + boolJson(currentSnapshot.stable);
  line += ",\"sensors\":[";
  for (uint8_t channel = 0; channel < laveggio::kChannelCount; ++channel) {
    if (channel) line += ',';
    const laveggio::SensorReading &reading = sensorReadings[channel];
    line += "{\"channel\":" + String(channel);
    line += ",\"present\":" + boolJson(reading.present);
    line += ",\"raw\":" + String(reading.raw);
    line += ",\"status\":" + String(reading.status);
    line += ",\"agc\":" + String(reading.agc);
    line += ",\"magnitude\":" + String(reading.magnitude) + "}";
  }
  line += "]}";
  appendLine("/logs/system.ndjson", line, kSystemLogLimit);
}

String readTailText(const char *path, size_t maxBytes) {
  if (!sdReady || !SD.exists(path)) return "Nessun log disponibile.";
  File file = SD.open(path, FILE_READ);
  if (!file) return "Impossibile leggere il log.";
  const size_t start = file.size() > maxBytes ? file.size() - maxBytes : 0;
  file.seek(start);
  if (start > 0) file.readStringUntil('\n');
  String output = file.readString();
  file.close();
  return output;
}

bool i2cProbe(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void discoverMux() {
  muxAddress = -1;
  for (uint8_t address = 0x70; address <= 0x77; ++address) {
    if (i2cProbe(address)) {
      muxAddress = address;
      return;
    }
  }
}

bool selectMuxChannel(uint8_t channel) {
  if (muxAddress < 0 || channel >= laveggio::kChannelCount) return false;
  Wire.beginTransmission(static_cast<uint8_t>(muxAddress));
  Wire.write(1U << channel);
  return Wire.endTransmission() == 0;
}

bool readAs5600(uint8_t reg, uint8_t *buffer, size_t length) {
  Wire.beginTransmission(kAs5600Address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(kAs5600Address, length) != length) return false;
  for (size_t index = 0; index < length; ++index) buffer[index] = Wire.read();
  return true;
}

void scanSensors() {
  const uint32_t now = millis();
  const bool healthDue = now - lastHealthReadMs >= kHealthIntervalMs;
  if (muxAddress < 0) discoverMux();

  for (uint8_t channel = 0; channel < laveggio::kChannelCount; ++channel) {
    if (!selectMuxChannel(channel)) {
      sensorReadings[channel].present = false;
      continue;
    }
    delayMicroseconds(450);
    uint8_t angle[2] = {0, 0};
    if (!readAs5600(0x0C, angle, 2)) {
      sensorReadings[channel].present = false;
      continue;
    }
    laveggio::SensorReading &reading = sensorReadings[channel];
    reading.present = true;
    reading.raw = ((static_cast<uint16_t>(angle[0]) << 8) | angle[1]) & 0x0FFF;
    if (healthDue) {
      uint8_t status = reading.status;
      uint8_t agc = reading.agc;
      uint8_t magnitude[2] = {0, 0};
      if (readAs5600(0x0B, &status, 1)) reading.status = status;
      if (readAs5600(0x1A, &agc, 1)) reading.agc = agc;
      if (readAs5600(0x1B, magnitude, 2)) {
        reading.magnitude =
          ((static_cast<uint16_t>(magnitude[0]) << 8) | magnitude[1]) & 0x0FFF;
      }
    }
  }
  if (healthDue) lastHealthReadMs = now;
  if (muxAddress >= 0) {
    Wire.beginTransmission(static_cast<uint8_t>(muxAddress));
    Wire.write(0);
    Wire.endTransmission();
  }
  ++scanCounter;
}

String buildSnapshotJson(const char *eventType, bool includeDelivery) {
  const DeviceConfig &config = configStore.get();
  String json;
  json.reserve(1024);
  json += "{\"type\":" + quoted(eventType);
  json += ",\"schema_version\":1";
  json += ",\"device_id\":" + quoted(config.deviceId);
  json += ",\"boot_id\":" + quoted(bootId);
  json += ",\"sequence\":" + String(sequenceNumber);
  json += ",\"captured_at\":" + quoted(timestampIso());
  json += ",\"captured_ms\":" + String(millis());
  json += ",\"digits\":[";
  for (uint8_t channel = 0; channel < laveggio::kChannelCount; ++channel) {
    if (channel) json += ',';
    json += String(currentSnapshot.digits[channel]);
  }
  json += "],\"multipliers_kg\":[";
  for (uint8_t channel = 0; channel < laveggio::kChannelCount; ++channel) {
    if (channel) json += ',';
    json += String(config.calibrations[channel].multiplierKg);
  }
  json += "]";
  json += ",\"weight_kg\":" + String(currentSnapshot.weightKg);
  json += ",\"stable\":" + boolJson(currentSnapshot.stable);
  json += ",\"valid\":" + boolJson(currentSnapshot.valid);
  json += ",\"sensors\":[";
  for (uint8_t channel = 0; channel < laveggio::kChannelCount; ++channel) {
    if (channel) json += ',';
    const laveggio::SensorReading &reading = sensorReadings[channel];
    json += "{\"channel\":" + String(channel);
    json += ",\"raw\":" + String(reading.raw);
    json += ",\"healthy\":" + boolJson(reading.healthy());
    json += ",\"status\":" + String(reading.status);
    json += ",\"agc\":" + String(reading.agc);
    json += ",\"magnitude\":" + String(reading.magnitude) + "}";
  }
  json += "]";
  if (includeDelivery) {
    const String delivery = config.backendUrl.isEmpty() ? "local" : "queued";
    json += ",\"delivery\":" + quoted(delivery);
  }
  json += "}";
  return json;
}

bool queueOutbound(const String &url, const String &token, const String &body) {
  if (url.isEmpty() || outboundQueue == nullptr) return false;
  OutboundMessage message{};
  strlcpy(message.url, url.c_str(), sizeof(message.url));
  strlcpy(message.token, token.c_str(), sizeof(message.token));
  strlcpy(message.caCertificate, configStore.get().tlsCaCertificate.c_str(), sizeof(message.caCertificate));
  strlcpy(message.body, body.c_str(), sizeof(message.body));
  return xQueueSend(outboundQueue, &message, 0) == pdTRUE;
}

void integrationTask(void *) {
  OutboundMessage message;
  while (true) {
    if (xQueueReceive(outboundQueue, &message, portMAX_DELAY) != pdTRUE) continue;
    if (WiFi.status() != WL_CONNECTED) {
      integrationLastOk = false;
      integrationLastCode = -1;
      continue;
    }

    HTTPClient http;
    const String url(message.url);
    int statusCode = -1;
    if (url.startsWith("https://")) {
      WiFiClientSecure client;
      if (!strlen(message.caCertificate)) {
        integrationLastOk = false;
        integrationLastCode = -2;
        continue;
      }
      client.setCACert(message.caCertificate);
      if (http.begin(client, url)) {
        http.setTimeout(3500);
        http.addHeader("Content-Type", "application/json");
        if (strlen(message.token)) http.addHeader("Authorization", "Bearer " + String(message.token));
        statusCode = http.POST(reinterpret_cast<uint8_t *>(message.body), strlen(message.body));
        http.end();
      }
    } else {
      WiFiClient client;
      if (http.begin(client, url)) {
        http.setTimeout(3500);
        http.addHeader("Content-Type", "application/json");
        if (strlen(message.token)) http.addHeader("Authorization", "Bearer " + String(message.token));
        statusCode = http.POST(reinterpret_cast<uint8_t *>(message.body), strlen(message.body));
        http.end();
      }
    }
    integrationLastCode = statusCode;
    integrationLastOk = statusCode >= 200 && statusCode < 300;
  }
}

void recordWeightEvent() {
  ++sequenceNumber;
  const String record = buildSnapshotJson("scale.snapshot", true);
  appendLine("/weights/history.ndjson", record, kHistoryLogLimit);
  queueOutbound(configStore.get().backendUrl, configStore.get().backendToken, buildSnapshotJson("scale.snapshot", false));
}

void sendHeartbeat() {
  if (!currentSnapshot.valid) return;
  ++sequenceNumber;
  queueOutbound(configStore.get().backendUrl, configStore.get().backendToken, buildSnapshotJson("scale.heartbeat", false));
}

void sendPowerEvent(bool externalPower) {
  String body = "{\"type\":\"device.power\",\"schema_version\":1,\"device_id\":";
  body += quoted(configStore.get().deviceId);
  body += ",\"boot_id\":" + quoted(bootId);
  body += ",\"captured_at\":" + quoted(timestampIso());
  body += ",\"external_power\":" + boolJson(externalPower) + "}";
  queueOutbound(configStore.get().notificationUrl, configStore.get().backendToken, body);
}

bool applyStaticNetworkConfig() {
  const DeviceConfig &config = configStore.get();
  if (config.useDhcp) return true;
  IPAddress ip, gateway, subnet, dns;
  if (!ip.fromString(config.staticIp) || !gateway.fromString(config.gateway) ||
      !subnet.fromString(config.subnet) || !dns.fromString(config.dns)) return false;
  return WiFi.config(ip, gateway, subnet, dns);
}

void startRescueAccessPoint() {
  if (accessPointActive) return;
  stationConnectedSinceMs = 0;
  WiFi.mode(WIFI_AP_STA);
  accessPointActive = WiFi.softAP(kRescueSsid, kRescuePassword);
  if (accessPointActive) {
    dnsServer.start(53, "*", WiFi.softAPIP());
    logSystem("warning", "rescue_ap_started", WiFi.softAPIP().toString());
  }
}

void maintainRescueAccessPoint(uint32_t now) {
  if (WiFi.status() != WL_CONNECTED) {
    stationConnectedSinceMs = 0;
    return;
  }
  if (stationConnectedSinceMs == 0) {
    stationConnectedSinceMs = now;
    const DeviceConfig &config = configStore.get();
    configTzTime(config.timezone.c_str(), config.ntpServer.c_str());
    logSystem("info", "wifi_reconnected", WiFi.localIP().toString());
  }
  if (!accessPointActive || now - stationConnectedSinceMs < kRescueApShutdownDelayMs) return;
  if (!WiFi.softAPdisconnect(false)) {
    stationConnectedSinceMs = now;
    logSystem("warning", "rescue_ap_stop_failed");
    return;
  }
  dnsServer.stop();
  WiFi.mode(WIFI_STA);
  accessPointActive = false;
  logSystem("info", "rescue_ap_stopped", "station_stable_ms=" + String(kRescueApShutdownDelayMs));
}

void connectNetwork() {
  const DeviceConfig &config = configStore.get();
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(config.hostname.c_str());
  if (config.wifiSsid.isEmpty()) {
    startRescueAccessPoint();
    return;
  }
  if (!applyStaticNetworkConfig()) logSystem("error", "invalid_static_network");
  WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());
  const uint32_t startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 12000) delay(100);
  if (WiFi.status() == WL_CONNECTED) {
    stationConnectedSinceMs = millis();
    configTzTime(config.timezone.c_str(), config.ntpServer.c_str());
    if (MDNS.begin(config.hostname.c_str())) MDNS.addService("http", "tcp", 80);
    logSystem("info", "wifi_connected", WiFi.localIP().toString());
  } else {
    startRescueAccessPoint();
  }
}

bool authorized() {
  if (!configStore.isProvisioned() && accessPointActive) return true;
  const DeviceConfig &config = configStore.get();
  if (webServer.authenticate(config.adminUser.c_str(), config.adminPassword.c_str())) return true;
  webServer.requestAuthentication(BASIC_AUTH, "Laveggio Printomatic");
  return false;
}

void sendJson(const String &json, int status = 200) {
  webServer.sendHeader("Cache-Control", "no-store");
  webServer.send(status, "application/json; charset=utf-8", json);
}

void sendError(int status, const String &message) {
  sendJson("{\"error\":" + quoted(message) + "}", status);
}

String buildStatusJson() {
  const DeviceConfig &config = configStore.get();
  String json;
  json.reserve(2600);
  json += "{\"firmware_version\":" + quoted(kFirmwareVersion);
  json += ",\"boot_id\":" + quoted(bootId);
  json += ",\"uptime_seconds\":" + String(millis() / 1000);
  json += ",\"free_heap\":" + String(ESP.getFreeHeap());
  json += ",\"reset_reason\":" + quoted(resetReasonLabel());
  json += ",\"device_time\":" + quoted(timestampIso());
  json += ",\"display_on\":" + boolJson(displayOn);
  json += ",\"scan_rate_hz\":" + String(scansPerSecond);
  json += ",\"network\":{\"connected\":" + boolJson(WiFi.status() == WL_CONNECTED);
  json += ",\"ssid\":" + quoted(WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "");
  json += ",\"ip\":" + quoted(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "");
  json += ",\"rssi\":" + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0);
  json += ",\"ap_active\":" + boolJson(accessPointActive);
  json += ",\"ap_ip\":" + quoted(accessPointActive ? WiFi.softAPIP().toString() : "") + "}";
  json += ",\"integration\":{\"configured\":" + boolJson(!config.backendUrl.isEmpty());
  json += ",\"last_ok\":" + boolJson(integrationLastOk);
  json += ",\"last_code\":" + String(integrationLastCode);
  json += ",\"sequence\":" + String(sequenceNumber) + "}";
  json += ",\"storage\":{\"ready\":" + boolJson(sdReady);
  json += ",\"total_bytes\":" + String(sdReady ? SD.totalBytes() : 0);
  json += ",\"used_bytes\":" + String(sdReady ? SD.usedBytes() : 0) + "}";
  const String powerLabel = !config.powerSenseEnabled ? "Non configurato" :
    (externalPowerPresent ? "Rete elettrica" : "Batteria UPS");
  json += ",\"power\":{\"external\":" + boolJson(externalPowerPresent);
  json += ",\"source_label\":" + quoted(powerLabel) + "}";
  json += ",\"snapshot\":{\"valid\":" + boolJson(currentSnapshot.valid);
  json += ",\"stable\":" + boolJson(currentSnapshot.stable);
  json += ",\"weight_kg\":" + String(currentSnapshot.weightKg);
  json += ",\"stable_for_ms\":" + String(currentSnapshot.stableForMs);
  json += ",\"digits\":[";
  for (uint8_t channel = 0; channel < laveggio::kChannelCount; ++channel) {
    if (channel) json += ',';
    json += String(currentSnapshot.digits[channel]);
  }
  json += "]},\"sensors\":[";
  for (uint8_t channel = 0; channel < laveggio::kChannelCount; ++channel) {
    if (channel) json += ',';
    const laveggio::SensorReading &reading = sensorReadings[channel];
    const laveggio::DecodeResult decoded = laveggio::decodePosition(
      reading.raw,
      config.calibrations[channel],
      -1
    );
    json += "{\"present\":" + boolJson(reading.present);
    json += ",\"healthy\":" + boolJson(reading.healthy());
    json += ",\"raw\":" + String(reading.raw);
    json += ",\"status\":" + String(reading.status);
    json += ",\"agc\":" + String(reading.agc);
    json += ",\"magnitude\":" + String(reading.magnitude);
    json += ",\"position\":" + (decoded.valid ? String(decoded.position) : "null") + "}";
  }
  json += "]}";
  return json;
}

String buildSettingsJson() {
  const DeviceConfig &config = configStore.get();
  String json;
  json.reserve(1200);
  json += "{\"device_id\":" + quoted(config.deviceId);
  json += ",\"hostname\":" + quoted(config.hostname);
  json += ",\"wifi_ssid\":" + quoted(config.wifiSsid);
  json += ",\"use_dhcp\":" + boolJson(config.useDhcp);
  json += ",\"static_ip\":" + quoted(config.staticIp);
  json += ",\"gateway\":" + quoted(config.gateway);
  json += ",\"subnet\":" + quoted(config.subnet);
  json += ",\"dns\":" + quoted(config.dns);
  json += ",\"backend_url\":" + quoted(config.backendUrl);
  json += ",\"tls_ca_certificate\":" + quoted(config.tlsCaCertificate);
  json += ",\"notification_url\":" + quoted(config.notificationUrl);
  json += ",\"stable_ms\":" + String(config.stableWindowMs);
  json += ",\"heartbeat_seconds\":" + String(config.heartbeatSeconds);
  json += ",\"ntp_server\":" + quoted(config.ntpServer);
  json += ",\"timezone\":" + quoted(config.timezone);
  json += ",\"admin_user\":" + quoted(config.adminUser);
  json += ",\"display_default_on\":" + boolJson(config.displayDefaultOn);
  json += ",\"power_sense_enabled\":" + boolJson(config.powerSenseEnabled) + "}";
  return json;
}

String buildCalibrationJson() {
  const DeviceConfig &config = configStore.get();
  String json = "{\"channels\":[";
  json.reserve(1800);
  for (uint8_t channel = 0; channel < laveggio::kChannelCount; ++channel) {
    if (channel) json += ',';
    const laveggio::ChannelCalibration &calibration = config.calibrations[channel];
    json += "{\"channel\":" + String(channel);
    json += ",\"multiplier_kg\":" + String(calibration.multiplierKg);
    json += ",\"tolerance\":" + String(calibration.tolerance);
    json += ",\"hysteresis\":" + String(calibration.hysteresis);
    json += ",\"points\":[";
    for (uint8_t position = 0; position < laveggio::kPositionCount; ++position) {
      if (position) json += ',';
      json += "{\"position\":" + String(position);
      json += ",\"enabled\":" + boolJson(calibration.points[position].enabled);
      json += ",\"raw\":" + String(calibration.points[position].raw) + "}";
    }
    json += "]}";
  }
  json += "]}";
  return json;
}

String buildHistoryJson(size_t limit) {
  const std::vector<String> paths = listNdjsonFiles("/weights", "history");
  if (paths.empty()) return "{\"items\":[]}";
  const size_t tailBytes = 128UL * 1024UL;
  std::vector<String> lines;
  lines.reserve(limit);
  for (auto path = paths.rbegin(); path != paths.rend() && lines.size() < limit; ++path) {
    File file = SD.open(*path, FILE_READ);
    if (!file) continue;
    const size_t start = file.size() > tailBytes ? file.size() - tailBytes : 0;
    file.seek(start);
    if (start > 0) file.readStringUntil('\n');
    std::vector<String> fileLines;
    while (file.available()) {
      String line = file.readStringUntil('\n');
      line.trim();
      if (!line.isEmpty()) fileLines.push_back(line);
    }
    file.close();
    for (auto line = fileLines.rbegin(); line != fileLines.rend() && lines.size() < limit; ++line) {
      lines.push_back(*line);
    }
  }
  String json = "{\"items\":[";
  for (size_t index = 0; index < lines.size(); ++index) {
    if (index) json += ',';
    json += lines[index];
  }
  json += "]}";
  return json;
}

void streamNdjsonExport(
  const char *directory,
  const char *prefix,
  const char *downloadName,
  const char *emptyMessage
) {
  const std::vector<String> paths = listNdjsonFiles(directory, prefix);
  if (paths.empty()) {
    sendError(404, emptyMessage);
    return;
  }
  webServer.sendHeader("Cache-Control", "no-store");
  webServer.sendHeader("Content-Disposition", "attachment; filename=" + String(downloadName));
  webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  webServer.send(200, "application/x-ndjson", "");
  char buffer[1025];
  for (const String &path : paths) {
    File file = SD.open(path, FILE_READ);
    if (!file) continue;
    while (file.available()) {
      const size_t count = file.readBytes(buffer, sizeof(buffer) - 1);
      if (count == 0) break;
      buffer[count] = '\0';
      webServer.sendContent(String(buffer));
    }
    file.close();
  }
  webServer.sendContent("");
}

void registerWebRoutes() {
  webServer.on("/", HTTP_GET, [] {
    webServer.sendHeader("Cache-Control", "no-cache");
    webServer.send_P(200, "text/html; charset=utf-8", WEB_INDEX_HTML);
  });
  webServer.on("/app.css", HTTP_GET, [] {
    webServer.sendHeader("Cache-Control", "public, max-age=3600");
    webServer.send_P(200, "text/css; charset=utf-8", WEB_APP_CSS);
  });
  webServer.on("/app.js", HTTP_GET, [] {
    webServer.sendHeader("Cache-Control", "public, max-age=3600");
    webServer.send_P(200, "application/javascript; charset=utf-8", WEB_APP_JS);
  });
  webServer.on("/casklogicmark.png", HTTP_GET, [] {
    webServer.sendHeader("Cache-Control", "public, max-age=86400");
    webServer.send_P(
      200,
      "image/png",
      reinterpret_cast<const char *>(WEB_CASKLOGIC_MARK),
      WEB_CASKLOGIC_MARK_LEN
    );
  });

  webServer.on("/api/status", HTTP_GET, [] {
    if (!authorized()) return;
    sendJson(buildStatusJson());
  });
  webServer.on("/api/settings", HTTP_GET, [] {
    if (!authorized()) return;
    sendJson(buildSettingsJson());
  });
  webServer.on("/api/calibration", HTTP_GET, [] {
    if (!authorized()) return;
    sendJson(buildCalibrationJson());
  });
  webServer.on("/api/history", HTTP_GET, [] {
    if (!authorized()) return;
    const size_t limit = constrain(webServer.arg("limit").toInt(), 1, 100);
    sendJson(buildHistoryJson(limit));
  });
  webServer.on("/api/history/export", HTTP_GET, [] {
    if (!authorized()) return;
    streamNdjsonExport(
      "/weights",
      "history",
      "laveggio-history-completo.ndjson",
      "Storico non disponibile"
    );
  });
  webServer.on("/api/logs", HTTP_GET, [] {
    if (!authorized()) return;
    webServer.sendHeader("Cache-Control", "no-store");
    webServer.send(200, "text/plain; charset=utf-8", readTailText("/logs/system.ndjson", 24000));
  });
  webServer.on("/api/logs/export", HTTP_GET, [] {
    if (!authorized()) return;
    streamNdjsonExport(
      "/logs",
      "system",
      "laveggio-log-completo.ndjson",
      "Log non disponibile"
    );
  });
  webServer.on("/api/display", HTTP_POST, [] {
    if (!authorized()) return;
    if (!webServer.hasArg("enabled")) {
      sendError(400, "Parametro enabled mancante");
      return;
    }
    displayOn = parseBool(webServer.arg("enabled"));
    display.setEnabled(displayOn);
    logSystem("info", displayOn ? "display_enabled" : "display_disabled");
    sendJson("{\"ok\":true}");
  });

  webServer.on("/api/calibration/capture", HTTP_POST, [] {
    if (!authorized()) return;
    const int channel = webServer.arg("channel").toInt();
    const int position = webServer.arg("position").toInt();
    if (channel < 0 || channel >= laveggio::kChannelCount ||
        position < 0 || position >= laveggio::kPositionCount) {
      sendError(400, "Canale o posizione non validi");
      return;
    }
    if (!sensorReadings[channel].healthy()) {
      sendError(409, "Il sensore non ha un campo magnetico regolare");
      return;
    }
    laveggio::CalibrationPoint &point =
      configStore.mutableConfig().calibrations[channel].points[position];
    point.enabled = true;
    point.raw = sensorReadings[channel].raw;
    configStore.saveCalibration(channel);
    logSystem("info", "calibration_point_saved", "channel=" + String(channel) + " position=" + position);
    sendJson("{\"ok\":true,\"raw\":" + String(point.raw) + "}");
  });

  webServer.on("/api/calibration/settings", HTTP_POST, [] {
    if (!authorized()) return;
    const int channel = webServer.arg("channel").toInt();
    if (channel < 0 || channel >= laveggio::kChannelCount) {
      sendError(400, "Canale non valido");
      return;
    }
    laveggio::ChannelCalibration &calibration = configStore.mutableConfig().calibrations[channel];
    calibration.multiplierKg = constrain(webServer.arg("multiplier").toInt(), 1, 100000);
    calibration.tolerance = constrain(webServer.arg("tolerance").toInt(), 10, 1024);
    calibration.hysteresis = constrain(webServer.arg("hysteresis").toInt(), 0, 512);
    configStore.saveCalibration(channel);
    sendJson("{\"ok\":true}");
  });

  webServer.on("/api/calibration/reset", HTTP_POST, [] {
    if (!authorized()) return;
    const int channel = webServer.arg("channel").toInt();
    if (channel < 0 || channel >= laveggio::kChannelCount) {
      sendError(400, "Canale non valido");
      return;
    }
    configStore.clearCalibration(channel);
    logSystem("warning", "calibration_channel_reset", String(channel));
    sendJson("{\"ok\":true}");
  });

  webServer.on("/api/settings/network", HTTP_POST, [] {
    if (!authorized()) return;
    DeviceConfig &config = configStore.mutableConfig();
    config.wifiSsid = webServer.arg("wifi_ssid");
    if (!webServer.arg("wifi_password").isEmpty()) config.wifiPassword = webServer.arg("wifi_password");
    config.useDhcp = parseBool(webServer.arg("use_dhcp"));
    config.staticIp = webServer.arg("static_ip");
    config.gateway = webServer.arg("gateway");
    config.subnet = webServer.arg("subnet");
    config.dns = webServer.arg("dns");
    configStore.saveSettings();
    logSystem("info", "network_settings_saved");
    sendJson("{\"ok\":true,\"restart_required\":true}");
    scheduledRestartMs = millis() + 1800;
  });

  webServer.on("/api/settings/integration", HTTP_POST, [] {
    if (!authorized()) return;
    DeviceConfig &config = configStore.mutableConfig();
    config.deviceId = webServer.arg("device_id");
    config.backendUrl = webServer.arg("backend_url");
    if (!webServer.arg("backend_token").isEmpty()) config.backendToken = webServer.arg("backend_token");
    config.tlsCaCertificate = webServer.arg("tls_ca_certificate");
    config.notificationUrl = webServer.arg("notification_url");
    config.stableWindowMs = constrain(webServer.arg("stable_ms").toInt(), 100, 5000);
    config.heartbeatSeconds = constrain(webServer.arg("heartbeat_seconds").toInt(), 5, 3600);
    stabilityTracker.setStableWindow(config.stableWindowMs);
    configStore.saveSettings();
    logSystem("info", "integration_settings_saved");
    sendJson("{\"ok\":true}");
  });

  webServer.on("/api/settings/system", HTTP_POST, [] {
    if (!authorized()) return;
    DeviceConfig &config = configStore.mutableConfig();
    config.hostname = webServer.arg("hostname");
    config.ntpServer = webServer.arg("ntp_server");
    config.timezone = webServer.arg("timezone");
    config.adminUser = webServer.arg("admin_user");
    if (!webServer.arg("admin_password").isEmpty()) {
      if (webServer.arg("admin_password").length() < 8) {
        sendError(400, "La password deve avere almeno 8 caratteri");
        return;
      }
      config.adminPassword = webServer.arg("admin_password");
    }
    config.displayDefaultOn = parseBool(webServer.arg("display_default_on"));
    config.powerSenseEnabled = parseBool(webServer.arg("power_sense_enabled"));
    configStore.saveSettings();
    sendJson("{\"ok\":true}");
  });

  webServer.on("/api/wifi/scan", HTTP_GET, [] {
    if (!authorized()) return;
    int count = WiFi.scanComplete();
    if (count == WIFI_SCAN_FAILED) {
      WiFi.scanNetworks(true, true);
      sendJson("{\"scanning\":true,\"networks\":[]}");
      return;
    }
    if (count == WIFI_SCAN_RUNNING) {
      sendJson("{\"scanning\":true,\"networks\":[]}");
      return;
    }
    String json = "{\"scanning\":false,\"networks\":[";
    for (int index = 0; index < count; ++index) {
      if (index) json += ',';
      json += "{\"ssid\":" + quoted(WiFi.SSID(index));
      json += ",\"rssi\":" + String(WiFi.RSSI(index));
      json += ",\"secure\":" + boolJson(WiFi.encryptionType(index) != WIFI_AUTH_OPEN) + "}";
    }
    json += "]}";
    WiFi.scanDelete();
    sendJson(json);
  });

  webServer.on("/api/restart", HTTP_POST, [] {
    if (!authorized()) return;
    logSystem("warning", "restart_requested");
    sendJson("{\"ok\":true}");
    scheduledRestartMs = millis() + 900;
  });

  webServer.on(
    "/api/ota",
    HTTP_POST,
    [] {
      if (!authorized()) return;
      if (otaSucceeded) {
        sendJson("{\"ok\":true,\"restart_required\":true}");
        scheduledRestartMs = millis() + 1200;
      } else {
        sendError(500, Update.errorString());
      }
    },
    [] {
      if (!authorized()) return;
      HTTPUpload &upload = webServer.upload();
      if (upload.status == UPLOAD_FILE_START) {
        otaSucceeded = false;
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
      } else if (upload.status == UPLOAD_FILE_END) {
        otaSucceeded = Update.end(true);
        logSystem(otaSucceeded ? "info" : "error", otaSucceeded ? "ota_complete" : "ota_failed");
      } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Update.abort();
        logSystem("warning", "ota_aborted");
      }
    }
  );

  webServer.onNotFound([] {
    if (webServer.uri().startsWith("/api/")) {
      sendError(404, "Endpoint non trovato");
      return;
    }
    webServer.sendHeader("Location", "/");
    webServer.send(302, "text/plain", "");
  });
  webServer.begin();
}

void initializeIdentity() {
  const uint64_t mac = ESP.getEfuseMac();
  char suffix[7];
  snprintf(suffix, sizeof(suffix), "%06llX", mac & 0xFFFFFFULL);
  deviceSuffix = suffix;
  char boot[24];
  snprintf(boot, sizeof(boot), "%s-%08lX", suffix, static_cast<unsigned long>(esp_random()));
  bootId = boot;
}

void initializeStorage() {
  sdReady = SD.begin(kSdCs, SPI, 20000000);
  if (sdReady) ensureSdDirectories();
}

void checkPowerSource() {
  const DeviceConfig &config = configStore.get();
  if (!config.powerSenseEnabled) return;
  const bool level = digitalRead(kPowerSensePin) == HIGH;
  externalPowerPresent = config.powerSenseActiveHigh ? level : !level;
  if (externalPowerPresent == previousExternalPowerPresent) return;
  previousExternalPowerPresent = externalPowerPresent;
  logSystem(
    externalPowerPresent ? "info" : "warning",
    externalPowerPresent ? "external_power_restored" : "external_power_lost"
  );
  sendPowerEvent(externalPowerPresent);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(250);
  initializeIdentity();
  configStore.begin(deviceSuffix);
  stabilityTracker.setStableWindow(configStore.get().stableWindowMs);

  pinMode(kMuxReset, OUTPUT);
  digitalWrite(kMuxReset, HIGH);
  pinMode(kPowerSensePin, INPUT);
  Wire.begin(kI2cSda, kI2cScl, 100000);
  Wire.setTimeOut(20);
  discoverMux();

  display.begin();
  displayOn = configStore.get().displayDefaultOn;
  display.setEnabled(displayOn);
  initializeStorage();
  logSystem("info", "device_started", "firmware=" + String(kFirmwareVersion));

  Serial.printf("Rescue AP: %s\n", kRescueSsid);
  Serial.printf("Rescue password: %s\n", kRescuePassword);
  Serial.printf("Web admin: %s / %s\n", configStore.get().adminUser.c_str(), configStore.get().adminPassword.c_str());

  outboundQueue = xQueueCreate(4, sizeof(OutboundMessage));
  xTaskCreate(integrationTask, "integration", 8192, nullptr, 1, nullptr);
  connectNetwork();
  registerWebRoutes();

  externalPowerPresent = !configStore.get().powerSenseEnabled ||
    ((digitalRead(kPowerSensePin) == HIGH) == configStore.get().powerSenseActiveHigh);
  previousExternalPowerPresent = externalPowerPresent;
}

void loop() {
  const uint32_t now = millis();
  dnsServer.processNextRequest();
  webServer.handleClient();
  maintainRescueAccessPoint(now);

  if (now - lastSensorReadMs >= kSensorIntervalMs) {
    lastSensorReadMs = now;
    scanSensors();
    currentSnapshot = stabilityTracker.update(
      sensorReadings,
      configStore.get().calibrations,
      now
    );
    if (currentSnapshot.changed) recordWeightEvent();
    display.render(sensorReadings, currentSnapshot, WiFi.status() == WL_CONNECTED, sdReady);
  }

  if (now - lastScanCounterMs >= 1000) {
    scansPerSecond = scanCounter;
    scanCounter = 0;
    lastScanCounterMs = now;
    checkPowerSource();
  }

  if (now - lastHeartbeatMs >= configStore.get().heartbeatSeconds * 1000UL) {
    lastHeartbeatMs = now;
    sendHeartbeat();
  }

  if (now - lastDiagnosticLogMs >= kDiagnosticLogIntervalMs) {
    lastDiagnosticLogMs = now;
    recordSensorDiagnostics();
  }

  if (WiFi.status() != WL_CONNECTED && now - lastReconnectMs >= kReconnectIntervalMs) {
    lastReconnectMs = now;
    if (!configStore.get().wifiSsid.isEmpty()) WiFi.reconnect();
    if (!accessPointActive) startRescueAccessPoint();
  }

  if (scheduledRestartMs != 0 && static_cast<int32_t>(now - scheduledRestartMs) >= 0) {
    delay(50);
    ESP.restart();
  }
  delay(1);
}
