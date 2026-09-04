// CaskLogic PesaLink production gateway for the Laveggio Printomatic scale.
// SPDX-License-Identifier: CC-BY-4.0

#include <Arduino.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <SD_MMC.h>
#include <SPI.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include <mbedtls/base64.h>
#include <mbedtls/md.h>
#include <time.h>
#include <algorithm>
#include <vector>

#include "DeviceConfig.h"
#include "DisplayDriver.h"
#include "BoardHardware.h"
#include "OtaPublicKey.h"
#include "ScaleCore.h"
#include "StoredZip.h"
#include "SpeakerDriver.h"
#include "WebAssets.h"

// Conserva il contratto FS esistente usando il controller SDMMC nativo della nuova scheda.
#define SD SD_MMC

extern "C" bool verifyRollbackLater() {
  return true;
}

namespace {

constexpr char kFirmwareVersion[] = "2.0.0";
constexpr uint8_t kAs5600Address = 0x36;
constexpr uint8_t kSdClock = 14;
constexpr uint8_t kSdCommand = 17;
constexpr uint8_t kSdData0 = 16;
constexpr uint8_t kSdData3 = 21;
constexpr uint8_t kI2cSda = 11;
constexpr uint8_t kI2cScl = 10;
constexpr uint8_t kMuxReset = 18;
constexpr uint8_t kPowerSensePin = 15;
constexpr uint8_t kFactoryResetButtonPin = 0;
constexpr uint8_t kBatteryPowerKeyPin = 6;
constexpr uint8_t kBatteryPowerHoldPin = 7;
constexpr uint32_t kFactoryResetHoldMs = 10000;
constexpr uint32_t kFactoryResetFeedbackDelayMs = 600;
constexpr uint32_t kButtonDebounceMs = 40;
constexpr uint32_t kSensorIntervalMs = 20;
constexpr uint32_t kHealthIntervalMs = 250;
constexpr uint32_t kReconnectIntervalMs = 15000;
constexpr uint32_t kRescueApShutdownDelayMs = 120000;
constexpr uint32_t kDiagnosticLogIntervalMs = 60000;
constexpr uint32_t kSdCheckIntervalMs = 900000;
constexpr uint32_t kMqttReconnectIntervalMs = 15000;
constexpr uint32_t kTimeSyncRetryIntervalMs = 15000;
constexpr uint32_t kRetentionIntervalMs = 86400000UL;
constexpr uint32_t kAuthBlockMs = 60000;
constexpr uint8_t kAuthFailureLimit = 5;
constexpr char kRescueSsid[] = "PesaLink_casklogic-192_168_4_1";
constexpr char kRescuePassword[] = "casklogic";
constexpr char kAuthRealm[] = "CaskLogic PesaLink v3";
static_assert(sizeof(kRescueSsid) - 1 <= 32, "Rescue SSID exceeds the Wi-Fi limit");

struct OutboundMessage {
  char url[256];
  char token[160];
  char caCertificate[3072];
  char clientCertificate[3072];
  char clientPrivateKey[3072];
  char body[1536];
  bool heartbeat;
};

struct SensorErrorCounters {
  uint32_t readFailures = 0;
  uint32_t missingSamples = 0;
  uint32_t weakMagnetSamples = 0;
  uint32_t strongMagnetSamples = 0;
  uint32_t unhealthyTransitions = 0;
  bool stateInitialized = false;
  bool previouslyHealthy = false;
  String lastErrorAt;
};

struct CachedWifiNetwork {
  String ssid;
  int32_t rssi = 0;
  bool secure = false;
};

constexpr uint8_t kMaxCachedWifiNetworks = 16;

struct SdHealthState {
  bool lastCheckOk = false;
  uint32_t checks = 0;
  uint32_t failures = 0;
  uint32_t malformedRecords = 0;
  String lastCheckedAt;
  String lastError;
};

ConfigStore configStore;
DisplayDriver display;
BoardHardware boardHardware;
SpeakerDriver speaker;
WebServer webServer(80);
DNSServer dnsServer;
WiFiClientSecure mqttTlsClient;
PubSubClient mqttClient(mqttTlsClient);
laveggio::SensorReading sensorReadings[laveggio::kChannelCount];
laveggio::StabilityTracker stabilityTracker;
laveggio::WeightSnapshot currentSnapshot;
QueueHandle_t outboundQueue = nullptr;
SensorErrorCounters sensorErrors[laveggio::kChannelCount];
SdHealthState sdHealth;
CachedWifiNetwork cachedWifiNetworks[kMaxCachedWifiNetworks];
uint8_t cachedWifiNetworkCount = 0;

String deviceSuffix;
String bootId;
int8_t muxAddress = -1;
bool sdReady = false;
bool accessPointActive = false;
bool displayOn = false;
bool speakerOn = true;
bool integrationLastOk = false;
bool otaSucceeded = false;
bool otaSignatureVerified = false;
bool otaUploadAuthorized = false;
bool otaChunkUploadActive = false;
bool timeSynchronized = false;
bool mqttLastConnected = false;
bool configSyncLastOk = false;
bool externalPowerPresent = true;
bool previousExternalPowerPresent = true;
volatile int integrationLastCode = 0;
volatile bool heartbeatResultPending = false;
volatile bool heartbeatResultOk = false;
volatile int heartbeatResultCode = 0;
portMUX_TYPE heartbeatResultMux = portMUX_INITIALIZER_UNLOCKED;
uint32_t sequenceNumber = 0;
uint16_t batteryVoltageMv = 0;
uint8_t heartbeatConsecutiveFailures = 0;
uint8_t authFailures = 0;
uint32_t scanCounter = 0;
uint32_t scansPerSecond = 0;
uint32_t lastSensorReadMs = 0;
uint32_t lastHealthReadMs = 0;
uint32_t lastScanCounterMs = 0;
uint32_t lastReconnectMs = 0;
uint32_t lastNetworkStatusMs = 0;
uint32_t lastHeartbeatMs = 0;
uint32_t lastDiagnosticLogMs = 0;
uint32_t lastSdCheckMs = 0;
uint32_t lastMqttReconnectMs = 0;
uint32_t lastTimeSyncAttemptMs = 0;
uint32_t lastConfigSyncMs = 0;
uint32_t lastRetentionMs = 0;
uint32_t stationConnectedSinceMs = 0;
uint32_t scheduledRestartMs = 0;
uint32_t factoryResetPressedSinceMs = 0;
bool factoryResetTriggered = false;
uint32_t authBlockedUntilMs = 0;
String csrfToken;
String lastHeartbeatAckAt;
String lastTimeSyncAt;
String lastConfigSyncAt;
String lastConfigSyncError;
String otaTargetLabel;
String otaPreviousVersion;
String otaErrorDetail;
size_t otaExpectedBytes = 0;
size_t otaReceivedBytes = 0;
UpdaterECDSAVerifier otaVerifier(PUBLIC_KEY, PUBLIC_KEY_LEN, HASH_SHA256);
bool configSyncAttemptedThisBoot = false;

void logSystem(const String &level, const String &event, const String &detail);
bool syncRemoteConfiguration();

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

bool parseIpAddress(const String &value, IPAddress &address) {
  return !value.isEmpty() && address.fromString(value);
}

String timestampIso() {
  struct tm timeInfo;
  if (!getLocalTime(&timeInfo, 10)) return "";
  char buffer[32];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S%z", &timeInfo);
  return buffer;
}

String timestampIso(time_t epoch) {
  if (epoch < 1700000000) return "";
  struct tm timeInfo;
  localtime_r(&epoch, &timeInfo);
  char buffer[32];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S%z", &timeInfo);
  return buffer;
}

void requestTimeSynchronization() {
  if (WiFi.status() != WL_CONNECTED) return;
  const DeviceConfig &config = configStore.get();
  configTzTime(config.timezone.c_str(), config.ntpServer.c_str());
  lastTimeSyncAttemptMs = millis();
}

void pollTimeSynchronization(uint32_t now) {
  const bool valid = time(nullptr) >= 1700000000;
  if (valid && !timeSynchronized) {
    timeSynchronized = true;
    lastTimeSyncAt = timestampIso();
    boardHardware.synchronizeRtc(time(nullptr));
    logSystem("info", "time_synchronized", lastTimeSyncAt);
  }
  if (!valid && WiFi.status() == WL_CONNECTED &&
      now - lastTimeSyncAttemptMs >= kTimeSyncRetryIntervalMs) {
    requestTimeSynchronization();
  }
}

String hmacSha256Hex(const String &secret, const String &payload) {
  if (secret.isEmpty()) return "";
  unsigned char digest[32] = {0};
  const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (info == nullptr || mbedtls_md_hmac(
        info,
        reinterpret_cast<const unsigned char *>(secret.c_str()),
        secret.length(),
        reinterpret_cast<const unsigned char *>(payload.c_str()),
        payload.length(),
        digest
      ) != 0) return "";
  char hex[65];
  for (uint8_t index = 0; index < sizeof(digest); ++index) {
    snprintf(hex + index * 2, 3, "%02x", digest[index]);
  }
  hex[64] = '\0';
  return hex;
}

uint8_t estimatedBatteryPercent() {
  const DeviceConfig &config = configStore.get();
  if (!config.batterySenseEnabled || batteryVoltageMv == 0 || config.batteryMaxMv <= config.batteryMinMv) return 0;
  const long percent = map(
    constrain(batteryVoltageMv, config.batteryMinMv, config.batteryMaxMv),
    config.batteryMinMv,
    config.batteryMaxMv,
    0,
    100
  );
  return static_cast<uint8_t>(constrain(percent, 0L, 100L));
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

bool ensureDirectoryTree(const String &path) {
  if (!sdReady || path.isEmpty()) return false;
  String current;
  int start = path.startsWith("/") ? 1 : 0;
  while (start < static_cast<int>(path.length())) {
    const int separator = path.indexOf('/', start);
    const int end = separator < 0 ? path.length() : separator;
    if (end > start) {
      current += "/" + path.substring(start, end);
      if (!SD.exists(current) && !SD.mkdir(current)) return false;
    }
    if (separator < 0) break;
    start = separator + 1;
  }
  return true;
}

String weeklyLogPath(const char *root, const char *prefix) {
  struct tm timeInfo;
  if (!getLocalTime(&timeInfo, 10)) {
    const String directory = String(root) + "/unsynced";
    ensureDirectoryTree(directory);
    return directory + "/" + prefix + "-" + bootId + ".ndjson";
  }
  char directory[24];
  char week[24];
  strftime(directory, sizeof(directory), "%Y/%m", &timeInfo);
  strftime(week, sizeof(week), "%G-W%V", &timeInfo);
  const String fullDirectory = String(root) + "/" + directory;
  ensureDirectoryTree(fullDirectory);
  return fullDirectory + "/" + prefix + "-" + week + ".ndjson";
}

String archivePath(const String &source) {
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

void rotateLogIfNeeded(const String &path, uint32_t maxBytes) {
  if (!sdReady || !SD.exists(path)) return;
  File file = SD.open(path, FILE_READ);
  if (!file) return;
  const size_t size = file.size();
  file.close();
  if (size <= maxBytes) return;
  SD.rename(path, archivePath(path));
}

void collectNdjsonFiles(const String &directory, const char *prefix, std::vector<String> &paths) {
  File folder = SD.open(directory);
  if (!folder || !folder.isDirectory()) return;
  File entry = folder.openNextFile();
  while (entry) {
    String entryPath = entry.path();
    if (!entryPath.startsWith("/")) entryPath = directory + "/" + entryPath;
    const bool directoryEntry = entry.isDirectory();
    entry.close();
    if (directoryEntry) {
      collectNdjsonFiles(entryPath, prefix, paths);
    } else {
      const String name = entryPath.substring(entryPath.lastIndexOf('/') + 1);
      if (name.startsWith(prefix) && name.endsWith(".ndjson")) {
        paths.push_back(entryPath);
      }
    }
    entry = folder.openNextFile();
  }
  folder.close();
}

std::vector<String> listNdjsonFiles(const char *directory, const char *prefix) {
  std::vector<String> paths;
  if (!sdReady) return paths;
  collectNdjsonFiles(directory, prefix, paths);
  const String root = directory;
  std::sort(paths.begin(), paths.end(), [&root](const String &left, const String &right) {
    String leftKey = left;
    String rightKey = right;
    const String leftRelative = left.substring(root.length() + 1);
    const String rightRelative = right.substring(root.length() + 1);
    if (leftRelative.indexOf('/') < 0) leftKey = root + "/0000/00/" + leftRelative;
    if (rightRelative.indexOf('/') < 0) rightKey = root + "/0000/00/" + rightRelative;
    leftKey.replace("/unsynced/", "/0000/00/");
    rightKey.replace("/unsynced/", "/0000/00/");
    return leftKey < rightKey;
  });
  return paths;
}

void appendLine(const String &path, const String &line, uint32_t maxBytes) {
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
  line += ",\"time_synchronized\":" + boolJson(timeSynchronized);
  line += ",\"uptime_ms\":" + String(millis());
  line += ",\"boot_id\":" + quoted(bootId);
  line += ",\"level\":" + quoted(level);
  line += ",\"event\":" + quoted(event);
  if (!detail.isEmpty()) line += ",\"detail\":" + quoted(detail);
  line += "}";
  appendLine(
    weeklyLogPath("/logs", "system"),
    line,
    configStore.get().systemLogFileMaxMb * 1024UL * 1024UL
  );
  Serial.println(line);
}

void recordSensorDiagnostics() {
  String line;
  line.reserve(760);
  line += "{\"captured_at\":" + quoted(timestampIso());
  line += ",\"time_synchronized\":" + boolJson(timeSynchronized);
  line += ",\"uptime_ms\":" + String(millis());
  line += ",\"boot_id\":" + quoted(bootId);
  line += ",\"event\":\"sensor_diagnostics\"";
  line += ",\"scan_rate_hz\":" + String(scansPerSecond);
  line += ",\"external_power\":" + boolJson(externalPowerPresent);
  line += ",\"battery_voltage_mv\":" + String(batteryVoltageMv);
  line += ",\"current_ma\":null";
  line += ",\"chip_temperature_c\":" + String(temperatureRead(), 1);
  line += ",\"wifi_rssi\":" + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0);
  line += ",\"free_heap\":" + String(ESP.getFreeHeap());
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
    line += ",\"magnitude\":" + String(reading.magnitude);
    line += ",\"read_failures\":" + String(sensorErrors[channel].readFailures);
    line += ",\"magnet_errors\":" + String(
      sensorErrors[channel].weakMagnetSamples + sensorErrors[channel].strongMagnetSamples
    ) + "}";
  }
  line += "]}";
  appendLine(
    weeklyLogPath("/logs", "system"),
    line,
    configStore.get().systemLogFileMaxMb * 1024UL * 1024UL
  );
  Serial.println(line);
}

String readTailText(const String &path, size_t maxBytes) {
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

String readLatestLogTail(size_t maxBytes) {
  const std::vector<String> paths = listNdjsonFiles("/logs", "system");
  if (paths.empty()) return "Nessun log disponibile.";
  return readTailText(paths.back(), maxBytes);
}

uint32_t countMalformedNdjsonTail(const String &path, size_t maxBytes = 32768) {
  if (!sdReady || !SD.exists(path)) return 0;
  File file = SD.open(path, FILE_READ);
  if (!file) return 1;
  const size_t start = file.size() > maxBytes ? file.size() - maxBytes : 0;
  file.seek(start);
  if (start > 0) file.readStringUntil('\n');
  uint32_t malformed = 0;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (!line.isEmpty() && !(line.startsWith("{") && line.endsWith("}"))) ++malformed;
  }
  file.close();
  return malformed;
}

bool runSdHealthCheck() {
  ++sdHealth.checks;
  sdHealth.lastCheckedAt = timestampIso();
  sdHealth.lastError = "";
  sdHealth.malformedRecords = 0;
  if (!sdReady) {
    ++sdHealth.failures;
    sdHealth.lastCheckOk = false;
    sdHealth.lastError = "MicroSD non montata";
    return false;
  }
  ensureDirectoryTree("/diagnostics");
  const String testPath = "/diagnostics/.sd-health-" + bootId + ".tmp";
  const String expected = "PESALINK-SD-CHECK-" + String(esp_random(), HEX);
  File output = SD.open(testPath, FILE_WRITE);
  if (!output || output.print(expected) != expected.length()) {
    if (output) output.close();
    ++sdHealth.failures;
    sdHealth.lastCheckOk = false;
    sdHealth.lastError = "Scrittura di controllo fallita";
    return false;
  }
  output.flush();
  output.close();
  File input = SD.open(testPath, FILE_READ);
  const String actual = input ? input.readString() : "";
  if (input) input.close();
  SD.remove(testPath);
  if (actual != expected) {
    ++sdHealth.failures;
    sdHealth.lastCheckOk = false;
    sdHealth.lastError = "Lettura di controllo incoerente";
    return false;
  }

  const std::vector<String> logPaths = listNdjsonFiles("/logs", "system");
  const std::vector<String> historyPaths = listNdjsonFiles("/weights", "history");
  if (!logPaths.empty()) sdHealth.malformedRecords += countMalformedNdjsonTail(logPaths.back());
  if (!historyPaths.empty()) sdHealth.malformedRecords += countMalformedNdjsonTail(historyPaths.back());
  const uint64_t total = SD.totalBytes();
  const uint64_t free = total > SD.usedBytes() ? total - SD.usedBytes() : 0;
  if (sdHealth.malformedRecords > 0) sdHealth.lastError = "Record NDJSON malformati=" + String(sdHealth.malformedRecords);
  if (total > 0 && (free < 128ULL * 1024ULL * 1024ULL || free * 100ULL / total < 5ULL)) {
    if (!sdHealth.lastError.isEmpty()) sdHealth.lastError += "; ";
    sdHealth.lastError += "Spazio libero insufficiente";
  }
  sdHealth.lastCheckOk = sdHealth.lastError.isEmpty();
  if (!sdHealth.lastCheckOk) {
    ++sdHealth.failures;
    logSystem("warning", "sd_health_warning", sdHealth.lastError);
  }
  return sdHealth.lastCheckOk;
}

void recordFirmwareUpdate(const String &outcome, const String &target, const String &detail = "") {
  if (!sdReady) return;
  ensureDirectoryTree("/updates");
  String line = "{\"captured_at\":" + quoted(timestampIso());
  line += ",\"boot_id\":" + quoted(bootId);
  line += ",\"previous_version\":" + quoted(otaPreviousVersion.isEmpty() ? kFirmwareVersion : otaPreviousVersion);
  line += ",\"target\":" + quoted(target);
  line += ",\"running_version\":" + quoted(kFirmwareVersion);
  line += ",\"outcome\":" + quoted(outcome);
  line += ",\"signature_verified\":" + boolJson(otaSignatureVerified);
  if (!detail.isEmpty()) line += ",\"detail\":" + quoted(detail);
  line += "}";
  appendLine("/updates/registry.ndjson", line, 4UL * 1024UL * 1024UL);
}

bool beginSignedOta(size_t signedSize, const String &target) {
  if (Update.isRunning()) Update.abort();
  otaSucceeded = false;
  otaSignatureVerified = false;
  otaErrorDetail = "";
  otaPreviousVersion = kFirmwareVersion;
  otaTargetLabel = target;
  otaExpectedBytes = signedSize;
  otaReceivedBytes = 0;
  if (signedSize <= 512) {
    otaErrorDetail = "Dimensione firmware firmato mancante o non valida";
    return false;
  }
  if (!Update.installSignature(&otaVerifier)) {
    otaErrorDetail = "Impossibile inizializzare la verifica ECDSA";
    return false;
  }
  if (!Update.begin(signedSize)) {
    otaErrorDetail = Update.errorString();
    Update.printError(Serial);
    return false;
  }
  recordFirmwareUpdate("started", otaTargetLabel, "signed_bytes=" + String(signedSize));
  return true;
}

bool finishSignedOta() {
  otaSucceeded = Update.isRunning() && otaReceivedBytes == otaExpectedBytes && Update.end();
  otaSignatureVerified = otaSucceeded;
  if (!otaSucceeded && otaErrorDetail.isEmpty()) otaErrorDetail = Update.errorString();
  recordFirmwareUpdate(otaSucceeded ? "verified" : "failed", otaTargetLabel, otaErrorDetail);
  logSystem(otaSucceeded ? "info" : "error", otaSucceeded ? "ota_complete" : "ota_failed");
  return otaSucceeded;
}

void pruneExpiredArchives() {
  const DeviceConfig &config = configStore.get();
  if (!sdReady || config.historyKeepForever) return;
  const time_t now = time(nullptr);
  if (now < 1700000000) return;
  const time_t cutoff = now - static_cast<time_t>(config.historyRetentionDays) * 86400;
  uint32_t removed = 0;
  const std::vector<String> paths = listNdjsonFiles("/weights", "history");
  for (const String &path : paths) {
    File file = SD.open(path, FILE_READ);
    if (!file) continue;
    const time_t modified = file.getLastWrite();
    file.close();
    if (modified > 1700000000 && modified < cutoff && SD.remove(path)) ++removed;
  }
  if (removed) logSystem("info", "history_retention_pruned", "files=" + String(removed));
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
      ++sensorErrors[channel].readFailures;
      if (healthDue) {
        ++sensorErrors[channel].missingSamples;
        sensorErrors[channel].lastErrorAt = timestampIso();
        if (sensorErrors[channel].stateInitialized && sensorErrors[channel].previouslyHealthy) {
          ++sensorErrors[channel].unhealthyTransitions;
        }
        sensorErrors[channel].stateInitialized = true;
        sensorErrors[channel].previouslyHealthy = false;
      }
      continue;
    }
    delayMicroseconds(450);
    if (!i2cProbe(kAs5600Address)) {
      sensorReadings[channel].present = false;
      ++sensorErrors[channel].readFailures;
      if (healthDue) {
        ++sensorErrors[channel].missingSamples;
        sensorErrors[channel].lastErrorAt = timestampIso();
        if (sensorErrors[channel].stateInitialized && sensorErrors[channel].previouslyHealthy) {
          ++sensorErrors[channel].unhealthyTransitions;
        }
        sensorErrors[channel].stateInitialized = true;
        sensorErrors[channel].previouslyHealthy = false;
      }
      continue;
    }
    uint8_t angle[2] = {0, 0};
    if (!readAs5600(0x0C, angle, 2)) {
      sensorReadings[channel].present = false;
      ++sensorErrors[channel].readFailures;
      if (healthDue) {
        ++sensorErrors[channel].missingSamples;
        sensorErrors[channel].lastErrorAt = timestampIso();
        if (sensorErrors[channel].stateInitialized && sensorErrors[channel].previouslyHealthy) {
          ++sensorErrors[channel].unhealthyTransitions;
        }
        sensorErrors[channel].stateInitialized = true;
        sensorErrors[channel].previouslyHealthy = false;
      }
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
      const bool healthy = reading.healthy();
      SensorErrorCounters &errors = sensorErrors[channel];
      if (reading.magnetWeak()) ++errors.weakMagnetSamples;
      if (reading.magnetStrong()) ++errors.strongMagnetSamples;
      if (!healthy) errors.lastErrorAt = timestampIso();
      if (errors.stateInitialized && errors.previouslyHealthy && !healthy) {
        ++errors.unhealthyTransitions;
      }
      errors.stateInitialized = true;
      errors.previouslyHealthy = healthy;
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
  const String capturedAt = timestampIso();
  const String eventId = config.deviceId + ":" + bootId + ":" + String(sequenceNumber);
  String digitsCanonical;
  for (uint8_t channel = 0; channel < laveggio::kChannelCount; ++channel) {
    if (channel) digitsCanonical += '.';
    digitsCanonical += String(currentSnapshot.digits[channel]);
  }
  const String signatureInput = eventId + "\n" + capturedAt + "\n" +
    String(currentSnapshot.weightKg) + "\n" + digitsCanonical;
  const String signature = strcmp(eventType, "scale.snapshot") == 0
    ? hmacSha256Hex(config.eventHmacSecret, signatureInput)
    : "";
  String json;
  json.reserve(1280);
  json += "{\"type\":" + quoted(eventType);
  json += ",\"schema_version\":1";
  json += ",\"event_id\":" + quoted(eventId);
  json += ",\"device_id\":" + quoted(config.deviceId);
  json += ",\"boot_id\":" + quoted(bootId);
  json += ",\"sequence\":" + String(sequenceNumber);
  json += ",\"captured_at\":" + quoted(capturedAt);
  json += ",\"time_synchronized\":" + boolJson(timeSynchronized);
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
  json += ",\"signature_alg\":" + (signature.isEmpty() ? String("null") : quoted("HMAC-SHA256"));
  json += ",\"signature\":" + (signature.isEmpty() ? String("null") : quoted(signature));
  if (includeDelivery) {
    const String delivery = config.backendUrl.isEmpty() && !config.mqttEnabled ? "local" : "requested";
    json += ",\"delivery\":" + quoted(delivery);
  }
  json += "}";
  return json;
}

void reportHeartbeatResult(bool ok, int code) {
  portENTER_CRITICAL(&heartbeatResultMux);
  heartbeatResultOk = ok;
  heartbeatResultCode = code;
  heartbeatResultPending = true;
  portEXIT_CRITICAL(&heartbeatResultMux);
}

bool queueOutbound(const String &url, const String &token, const String &body, bool heartbeat = false) {
  if (url.isEmpty() || outboundQueue == nullptr) return false;
  OutboundMessage message{};
  strlcpy(message.url, url.c_str(), sizeof(message.url));
  strlcpy(message.token, token.c_str(), sizeof(message.token));
  strlcpy(message.caCertificate, configStore.get().tlsCaCertificate.c_str(), sizeof(message.caCertificate));
  strlcpy(message.clientCertificate, configStore.get().tlsClientCertificate.c_str(), sizeof(message.clientCertificate));
  strlcpy(message.clientPrivateKey, configStore.get().tlsClientPrivateKey.c_str(), sizeof(message.clientPrivateKey));
  strlcpy(message.body, body.c_str(), sizeof(message.body));
  message.heartbeat = heartbeat;
  return xQueueSend(outboundQueue, &message, 0) == pdTRUE;
}

void integrationTask(void *) {
  OutboundMessage *message = static_cast<OutboundMessage *>(malloc(sizeof(OutboundMessage)));
  if (message == nullptr) {
    integrationLastOk = false;
    integrationLastCode = -5;
    vTaskDelete(nullptr);
    return;
  }
  while (true) {
    if (xQueueReceive(outboundQueue, message, portMAX_DELAY) != pdTRUE) continue;
    if (WiFi.status() != WL_CONNECTED) {
      integrationLastOk = false;
      integrationLastCode = -1;
      if (message->heartbeat) reportHeartbeatResult(false, -1);
      continue;
    }

    HTTPClient http;
    const String url(message->url);
    int statusCode = -1;
    if (url.startsWith("https://")) {
      WiFiClientSecure client;
      if (!strlen(message->caCertificate)) {
        integrationLastOk = false;
        integrationLastCode = -2;
        if (message->heartbeat) reportHeartbeatResult(false, -2);
        continue;
      }
      client.setCACert(message->caCertificate);
      if (strlen(message->clientCertificate) && strlen(message->clientPrivateKey)) {
        client.setCertificate(message->clientCertificate);
        client.setPrivateKey(message->clientPrivateKey);
      }
      if (http.begin(client, url)) {
        http.setTimeout(3500);
        http.addHeader("Content-Type", "application/json");
        if (strlen(message->token)) http.addHeader("Authorization", "Bearer " + String(message->token));
        statusCode = http.POST(reinterpret_cast<uint8_t *>(message->body), strlen(message->body));
        http.end();
      }
    } else {
      statusCode = -4;
    }
    integrationLastCode = statusCode;
    integrationLastOk = statusCode >= 200 && statusCode < 300;
    if (message->heartbeat) reportHeartbeatResult(integrationLastOk, statusCode);
  }
}

String mqttTopic(const char *suffix) {
  String base = configStore.get().mqttBaseTopic;
  while (base.endsWith("/")) base.remove(base.length() - 1);
  return base + "/" + configStore.get().deviceId + "/" + suffix;
}

void publishMqttCommandAck(const String &commandId, const String &command, bool ok, const String &detail) {
  if (!mqttClient.connected()) return;
  String body = "{\"command_id\":" + quoted(commandId);
  body += ",\"command\":" + quoted(command);
  body += ",\"ok\":" + boolJson(ok);
  body += ",\"detail\":" + quoted(detail);
  body += ",\"captured_at\":" + quoted(timestampIso()) + "}";
  mqttClient.publish(mqttTopic("command-acks").c_str(), body.c_str(), false);
}

void mqttMessageReceived(char *, byte *payload, unsigned int length) {
  if (!configStore.get().mqttCommandsEnabled || length == 0 || length > 1024) return;
  JsonDocument document;
  if (deserializeJson(document, payload, length) != DeserializationError::Ok) return;
  const String commandId = document["command_id"] | "";
  const String command = document["command"] | "";
  if (commandId.isEmpty() || command.isEmpty()) return;

  if (command == "display.set") {
    if (!document["enabled"].is<bool>()) {
      publishMqttCommandAck(commandId, command, false, "Parametro enabled mancante");
      return;
    }
    displayOn = document["enabled"].as<bool>();
    display.setEnabled(displayOn);
    logSystem("info", "mqtt_display_command", displayOn ? "enabled" : "disabled");
    publishMqttCommandAck(commandId, command, true, "Display aggiornato");
    return;
  }
  if (command == "speaker.set") {
    if (!document["enabled"].is<bool>()) {
      publishMqttCommandAck(commandId, command, false, "Parametro enabled mancante");
      return;
    }
    speakerOn = document["enabled"].as<bool>();
    speaker.setEnabled(speakerOn);
    logSystem("info", "mqtt_speaker_command", speakerOn ? "enabled" : "disabled");
    publishMqttCommandAck(commandId, command, true, "Speaker aggiornato");
    return;
  }
  if (command == "config.sync") {
    const bool ok = syncRemoteConfiguration();
    publishMqttCommandAck(commandId, command, ok, ok ? "Configurazione sincronizzata" : lastConfigSyncError);
    return;
  }
  if (command == "diagnostics.run") {
    lastSdCheckMs = 0;
    logSystem("info", "mqtt_diagnostics_requested", commandId);
    publishMqttCommandAck(commandId, command, true, "Diagnostica pianificata");
    return;
  }
  publishMqttCommandAck(commandId, command, false, "Comando non consentito");
}

void maintainMqtt(uint32_t now) {
  const DeviceConfig &config = configStore.get();
  if (!config.mqttEnabled || config.mqttHost.isEmpty()) {
    if (mqttClient.connected()) mqttClient.disconnect();
    mqttLastConnected = false;
    return;
  }
  if (mqttClient.connected()) {
    mqttLastConnected = true;
    mqttClient.loop();
    return;
  }
  mqttLastConnected = false;
  if (WiFi.status() != WL_CONNECTED || config.tlsCaCertificate.isEmpty() ||
      now - lastMqttReconnectMs < kMqttReconnectIntervalMs) return;
  lastMqttReconnectMs = now;
  mqttTlsClient.setCACert(config.tlsCaCertificate.c_str());
  if (!config.tlsClientCertificate.isEmpty() && !config.tlsClientPrivateKey.isEmpty()) {
    mqttTlsClient.setCertificate(config.tlsClientCertificate.c_str());
    mqttTlsClient.setPrivateKey(config.tlsClientPrivateKey.c_str());
  }
  mqttClient.setServer(config.mqttHost.c_str(), config.mqttPort);
  const String willTopic = mqttTopic("availability");
  const bool connected = mqttClient.connect(
    config.deviceId.c_str(),
    config.mqttUsername.c_str(),
    config.mqttPassword.c_str(),
    willTopic.c_str(),
    1,
    true,
    "offline"
  );
  if (!connected) {
    logSystem("warning", "mqtt_connect_failed", "state=" + String(mqttClient.state()));
    return;
  }
  mqttLastConnected = true;
  mqttClient.publish(willTopic.c_str(), "online", true);
  mqttClient.publish(mqttTopic("status").c_str(), buildSnapshotJson("scale.heartbeat", false).c_str(), true);
  if (config.mqttCommandsEnabled) mqttClient.subscribe(mqttTopic("commands").c_str(), 1);
  logSystem("info", "mqtt_connected", config.mqttHost + ":" + String(config.mqttPort));
}

bool syncRemoteConfiguration() {
  DeviceConfig &config = configStore.mutableConfig();
  configSyncAttemptedThisBoot = true;
  lastConfigSyncMs = millis();
  lastConfigSyncError = "";
  if (!config.configSyncEnabled || config.configSyncUrl.isEmpty()) {
    lastConfigSyncError = "Sincronizzazione non configurata";
    return false;
  }
  if (WiFi.status() != WL_CONNECTED || !timeSynchronized) {
    lastConfigSyncError = "Rete o orario non disponibili";
    return false;
  }
  if (!config.configSyncUrl.startsWith("https://") || config.tlsCaCertificate.isEmpty()) {
    lastConfigSyncError = "HTTPS verificato obbligatorio";
    return false;
  }

  WiFiClientSecure client;
  client.setCACert(config.tlsCaCertificate.c_str());
  if (!config.tlsClientCertificate.isEmpty() && !config.tlsClientPrivateKey.isEmpty()) {
    client.setCertificate(config.tlsClientCertificate.c_str());
    client.setPrivateKey(config.tlsClientPrivateKey.c_str());
  }
  HTTPClient http;
  if (!http.begin(client, config.configSyncUrl)) {
    lastConfigSyncError = "Impossibile inizializzare HTTPS";
    return false;
  }
  http.setTimeout(3500);
  if (!config.backendToken.isEmpty()) http.addHeader("Authorization", "Bearer " + config.backendToken);
  http.addHeader("X-Device-Id", config.deviceId);
  http.addHeader("X-Config-Version", String(config.remoteConfigVersion));
  const int code = http.GET();
  if (code == 304) {
    configSyncLastOk = true;
    lastConfigSyncAt = timestampIso();
    http.end();
    return true;
  }
  if (code != 200) {
    lastConfigSyncError = "HTTP " + String(code);
    configSyncLastOk = false;
    http.end();
    logSystem("warning", "config_sync_failed", lastConfigSyncError);
    return false;
  }
  JsonDocument document;
  const DeserializationError error = deserializeJson(document, http.getStream());
  http.end();
  if (error) {
    lastConfigSyncError = "JSON non valido";
    configSyncLastOk = false;
    return false;
  }
  const uint32_t version = document["version"] | 0;
  if (version <= config.remoteConfigVersion) {
    lastConfigSyncError = "Versione non crescente";
    configSyncLastOk = false;
    return false;
  }

  if (document["stable_ms"].is<uint32_t>()) config.stableWindowMs = constrain(document["stable_ms"].as<uint32_t>(), 100UL, 5000UL);
  if (document["display_default_on"].is<bool>()) config.displayDefaultOn = document["display_default_on"].as<bool>();
  if (document["speaker_default_on"].is<bool>()) config.speakerDefaultOn = document["speaker_default_on"].as<bool>();
  if (document["heartbeat_seconds"].is<uint32_t>()) config.heartbeatSeconds = constrain(document["heartbeat_seconds"].as<uint32_t>(), 5UL, 3600UL);
  if (document["heartbeat_watchdog_enabled"].is<bool>()) config.heartbeatWatchdogEnabled = document["heartbeat_watchdog_enabled"].as<bool>();
  if (document["heartbeat_failure_threshold"].is<uint8_t>()) config.heartbeatFailureThreshold = constrain(document["heartbeat_failure_threshold"].as<uint8_t>(), 3, 20);
  if (document["history_enabled"].is<bool>()) config.historyEnabled = document["history_enabled"].as<bool>();
  if (document["history_keep_forever"].is<bool>()) config.historyKeepForever = document["history_keep_forever"].as<bool>();
  if (document["history_retention_days"].is<uint16_t>()) config.historyRetentionDays = constrain(document["history_retention_days"].as<uint16_t>(), 1, 3650);

  JsonArray calibrations = document["calibrations"].as<JsonArray>();
  for (JsonObject remote : calibrations) {
    const int channel = remote["channel"] | -1;
    if (channel < 0 || channel >= laveggio::kChannelCount) continue;
    laveggio::ChannelCalibration &calibration = config.calibrations[channel];
    if (remote["multiplier_kg"].is<uint32_t>()) calibration.multiplierKg = constrain(remote["multiplier_kg"].as<uint32_t>(), 1UL, 100000UL);
    if (remote["tolerance"].is<uint16_t>()) calibration.tolerance = constrain(remote["tolerance"].as<uint16_t>(), 10, 1024);
    if (remote["hysteresis"].is<uint16_t>()) calibration.hysteresis = constrain(remote["hysteresis"].as<uint16_t>(), 0, 512);
    JsonArray points = remote["points"].as<JsonArray>();
    for (JsonObject point : points) {
      const int position = point["position"] | -1;
      if (position < 0 || position >= laveggio::kPositionCount) continue;
      if (point["enabled"].is<bool>()) calibration.points[position].enabled = point["enabled"].as<bool>();
      if (point["raw"].is<uint16_t>()) calibration.points[position].raw = constrain(point["raw"].as<uint16_t>(), 0, 4095);
    }
    configStore.saveCalibration(channel);
  }
  config.remoteConfigVersion = version;
  configStore.saveSettings();
  stabilityTracker.setStableWindow(config.stableWindowMs);
  configSyncLastOk = true;
  lastConfigSyncAt = timestampIso();
  logSystem("info", "config_sync_applied", "version=" + String(version));
  return true;
}

void recordWeightEvent() {
  ++sequenceNumber;
  const DeviceConfig &config = configStore.get();
  const String outboundRecord = buildSnapshotJson("scale.snapshot", false);
  String record = outboundRecord;
  if (record.endsWith("}")) {
    record.remove(record.length() - 1);
    const String delivery = config.backendUrl.isEmpty() && !config.mqttEnabled ? "local" : "requested";
    record += ",\"delivery\":" + quoted(delivery) + "}";
  }
  if (config.historyEnabled) {
    appendLine(
      weeklyLogPath("/weights", "history"),
      record,
      config.historyFileMaxMb * 1024UL * 1024UL
    );
  }
  queueOutbound(config.backendUrl, config.backendToken, outboundRecord);
  if (config.mqttEnabled && mqttClient.connected()) {
    mqttClient.publish(mqttTopic("weights").c_str(), outboundRecord.c_str(), false);
  }
  speaker.confirmWeight();
}

void sendHeartbeat() {
  const DeviceConfig &config = configStore.get();
  if (config.backendUrl.isEmpty() && !config.mqttEnabled) return;
  ++sequenceNumber;
  const String heartbeat = buildSnapshotJson("scale.heartbeat", false);
  if (!config.backendUrl.isEmpty() && !queueOutbound(
    config.backendUrl,
    config.backendToken,
    heartbeat,
    true
  )) reportHeartbeatResult(false, -3);
  if (config.mqttEnabled && mqttClient.connected()) {
    mqttClient.publish(mqttTopic("status").c_str(), heartbeat.c_str(), true);
  }
}

void processHeartbeatResult() {
  bool pending;
  bool ok;
  int code;
  portENTER_CRITICAL(&heartbeatResultMux);
  pending = heartbeatResultPending;
  ok = heartbeatResultOk;
  code = heartbeatResultCode;
  heartbeatResultPending = false;
  portEXIT_CRITICAL(&heartbeatResultMux);
  if (!pending) return;

  DeviceConfig &config = configStore.mutableConfig();
  if (ok) {
    const bool recovered = heartbeatConsecutiveFailures > 0 || config.heartbeatRestartSuppressed;
    heartbeatConsecutiveFailures = 0;
    lastHeartbeatAckAt = timestampIso();
    if (config.heartbeatRestartSuppressed) {
      config.heartbeatRestartSuppressed = false;
      configStore.saveHeartbeatRestartSuppressed();
    }
    if (recovered) logSystem("info", "heartbeat_recovered", "code=" + String(code));
    return;
  }

  if (heartbeatConsecutiveFailures < 255) ++heartbeatConsecutiveFailures;
  logSystem(
    "warning",
    "heartbeat_failed",
    "code=" + String(code) + " failures=" + String(heartbeatConsecutiveFailures)
  );
  if (!config.heartbeatWatchdogEnabled || config.heartbeatRestartSuppressed ||
      heartbeatConsecutiveFailures < config.heartbeatFailureThreshold) return;
  config.heartbeatRestartSuppressed = true;
  configStore.saveHeartbeatRestartSuppressed();
  logSystem("error", "heartbeat_watchdog_restart", "failures=" + String(heartbeatConsecutiveFailures));
  scheduledRestartMs = millis() + 1200;
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

void refreshWifiScanCache() {
  cachedWifiNetworkCount = 0;
  if (accessPointActive) return;

  WiFi.mode(WIFI_STA);
  const int found = WiFi.scanNetworks(false, true);
  if (found <= 0) {
    WiFi.scanDelete();
    return;
  }

  for (int index = 0; index < found && cachedWifiNetworkCount < kMaxCachedWifiNetworks; ++index) {
    const String ssid = WiFi.SSID(index);
    if (ssid.isEmpty()) continue;
    bool duplicate = false;
    for (uint8_t cached = 0; cached < cachedWifiNetworkCount; ++cached) {
      if (cachedWifiNetworks[cached].ssid != ssid) continue;
      duplicate = true;
      if (WiFi.RSSI(index) > cachedWifiNetworks[cached].rssi) {
        cachedWifiNetworks[cached].rssi = WiFi.RSSI(index);
        cachedWifiNetworks[cached].secure = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
      }
      break;
    }
    if (duplicate) continue;
    CachedWifiNetwork &network = cachedWifiNetworks[cachedWifiNetworkCount++];
    network.ssid = ssid;
    network.rssi = WiFi.RSSI(index);
    network.secure = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
}

String cachedWifiScanJson() {
  String json = "{\"scanning\":false,\"cached\":true,\"networks\":[";
  for (uint8_t index = 0; index < cachedWifiNetworkCount; ++index) {
    if (index) json += ',';
    json += "{\"ssid\":" + quoted(cachedWifiNetworks[index].ssid);
    json += ",\"rssi\":" + String(cachedWifiNetworks[index].rssi);
    json += ",\"secure\":" + boolJson(cachedWifiNetworks[index].secure) + "}";
  }
  json += "]}";
  return json;
}

void maintainRescueAccessPoint(uint32_t now) {
  if (WiFi.status() != WL_CONNECTED) {
    stationConnectedSinceMs = 0;
    return;
  }
  if (stationConnectedSinceMs == 0) {
    stationConnectedSinceMs = now;
    const DeviceConfig &config = configStore.get();
    requestTimeSynchronization();
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
    refreshWifiScanCache();
    startRescueAccessPoint();
    return;
  }
  if (!applyStaticNetworkConfig()) logSystem("error", "invalid_static_network");
  WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());
  const uint32_t startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 12000) delay(100);
  if (WiFi.status() == WL_CONNECTED) {
    stationConnectedSinceMs = millis();
    requestTimeSynchronization();
    if (MDNS.begin(config.hostname.c_str())) MDNS.addService("http", "tcp", 80);
    logSystem("info", "wifi_connected", WiFi.localIP().toString());
  } else {
    WiFi.disconnect(false, false);
    refreshWifiScanCache();
    startRescueAccessPoint();
  }
}

void printNetworkStatus() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  const String stationIp = connected ? WiFi.localIP().toString() : "-";
  const String gatewayIp = connected ? WiFi.gatewayIP().toString() : "-";
  const String rescueIp = accessPointActive ? WiFi.softAPIP().toString() : "-";
  Serial.printf(
    "Network: station=%s ssid=%s ip=%s gateway=%s mode=%s rescue_ap=%s ap_ip=%s\n",
    connected ? "connected" : "disconnected",
    connected ? WiFi.SSID().c_str() : configStore.get().wifiSsid.c_str(),
    stationIp.c_str(),
    gatewayIp.c_str(),
    configStore.get().useDhcp ? "DHCP" : "static",
    accessPointActive ? "active" : "inactive",
    rescueIp.c_str()
  );
}

void checkFactoryResetButton(uint32_t now) {
  const bool pressed = digitalRead(kFactoryResetButtonPin) == LOW;
  if (!pressed) {
    const uint32_t heldMs = factoryResetPressedSinceMs == 0 ? 0 : now - factoryResetPressedSinceMs;
    factoryResetPressedSinceMs = 0;
    if (!factoryResetTriggered) {
      if (heldMs >= kFactoryResetFeedbackDelayMs) display.cancelFactoryResetProgress();
      else if (heldMs >= kButtonDebounceMs) display.nextPage();
      return;
    }
    delay(150);
    ESP.restart();
    return;
  }
  if (factoryResetTriggered) return;
  if (factoryResetPressedSinceMs == 0) {
    factoryResetPressedSinceMs = now;
    return;
  }
  const uint32_t heldMs = now - factoryResetPressedSinceMs;
  if (heldMs >= kFactoryResetFeedbackDelayMs) {
    display.showFactoryResetProgress(heldMs, kFactoryResetHoldMs);
  }
  if (heldMs < kFactoryResetHoldMs) return;

  factoryResetTriggered = true;
  logSystem("warning", "factory_reset", "boot_button_held_ms=" + String(kFactoryResetHoldMs));
  const bool resetOk = configStore.factoryReset();
  Serial.printf("Factory reset: %s; release BOOT to restart\n", resetOk ? "completed" : "failed");
  display.showFactoryReset();
}

void sendSecurityHeaders() {
  webServer.sendHeader(
    "Content-Security-Policy",
    "default-src 'self'; img-src 'self' data:; style-src 'self'; script-src 'self'; "
    "connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'"
  );
  webServer.sendHeader("X-Content-Type-Options", "nosniff");
  webServer.sendHeader("X-Frame-Options", "DENY");
  webServer.sendHeader("Referrer-Policy", "no-referrer");
  webServer.sendHeader("Permissions-Policy", "camera=(), microphone=(), geolocation=()");
}

bool constantTimeEqual(const String &left, const String &right) {
  const size_t maximum = std::max(left.length(), right.length());
  uint8_t difference = static_cast<uint8_t>(left.length() ^ right.length());
  for (size_t index = 0; index < maximum; ++index) {
    const uint8_t a = index < left.length() ? left[index] : 0;
    const uint8_t b = index < right.length() ? right[index] : 0;
    difference |= a ^ b;
  }
  return difference == 0;
}

bool authorized() {
  sendSecurityHeaders();
  const uint32_t now = millis();
  if (authBlockedUntilMs != 0 && static_cast<int32_t>(authBlockedUntilMs - now) > 0) {
    webServer.sendHeader("Retry-After", "60");
    webServer.send(429, "application/json; charset=utf-8", "{\"error\":\"Troppi tentativi; riprovare tra un minuto\"}");
    return false;
  }
  const DeviceConfig &config = configStore.get();
  if (webServer.authenticate(config.adminUser.c_str(), config.adminPassword.c_str())) {
    authFailures = 0;
    authBlockedUntilMs = 0;
    if (webServer.method() != HTTP_GET && !constantTimeEqual(webServer.header("X-CSRF-Token"), csrfToken)) {
      webServer.send(403, "application/json; charset=utf-8", "{\"error\":\"Token CSRF non valido\"}");
      return false;
    }
    return true;
  }
  if (!webServer.header("Authorization").isEmpty()) {
    if (authFailures < 255) ++authFailures;
    if (authFailures >= kAuthFailureLimit) {
      authBlockedUntilMs = now + kAuthBlockMs;
      logSystem("warning", "web_auth_rate_limited");
    }
  }
  webServer.requestAuthentication(BASIC_AUTH, kAuthRealm);
  return false;
}

bool authorizedMetrics() {
  const String token = configStore.get().metricsToken;
  if (!token.isEmpty()) {
    const String authorization = webServer.header("Authorization");
    if (authorization.startsWith("Bearer ") && constantTimeEqual(authorization.substring(7), token)) return true;
    sendSecurityHeaders();
    webServer.sendHeader("WWW-Authenticate", "Bearer realm=\"CaskLogic PesaLink metrics\"");
    webServer.send(401, "text/plain; charset=utf-8", "Token metriche non valido\n");
    return false;
  }
  return authorized();
}

String buildPrometheusMetrics() {
  const DeviceConfig &config = configStore.get();
  String metrics;
  metrics.reserve(2600);
  metrics += "# HELP pesalink_up Device firmware is running.\n# TYPE pesalink_up gauge\npesalink_up 1\n";
  metrics += "# TYPE pesalink_uptime_seconds counter\npesalink_uptime_seconds " + String(millis() / 1000) + "\n";
  metrics += "# TYPE pesalink_free_heap_bytes gauge\npesalink_free_heap_bytes " + String(ESP.getFreeHeap()) + "\n";
  metrics += "# TYPE pesalink_wifi_connected gauge\npesalink_wifi_connected " + String(WiFi.status() == WL_CONNECTED ? 1 : 0) + "\n";
  metrics += "# TYPE pesalink_wifi_rssi_dbm gauge\npesalink_wifi_rssi_dbm " + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0) + "\n";
  metrics += "# TYPE pesalink_time_synchronized gauge\npesalink_time_synchronized " + String(timeSynchronized ? 1 : 0) + "\n";
  metrics += "# TYPE pesalink_sd_ready gauge\npesalink_sd_ready " + String(sdReady ? 1 : 0) + "\n";
  metrics += "# TYPE pesalink_sd_health gauge\npesalink_sd_health " + String(sdHealth.lastCheckOk ? 1 : 0) + "\n";
  metrics += "# TYPE pesalink_sd_free_bytes gauge\npesalink_sd_free_bytes " + String(sdReady ? SD.totalBytes() - SD.usedBytes() : 0) + "\n";
  metrics += "# TYPE pesalink_battery_voltage_millivolts gauge\npesalink_battery_voltage_millivolts " + String(batteryVoltageMv) + "\n";
  metrics += "# TYPE pesalink_chip_temperature_celsius gauge\npesalink_chip_temperature_celsius " + String(temperatureRead(), 1) + "\n";
  metrics += "# TYPE pesalink_integration_last_ok gauge\npesalink_integration_last_ok " + String(integrationLastOk ? 1 : 0) + "\n";
  metrics += "# TYPE pesalink_mqtt_connected gauge\npesalink_mqtt_connected " + String(mqttClient.connected() ? 1 : 0) + "\n";
  metrics += "# TYPE pesalink_weight_kg gauge\npesalink_weight_kg " + String(currentSnapshot.weightKg) + "\n";
  for (uint8_t channel = 0; channel < laveggio::kChannelCount; ++channel) {
    const String label = "{channel=\"" + String(channel) + "\"}";
    metrics += "pesalink_sensor_healthy" + label + " " + String(sensorReadings[channel].healthy() ? 1 : 0) + "\n";
    metrics += "pesalink_sensor_read_failures_total" + label + " " + String(sensorErrors[channel].readFailures) + "\n";
    metrics += "pesalink_sensor_magnet_errors_total" + label + " " + String(
      sensorErrors[channel].weakMagnetSamples + sensorErrors[channel].strongMagnetSamples
    ) + "\n";
  }
  metrics += "# pesalink_device_id " + config.deviceId + "\n";
  return metrics;
}

void sendJson(const String &json, int status = 200) {
  sendSecurityHeaders();
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
  json += ",\"time_synchronized\":" + boolJson(timeSynchronized);
  json += ",\"time_last_sync_at\":" + quoted(lastTimeSyncAt);
  const time_t nowEpoch = time(nullptr);
  json += ",\"booted_at\":" + quoted(timestampIso(nowEpoch - millis() / 1000));
  json += ",\"display_on\":" + boolJson(displayOn);
  json += ",\"speaker_on\":" + boolJson(speakerOn);
  json += ",\"speaker_ready\":" + boolJson(speaker.ready());
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
  json += ",\"sequence\":" + String(sequenceNumber);
  json += ",\"tls_verified\":" + boolJson(config.backendUrl.startsWith("https://") && !config.tlsCaCertificate.isEmpty());
  json += ",\"mtls_enabled\":" + boolJson(!config.tlsClientCertificate.isEmpty() && !config.tlsClientPrivateKey.isEmpty());
  json += ",\"heartbeat_last_ack_at\":" + quoted(lastHeartbeatAckAt);
  json += ",\"heartbeat_failures\":" + String(heartbeatConsecutiveFailures);
  json += ",\"watchdog_enabled\":" + boolJson(config.heartbeatWatchdogEnabled);
  json += ",\"watchdog_suppressed\":" + boolJson(config.heartbeatRestartSuppressed);
  json += ",\"hmac_enabled\":" + boolJson(!config.eventHmacSecret.isEmpty());
  json += ",\"mqtt_enabled\":" + boolJson(config.mqttEnabled);
  json += ",\"mqtt_connected\":" + boolJson(mqttClient.connected());
  json += ",\"config_sync_enabled\":" + boolJson(config.configSyncEnabled);
  json += ",\"config_sync_last_ok\":" + boolJson(configSyncLastOk);
  json += ",\"config_sync_last_at\":" + quoted(lastConfigSyncAt);
  json += ",\"config_version\":" + String(config.remoteConfigVersion) + "}";
  json += ",\"storage\":{\"ready\":" + boolJson(sdReady);
  json += ",\"total_bytes\":" + String(sdReady ? SD.totalBytes() : 0);
  json += ",\"used_bytes\":" + String(sdReady ? SD.usedBytes() : 0);
  json += ",\"health_ok\":" + boolJson(sdHealth.lastCheckOk);
  json += ",\"health_checks\":" + String(sdHealth.checks);
  json += ",\"health_failures\":" + String(sdHealth.failures);
  json += ",\"malformed_records\":" + String(sdHealth.malformedRecords);
  json += ",\"last_checked_at\":" + quoted(sdHealth.lastCheckedAt);
  json += ",\"last_error\":" + quoted(sdHealth.lastError) + "}";
  const String powerLabel = !config.powerSenseEnabled ? "Non configurato" :
    (externalPowerPresent ? "Rete elettrica" : "Batteria UPS");
  json += ",\"power\":{\"external\":" + boolJson(externalPowerPresent);
  json += ",\"source_label\":" + quoted(powerLabel);
  json += ",\"battery_configured\":" + boolJson(config.batterySenseEnabled);
  json += ",\"battery_present\":" + boolJson(boardHardware.status().batteryAvailable);
  json += ",\"battery_voltage_mv\":" + String(batteryVoltageMv);
  json += ",\"battery_percent\":" + String(estimatedBatteryPercent());
  json += ",\"battery_capacity_mah\":" + String(config.batteryCapacityMah);
  json += ",\"current_sensor_configured\":false";
  json += ",\"current_ma\":null";
  json += ",\"chip_temperature_c\":" + String(temperatureRead(), 1) + "}";
  const BoardHardwareStatus &board = boardHardware.status();
  json += ",\"board\":{\"model\":\"Waveshare ESP32-S3-Touch-LCD-2.8\"";
#ifdef TOUCH_CST328_PREFERRED
  json += ",\"revision_profile\":\"V1\"";
#else
  json += ",\"revision_profile\":\"V2\"";
#endif
  json += ",\"touch_available\":" + boolJson(display.touchAvailable());
  json += ",\"touch_controller\":" + quoted(display.touchControllerName());
  json += ",\"imu_available\":" + boolJson(board.imuAvailable);
  json += ",\"acceleration_g\":{\"x\":" + String(board.accelerationX, 3);
  json += ",\"y\":" + String(board.accelerationY, 3) + ",\"z\":" + String(board.accelerationZ, 3) + "}";
  json += ",\"board_temperature_c\":" + String(board.boardTemperatureC, 1);
  json += ",\"rtc_available\":" + boolJson(board.rtcAvailable);
  json += ",\"rtc_valid\":" + boolJson(board.rtcClockValid);
  json += ",\"rtc_datetime\":" + quoted(board.rtcDateTime) + "}";
  json += ",\"security\":{\"portal_auth\":\"basic\",\"portal_https\":false";
  json += ",\"csrf_protected\":true,\"rate_limit_enabled\":true";
  json += ",\"ota_signature_required\":true,\"ota_signature_algorithm\":\"ECDSA-P256-SHA256\"";
  json += ",\"ota_rollback_enabled\":true";
  json += ",\"default_credentials_active\":" + boolJson(
    config.adminUser == "admin" && config.adminPassword == "casklogic"
  );
  json += ",\"vlan_managed_by_network\":true}";
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
    json += ",\"position\":" + (decoded.valid ? String(decoded.position) : "null");
    json += ",\"magnet_weak\":" + boolJson(reading.magnetWeak());
    json += ",\"magnet_strong\":" + boolJson(reading.magnetStrong());
    json += ",\"read_failures\":" + String(sensorErrors[channel].readFailures);
    json += ",\"missing_samples\":" + String(sensorErrors[channel].missingSamples);
    json += ",\"weak_magnet_samples\":" + String(sensorErrors[channel].weakMagnetSamples);
    json += ",\"strong_magnet_samples\":" + String(sensorErrors[channel].strongMagnetSamples);
    json += ",\"unhealthy_transitions\":" + String(sensorErrors[channel].unhealthyTransitions);
    json += ",\"last_error_at\":" + quoted(sensorErrors[channel].lastErrorAt) + "}";
  }
  json += "]}";
  return json;
}

String buildSettingsJson() {
  const DeviceConfig &config = configStore.get();
  String json;
  json.reserve(5200);
  json += "{\"device_id\":" + quoted(config.deviceId);
  json += ",\"hostname\":" + quoted(config.hostname);
  json += ",\"wifi_ssid\":" + quoted(config.wifiSsid);
  json += ",\"use_dhcp\":" + boolJson(config.useDhcp);
  json += ",\"static_ip\":" + quoted(config.staticIp);
  json += ",\"gateway\":" + quoted(config.gateway);
  json += ",\"subnet\":" + quoted(config.subnet);
  json += ",\"dns\":" + quoted(config.dns);
  json += ",\"backend_url\":" + quoted(config.backendUrl);
  json += ",\"event_hmac_configured\":" + boolJson(!config.eventHmacSecret.isEmpty());
  json += ",\"metrics_token_configured\":" + boolJson(!config.metricsToken.isEmpty());
  json += ",\"tls_ca_certificate\":" + quoted(config.tlsCaCertificate);
  json += ",\"tls_client_certificate\":" + quoted(config.tlsClientCertificate);
  json += ",\"tls_client_key_configured\":" + boolJson(!config.tlsClientPrivateKey.isEmpty());
  json += ",\"notification_url\":" + quoted(config.notificationUrl);
  json += ",\"config_sync_enabled\":" + boolJson(config.configSyncEnabled);
  json += ",\"config_sync_url\":" + quoted(config.configSyncUrl);
  json += ",\"config_sync_seconds\":" + String(config.configSyncSeconds);
  json += ",\"remote_config_version\":" + String(config.remoteConfigVersion);
  json += ",\"mqtt_enabled\":" + boolJson(config.mqttEnabled);
  json += ",\"mqtt_host\":" + quoted(config.mqttHost);
  json += ",\"mqtt_port\":" + String(config.mqttPort);
  json += ",\"mqtt_username\":" + quoted(config.mqttUsername);
  json += ",\"mqtt_password_configured\":" + boolJson(!config.mqttPassword.isEmpty());
  json += ",\"mqtt_base_topic\":" + quoted(config.mqttBaseTopic);
  json += ",\"mqtt_commands_enabled\":" + boolJson(config.mqttCommandsEnabled);
  json += ",\"stable_ms\":" + String(config.stableWindowMs);
  json += ",\"heartbeat_seconds\":" + String(config.heartbeatSeconds);
  json += ",\"heartbeat_watchdog_enabled\":" + boolJson(config.heartbeatWatchdogEnabled);
  json += ",\"heartbeat_failure_threshold\":" + String(config.heartbeatFailureThreshold);
  json += ",\"ntp_server\":" + quoted(config.ntpServer);
  json += ",\"timezone\":" + quoted(config.timezone);
  json += ",\"admin_user\":" + quoted(config.adminUser);
  json += ",\"display_default_on\":" + boolJson(config.displayDefaultOn);
  json += ",\"speaker_default_on\":" + boolJson(config.speakerDefaultOn);
  json += ",\"power_sense_enabled\":" + boolJson(config.powerSenseEnabled);
  json += ",\"history_enabled\":" + boolJson(config.historyEnabled);
  json += ",\"history_keep_forever\":" + boolJson(config.historyKeepForever);
  json += ",\"history_retention_days\":" + String(config.historyRetentionDays);
  json += ",\"history_file_max_mb\":" + String(config.historyFileMaxMb);
  json += ",\"system_log_file_max_mb\":" + String(config.systemLogFileMaxMb);
  json += ",\"battery_sense_enabled\":" + boolJson(config.batterySenseEnabled);
  json += ",\"battery_divider_milli\":" + String(config.batteryDividerMilli);
  json += ",\"battery_min_mv\":" + String(config.batteryMinMv);
  json += ",\"battery_max_mv\":" + String(config.batteryMaxMv);
  json += ",\"battery_capacity_mah\":" + String(config.batteryCapacityMah);
  json += ",\"csrf_token\":" + quoted(csrfToken) + "}";
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

struct HistoryQuery {
  size_t limit = 20;
  String from;
  String to;
  String digits;
  String delivery;
  String sort = "captured_at";
  bool descending = true;
  bool minimumWeightSet = false;
  bool maximumWeightSet = false;
  bool sequenceSet = false;
  long minimumWeight = 0;
  long maximumWeight = 0;
  long sequence = 0;
};

int jsonFieldStart(const String &line, const char *field) {
  return line.indexOf(String('"') + field + "\":");
}

String jsonStringField(const String &line, const char *field) {
  const int marker = jsonFieldStart(line, field);
  if (marker < 0) return "";
  int start = line.indexOf('"', marker + strlen(field) + 3);
  if (start < 0) return "";
  ++start;
  int end = start;
  while (end < static_cast<int>(line.length())) {
    if (line[end] == '"' && (end == start || line[end - 1] != '\\')) break;
    ++end;
  }
  return line.substring(start, end);
}

long jsonLongField(const String &line, const char *field) {
  const int marker = jsonFieldStart(line, field);
  if (marker < 0) return 0;
  int start = marker + strlen(field) + 3;
  while (start < static_cast<int>(line.length()) && (line[start] == ' ' || line[start] == ':')) ++start;
  return strtol(line.c_str() + start, nullptr, 10);
}

float jsonFloatField(const String &line, const char *field) {
  const int marker = jsonFieldStart(line, field);
  if (marker < 0) return 0;
  int start = marker + strlen(field) + 3;
  while (start < static_cast<int>(line.length()) && (line[start] == ' ' || line[start] == ':')) ++start;
  return strtof(line.c_str() + start, nullptr);
}

String urlHost(const String &url) {
  int start = url.indexOf("://");
  start = start < 0 ? 0 : start + 3;
  int end = url.indexOf('/', start);
  if (end < 0) end = url.length();
  String host = url.substring(start, end);
  const int colon = host.indexOf(':');
  if (colon >= 0) host = host.substring(0, colon);
  return host;
}

String diagnosticTestJson(const char *id, const char *label, const String &status, const String &detail) {
  return "{\"id\":" + quoted(id) + ",\"label\":" + quoted(label) +
    ",\"status\":" + quoted(status) + ",\"detail\":" + quoted(detail) + "}";
}

String buildDiagnosticsJson(bool active) {
  const DeviceConfig &config = configStore.get();
  String tests[11];
  uint8_t presentSensors = 0;
  uint8_t healthySensors = 0;
  for (const laveggio::SensorReading &reading : sensorReadings) {
    if (reading.present) ++presentSensors;
    if (reading.healthy()) ++healthySensors;
  }
  const String sensorStatus = presentSensors < laveggio::kChannelCount
    ? "fail"
    : (healthySensors == laveggio::kChannelCount ? "pass" : "warn");
  tests[0] = diagnosticTestJson(
    "sensors", "Sensori AS5600", sensorStatus,
    String(presentSensors) + "/" + String(laveggio::kChannelCount) + " rilevati; " +
      String(healthySensors) + "/" + String(laveggio::kChannelCount) + " con magnete regolare"
  );
  if (active) runSdHealthCheck();
  tests[1] = diagnosticTestJson(
    "storage", "MicroSD", !sdReady ? "fail" : (sdHealth.lastCheckOk ? "pass" : "warn"),
    !sdReady ? "Non montata" : (sdHealth.lastError.isEmpty() ? "Lettura e scrittura riuscite" : sdHealth.lastError)
  );
  tests[2] = diagnosticTestJson(
    "network", "Rete Wi-Fi", WiFi.status() == WL_CONNECTED ? "pass" : "fail",
    WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() + " RSSI " + String(WiFi.RSSI()) + " dBm" : "Non connessa"
  );

  IPAddress resolved;
  bool dnsOk = false;
  if (active && WiFi.status() == WL_CONNECTED) dnsOk = WiFi.hostByName(config.ntpServer.c_str(), resolved) == 1;
  else dnsOk = WiFi.status() == WL_CONNECTED && timeSynchronized;
  tests[3] = diagnosticTestJson(
    "dns", "Risoluzione DNS", dnsOk ? "pass" : "fail",
    dnsOk ? (active ? resolved.toString() : "Operativa") : "Risoluzione non riuscita"
  );

  String backendStatus = "na";
  String backendDetail = "Non configurato";
  if (!config.backendUrl.isEmpty()) {
    backendStatus = integrationLastOk ? "pass" : "warn";
    backendDetail = "Ultimo HTTP " + String(integrationLastCode);
    if (active && WiFi.status() == WL_CONNECTED && timeSynchronized) {
      WiFiClientSecure client;
      client.setCACert(config.tlsCaCertificate.c_str());
      if (!config.tlsClientCertificate.isEmpty() && !config.tlsClientPrivateKey.isEmpty()) {
        client.setCertificate(config.tlsClientCertificate.c_str());
        client.setPrivateKey(config.tlsClientPrivateKey.c_str());
      }
      HTTPClient http;
      int code = -1;
      if (http.begin(client, config.backendUrl)) {
        http.setTimeout(2500);
        if (!config.backendToken.isEmpty()) http.addHeader("Authorization", "Bearer " + config.backendToken);
        code = http.sendRequest("HEAD");
        http.end();
      }
      backendStatus = code > 0 ? "pass" : "fail";
      backendDetail = code > 0 ? "HTTPS raggiungibile, HTTP " + String(code) : "Connessione HTTPS fallita";
    }
  }
  tests[4] = diagnosticTestJson("backend", "Gestionale", backendStatus, backendDetail);
  tests[5] = diagnosticTestJson(
    "memory", "Memoria libera", ESP.getFreeHeap() >= 60000 ? "pass" : "warn",
    String(ESP.getFreeHeap()) + " byte heap liberi"
  );
  const bool batteryAvailable = config.batterySenseEnabled && batteryVoltageMv > 0;
  tests[6] = diagnosticTestJson(
    "battery", "Batteria", !config.batterySenseEnabled ? "na" : (batteryAvailable ? "pass" : "warn"),
    !config.batterySenseEnabled ? "Monitoraggio non configurato" : String(batteryVoltageMv) + " mV"
  );
  const BoardHardwareStatus &board = boardHardware.status();
  tests[7] = diagnosticTestJson(
    "touch", "Touch capacitivo", display.touchAvailable() ? "pass" : "fail",
    display.touchAvailable() ? String(display.touchControllerName()) + " operativo" : "Controller non rilevato"
  );
  tests[8] = diagnosticTestJson(
    "speaker", "Speaker PCM5101", speaker.ready() ? "pass" : "fail",
    speaker.ready() ? (speakerOn ? "Pronto e abilitato" : "Pronto, suono disabilitato") : "I2S non inizializzato"
  );
  tests[9] = diagnosticTestJson(
    "imu", "IMU QMI8658", board.imuAvailable ? "pass" : "fail",
    board.imuAvailable ? "Accelerometro e giroscopio disponibili" : "Sensore non rilevato"
  );
  tests[10] = diagnosticTestJson(
    "rtc", "RTC PCF85063", !board.rtcAvailable ? "fail" : (board.rtcClockValid ? "pass" : "warn"),
    !board.rtcAvailable ? "RTC non rilevato" : (board.rtcClockValid ? String(board.rtcDateTime) : "Presente, ora da sincronizzare")
  );

  String json = "{\"captured_at\":" + quoted(timestampIso());
  json += ",\"active_tests\":" + boolJson(active);
  json += ",\"time_synchronized\":" + boolJson(timeSynchronized);
  json += ",\"tests\":[";
  for (uint8_t index = 0; index < 11; ++index) {
    if (index) json += ',';
    json += tests[index];
  }
  json += "]}";
  if (active) logSystem("info", "autodiagnostics_completed");
  return json;
}

struct DiagnosticChartPoint {
  String capturedAt;
  long batteryMv;
  float temperatureC;
  long rssi;
  long freeHeap;
};

String buildDailyDiagnosticsJson() {
  std::vector<DiagnosticChartPoint> points;
  points.reserve(288);
  const String cutoff = timestampIso(time(nullptr) - 86400).substring(0, 19);
  uint32_t matchingIndex = 0;
  const std::vector<String> paths = listNdjsonFiles("/logs", "system");
  for (const String &path : paths) {
    File file = SD.open(path, FILE_READ);
    if (!file) continue;
    while (file.available()) {
      String line = file.readStringUntil('\n');
      if (line.indexOf("\"event\":\"sensor_diagnostics\"") < 0) continue;
      const String capturedAt = jsonStringField(line, "captured_at");
      if (capturedAt.isEmpty() || capturedAt.substring(0, 19) < cutoff) continue;
      if ((matchingIndex++ % 5) != 0) continue;
      if (points.size() >= 288) points.erase(points.begin());
      points.push_back({
        capturedAt,
        jsonLongField(line, "battery_voltage_mv"),
        jsonFloatField(line, "chip_temperature_c"),
        jsonLongField(line, "wifi_rssi"),
        jsonLongField(line, "free_heap")
      });
    }
    file.close();
  }
  String json = "{\"current_available\":false,\"points\":[";
  for (size_t index = 0; index < points.size(); ++index) {
    if (index) json += ',';
    const DiagnosticChartPoint &point = points[index];
    json += "{\"captured_at\":" + quoted(point.capturedAt);
    json += ",\"battery_mv\":" + String(point.batteryMv);
    json += ",\"current_ma\":null";
    json += ",\"temperature_c\":" + String(point.temperatureC, 1);
    json += ",\"rssi\":" + String(point.rssi);
    json += ",\"free_heap\":" + String(point.freeHeap) + "}";
  }
  json += "]}";
  return json;
}

String buildFirmwareUpdatesJson() {
  if (!sdReady || !SD.exists("/updates/registry.ndjson")) return "{\"items\":[]}";
  String text = readTailText("/updates/registry.ndjson", 32768);
  std::vector<String> lines;
  int start = 0;
  while (start < static_cast<int>(text.length())) {
    int end = text.indexOf('\n', start);
    if (end < 0) end = text.length();
    String line = text.substring(start, end);
    line.trim();
    if (line.startsWith("{") && line.endsWith("}")) lines.push_back(line);
    start = end + 1;
  }
  String json = "{\"items\":[";
  const size_t first = lines.size() > 50 ? lines.size() - 50 : 0;
  for (size_t index = lines.size(); index > first; --index) {
    if (index != lines.size()) json += ',';
    json += lines[index - 1];
  }
  json += "]}";
  return json;
}

String buildAnonymizedConfigJson() {
  const DeviceConfig &config = configStore.get();
  String json = "{\"firmware_version\":" + quoted(kFirmwareVersion);
  json += ",\"wifi_configured\":" + boolJson(!config.wifiSsid.isEmpty());
  json += ",\"network_mode\":" + quoted(config.useDhcp ? "dhcp" : "static");
  json += ",\"backend_configured\":" + boolJson(!config.backendUrl.isEmpty());
  json += ",\"notification_configured\":" + boolJson(!config.notificationUrl.isEmpty());
  json += ",\"config_sync_configured\":" + boolJson(!config.configSyncUrl.isEmpty());
  json += ",\"mqtt_host_configured\":" + boolJson(!config.mqttHost.isEmpty());
  json += ",\"mqtt_port\":" + String(config.mqttPort);
  json += ",\"hmac_configured\":" + boolJson(!config.eventHmacSecret.isEmpty());
  json += ",\"metrics_token_configured\":" + boolJson(!config.metricsToken.isEmpty());
  json += ",\"mtls_configured\":" + boolJson(!config.tlsClientCertificate.isEmpty());
  json += ",\"remote_config_version\":" + String(config.remoteConfigVersion);
  json += ",\"history_enabled\":" + boolJson(config.historyEnabled);
  json += ",\"history_keep_forever\":" + boolJson(config.historyKeepForever);
  json += ",\"battery_sense_enabled\":" + boolJson(config.batterySenseEnabled) + "}";
  return json;
}

String anonymizeSupportText(String text) {
  const DeviceConfig &config = configStore.get();
  const String sensitiveValues[] = {
    config.deviceId,
    config.hostname,
    config.wifiSsid,
    config.staticIp,
    config.gateway,
    config.dns,
    config.backendUrl,
    config.notificationUrl,
    config.configSyncUrl,
    config.mqttHost,
    config.mqttUsername,
    config.adminUser,
    urlHost(config.backendUrl),
    urlHost(config.notificationUrl),
    urlHost(config.configSyncUrl),
    WiFi.localIP().toString(),
    WiFi.gatewayIP().toString(),
    WiFi.dnsIP().toString()
  };
  for (const String &value : sensitiveValues) {
    if (value.length() >= 3 && value != "0.0.0.0") text.replace(value, "[redacted]");
  }
  return text;
}

String buildSupportDeviceStatusJson() {
  String json = "{\"captured_at\":" + quoted(timestampIso());
  json += ",\"time_synchronized\":" + boolJson(timeSynchronized);
  json += ",\"boot_id\":" + quoted(bootId);
  json += ",\"uptime_seconds\":" + String(millis() / 1000);
  json += ",\"reset_reason\":" + quoted(resetReasonLabel());
  json += ",\"free_heap\":" + String(ESP.getFreeHeap());
  json += ",\"wifi_connected\":" + boolJson(WiFi.status() == WL_CONNECTED);
  json += ",\"wifi_rssi\":" + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0);
  json += ",\"sd_ready\":" + boolJson(sdReady);
  json += ",\"sd_health_ok\":" + boolJson(sdHealth.lastCheckOk);
  json += ",\"battery_voltage_mv\":" + String(batteryVoltageMv);
  json += ",\"chip_temperature_c\":" + String(temperatureRead(), 1);
  json += ",\"integration_last_ok\":" + boolJson(integrationLastOk);
  json += ",\"mqtt_connected\":" + boolJson(mqttClient.connected());
  json += ",\"sensors\":[";
  for (uint8_t channel = 0; channel < laveggio::kChannelCount; ++channel) {
    if (channel) json += ',';
    const laveggio::SensorReading &reading = sensorReadings[channel];
    json += "{\"channel\":" + String(channel);
    json += ",\"present\":" + boolJson(reading.present);
    json += ",\"healthy\":" + boolJson(reading.healthy());
    json += ",\"raw\":" + String(reading.raw);
    json += ",\"status\":" + String(reading.status);
    json += ",\"agc\":" + String(reading.agc);
    json += ",\"magnitude\":" + String(reading.magnitude);
    json += ",\"read_failures\":" + String(sensorErrors[channel].readFailures);
    json += ",\"unhealthy_transitions\":" + String(sensorErrors[channel].unhealthyTransitions) + "}";
  }
  json += "]}";
  return json;
}

String createSupportZip() {
  if (!sdReady || !ensureDirectoryTree("/diagnostics")) return "";
  const String path = "/diagnostics/support-" + bootId + ".zip";
  if (SD.exists(path)) SD.remove(path);
  File file = SD.open(path, FILE_WRITE);
  if (!file) return "";
  StoredZipWriter zip(file);
  bool ok = zip.add(
    "README.txt",
    "Pacchetto assistenza CaskLogic PesaLink per la pesa Laveggio Printomatic.\r\n"
    "Credenziali, token, chiavi, certificati, SSID e indirizzi IP sono esclusi.\r\n"
  );
  ok &= zip.add("configuration-anonymized.json", buildAnonymizedConfigJson());
  ok &= zip.add("device-status.json", buildSupportDeviceStatusJson());
  ok &= zip.add("autodiagnostics.json", anonymizeSupportText(buildDiagnosticsJson(false)));
  ok &= zip.add("diagnostic-series-24h.json", buildDailyDiagnosticsJson());
  ok &= zip.add("system-log-tail.ndjson", anonymizeSupportText(readLatestLogTail(65536)));
  ok &= zip.add(
    "firmware-updates.ndjson",
    SD.exists("/updates/registry.ndjson") ? readTailText("/updates/registry.ndjson", 32768) : ""
  );
  ok &= zip.finish();
  file.close();
  if (!ok) {
    SD.remove(path);
    return "";
  }
  logSystem("info", "support_package_created");
  return path;
}

String jsonDigitsField(const String &line) {
  const int marker = jsonFieldStart(line, "digits");
  if (marker < 0) return "";
  const int start = line.indexOf('[', marker);
  const int end = line.indexOf(']', start);
  if (start < 0 || end < 0) return "";
  String digits = line.substring(start + 1, end);
  digits.replace(",", ".");
  digits.replace(" ", "");
  return digits;
}

HistoryQuery historyQueryFromRequest(bool includeLimit = true) {
  HistoryQuery query;
  if (includeLimit) {
    const int requestedLimit = webServer.arg("limit").toInt();
    query.limit = constrain(requestedLimit > 0 ? requestedLimit : 20, 1, 100);
  }
  query.from = webServer.arg("from");
  query.to = webServer.arg("to");
  if (query.from.length() == 16) query.from += ":00";
  if (query.to.length() == 16) query.to += ":59";
  query.digits = webServer.arg("digits");
  query.digits.replace(" ", "");
  query.delivery = webServer.arg("delivery");
  const String requestedSort = webServer.arg("sort");
  if (requestedSort == "captured_at" || requestedSort == "weight_kg" ||
      requestedSort == "digits" || requestedSort == "sequence" || requestedSort == "delivery") {
    query.sort = requestedSort;
  }
  query.descending = webServer.arg("direction") != "asc";
  query.minimumWeightSet = webServer.hasArg("min_weight") && !webServer.arg("min_weight").isEmpty();
  query.maximumWeightSet = webServer.hasArg("max_weight") && !webServer.arg("max_weight").isEmpty();
  query.sequenceSet = webServer.hasArg("sequence") && !webServer.arg("sequence").isEmpty();
  query.minimumWeight = webServer.arg("min_weight").toInt();
  query.maximumWeight = webServer.arg("max_weight").toInt();
  query.sequence = webServer.arg("sequence").toInt();
  return query;
}

bool historyLineMatches(const String &line, const HistoryQuery &query) {
  const String capturedAt = jsonStringField(line, "captured_at").substring(0, 19);
  const long weight = jsonLongField(line, "weight_kg");
  if (!query.from.isEmpty() && capturedAt < query.from) return false;
  if (!query.to.isEmpty() && capturedAt > query.to) return false;
  if (query.minimumWeightSet && weight < query.minimumWeight) return false;
  if (query.maximumWeightSet && weight > query.maximumWeight) return false;
  if (query.sequenceSet && jsonLongField(line, "sequence") != query.sequence) return false;
  if (!query.digits.isEmpty() && jsonDigitsField(line).indexOf(query.digits) < 0) return false;
  if (!query.delivery.isEmpty() && jsonStringField(line, "delivery") != query.delivery) return false;
  return true;
}

int compareHistoryLines(const String &left, const String &right, const HistoryQuery &query) {
  if (query.sort == "weight_kg" || query.sort == "sequence") {
    const long a = jsonLongField(left, query.sort.c_str());
    const long b = jsonLongField(right, query.sort.c_str());
    if (a != b) return a < b ? -1 : 1;
  } else {
    const String a = query.sort == "digits" ? jsonDigitsField(left) : jsonStringField(left, query.sort.c_str());
    const String b = query.sort == "digits" ? jsonDigitsField(right) : jsonStringField(right, query.sort.c_str());
    const int compared = a.compareTo(b);
    if (compared != 0) return compared < 0 ? -1 : 1;
  }
  const long aSequence = jsonLongField(left, "sequence");
  const long bSequence = jsonLongField(right, "sequence");
  return aSequence == bSequence ? 0 : (aSequence < bSequence ? -1 : 1);
}

bool historyComesBefore(const String &left, const String &right, const HistoryQuery &query) {
  const int compared = compareHistoryLines(left, right, query);
  return query.descending ? compared > 0 : compared < 0;
}

bool isDefaultRecentHistoryQuery(const HistoryQuery &query) {
  return query.from.isEmpty() && query.to.isEmpty() && query.digits.isEmpty() &&
    query.delivery.isEmpty() && !query.minimumWeightSet && !query.maximumWeightSet &&
    !query.sequenceSet && query.sort == "captured_at" && query.descending;
}

String buildHistoryJson(const HistoryQuery &query) {
  const std::vector<String> paths = listNdjsonFiles("/weights", "history");
  if (paths.empty()) return "{\"items\":[]}";
  const size_t tailBytes = 128UL * 1024UL;
  std::vector<String> lines;
  lines.reserve(query.limit + 1);
  if (!isDefaultRecentHistoryQuery(query)) {
    for (const String &path : paths) {
      File file = SD.open(path, FILE_READ);
      if (!file) continue;
      while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.isEmpty() || !historyLineMatches(line, query)) continue;
        const auto position = std::lower_bound(
          lines.begin(),
          lines.end(),
          line,
          [&query](const String &left, const String &right) {
            return historyComesBefore(left, right, query);
          }
        );
        lines.insert(position, line);
        if (lines.size() > query.limit) lines.pop_back();
      }
      file.close();
    }
  } else for (auto path = paths.rbegin(); path != paths.rend() && lines.size() < query.limit; ++path) {
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
    for (auto line = fileLines.rbegin(); line != fileLines.rend() && lines.size() < query.limit; ++line) {
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

void streamFilteredHistoryExport(const HistoryQuery &query) {
  const std::vector<String> paths = listNdjsonFiles("/weights", "history");
  if (paths.empty()) {
    sendError(404, "Storico non disponibile");
    return;
  }
  sendSecurityHeaders();
  webServer.sendHeader("Cache-Control", "no-store");
  webServer.sendHeader("Content-Disposition", "attachment; filename=pesalink-pesate-filtrate.ndjson");
  webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  webServer.send(200, "application/x-ndjson", "");
  for (const String &path : paths) {
    File file = SD.open(path, FILE_READ);
    if (!file) continue;
    while (file.available()) {
      String line = file.readStringUntil('\n');
      line.trim();
      if (line.isEmpty() || !historyLineMatches(line, query)) continue;
      webServer.sendContent(line + "\n");
    }
    file.close();
  }
  webServer.sendContent("");
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
  sendSecurityHeaders();
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
  const char *requestHeaders[] = {"Authorization", "X-CSRF-Token", "X-Firmware-Size", "X-Firmware-Version"};
  webServer.collectHeaders(requestHeaders, 4);
  webServer.on("/", HTTP_GET, [] {
    if (!authorized()) return;
    sendSecurityHeaders();
    webServer.sendHeader("Cache-Control", "no-cache");
    webServer.send_P(200, "text/html; charset=utf-8", WEB_INDEX_HTML);
  });
  webServer.on("/app.css", HTTP_GET, [] {
    if (!authorized()) return;
    sendSecurityHeaders();
    webServer.sendHeader("Cache-Control", "public, max-age=3600");
    webServer.send_P(200, "text/css; charset=utf-8", WEB_APP_CSS);
  });
  webServer.on("/app.js", HTTP_GET, [] {
    if (!authorized()) return;
    sendSecurityHeaders();
    webServer.sendHeader("Cache-Control", "public, max-age=3600");
    webServer.send_P(200, "application/javascript; charset=utf-8", WEB_APP_JS);
  });
  webServer.on("/casklogicmark.png", HTTP_GET, [] {
    if (!authorized()) return;
    sendSecurityHeaders();
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
  webServer.on("/api/diagnostics", HTTP_GET, [] {
    if (!authorized()) return;
    sendJson(buildDiagnosticsJson(false));
  });
  webServer.on("/api/diagnostics/run", HTTP_POST, [] {
    if (!authorized()) return;
    sendJson(buildDiagnosticsJson(true));
  });
  webServer.on("/api/diagnostics/daily", HTTP_GET, [] {
    if (!authorized()) return;
    sendJson(buildDailyDiagnosticsJson());
  });
  webServer.on("/api/firmware/updates", HTTP_GET, [] {
    if (!authorized()) return;
    sendJson(buildFirmwareUpdatesJson());
  });
  webServer.on("/api/support-package", HTTP_GET, [] {
    if (!authorized()) return;
    const String path = createSupportZip();
    if (path.isEmpty()) {
      sendError(500, "Impossibile creare il pacchetto assistenza");
      return;
    }
    File file = SD.open(path, FILE_READ);
    if (!file) {
      SD.remove(path);
      sendError(500, "Impossibile leggere il pacchetto assistenza");
      return;
    }
    sendSecurityHeaders();
    webServer.sendHeader("Cache-Control", "no-store");
    webServer.sendHeader("Content-Disposition", "attachment; filename=pesalink-support-" + bootId + ".zip");
    webServer.streamFile(file, "application/zip");
    file.close();
    SD.remove(path);
  });
  webServer.on("/api/metrics", HTTP_GET, [] {
    if (!authorizedMetrics()) return;
    sendSecurityHeaders();
    webServer.sendHeader("Cache-Control", "no-store");
    webServer.send(200, "text/plain; version=0.0.4; charset=utf-8", buildPrometheusMetrics());
  });
  webServer.on("/api/config/sync", HTTP_POST, [] {
    if (!authorized()) return;
    if (!syncRemoteConfiguration()) {
      sendError(502, lastConfigSyncError);
      return;
    }
    sendJson("{\"ok\":true,\"version\":" + String(configStore.get().remoteConfigVersion) + "}");
  });
  webServer.on("/api/calibration", HTTP_GET, [] {
    if (!authorized()) return;
    sendJson(buildCalibrationJson());
  });
  webServer.on("/api/history", HTTP_GET, [] {
    if (!authorized()) return;
    sendJson(buildHistoryJson(historyQueryFromRequest()));
  });
  webServer.on("/api/history/export", HTTP_GET, [] {
    if (!authorized()) return;
    streamFilteredHistoryExport(historyQueryFromRequest(false));
  });
  webServer.on("/api/logs", HTTP_GET, [] {
    if (!authorized()) return;
    sendSecurityHeaders();
    webServer.sendHeader("Cache-Control", "no-store");
    webServer.send(200, "text/plain; charset=utf-8", readLatestLogTail(24000));
  });
  webServer.on("/api/logs/export", HTTP_GET, [] {
    if (!authorized()) return;
    streamNdjsonExport(
      "/logs",
      "system",
      "pesalink-log-completo.ndjson",
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
    configStore.mutableConfig().displayDefaultOn = displayOn;
    if (!configStore.saveDisplayDefaultOn()) {
      sendError(500, "Impossibile salvare lo stato predefinito del display");
      return;
    }
    display.setEnabled(displayOn);
    logSystem("info", displayOn ? "display_enabled" : "display_disabled", "persisted=true");
    sendJson("{\"ok\":true}");
  });
  webServer.on("/api/speaker", HTTP_POST, [] {
    if (!authorized()) return;
    if (!webServer.hasArg("enabled")) {
      sendError(400, "Parametro enabled mancante");
      return;
    }
    speakerOn = parseBool(webServer.arg("enabled"));
    configStore.mutableConfig().speakerDefaultOn = speakerOn;
    if (!configStore.saveSpeakerDefaultOn()) {
      sendError(500, "Impossibile salvare lo stato predefinito dello speaker");
      return;
    }
    speaker.setEnabled(speakerOn);
    if (speakerOn && parseBool(webServer.arg("test"))) speaker.testTone();
    logSystem("info", speakerOn ? "speaker_enabled" : "speaker_disabled", "persisted=true");
    sendJson("{\"ok\":true,\"ready\":" + boolJson(speaker.ready()) + "}");
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
    const String ssid = webServer.arg("wifi_ssid");
    const String password = webServer.arg("wifi_password");
    if (ssid.length() > 32 || password.length() > 63) {
      sendError(400, "SSID o password Wi-Fi oltre i limiti consentiti");
      return;
    }
    const bool useDhcp = parseBool(webServer.arg("use_dhcp"));
    IPAddress staticIp, gateway, subnet, dns;
    if (!useDhcp &&
        (!parseIpAddress(webServer.arg("static_ip"), staticIp) ||
         !parseIpAddress(webServer.arg("gateway"), gateway) ||
         !parseIpAddress(webServer.arg("subnet"), subnet) ||
         !parseIpAddress(webServer.arg("dns"), dns))) {
      sendError(400, "IP statico, gateway, subnet o DNS non validi");
      return;
    }
    DeviceConfig &config = configStore.mutableConfig();
    config.wifiSsid = ssid;
    if (!password.isEmpty()) config.wifiPassword = password;
    config.useDhcp = useDhcp;
    config.staticIp = webServer.arg("static_ip");
    config.gateway = webServer.arg("gateway");
    config.subnet = webServer.arg("subnet");
    config.dns = webServer.arg("dns");
    configStore.saveSettings();
    timeSynchronized = false;
    requestTimeSynchronization();
    logSystem("info", "network_settings_saved");
    sendJson("{\"ok\":true,\"restart_required\":true}");
    scheduledRestartMs = millis() + 1800;
  });

  webServer.on("/api/settings/integration", HTTP_POST, [] {
    if (!authorized()) return;
    const String backendUrl = webServer.arg("backend_url");
    const String notificationUrl = webServer.arg("notification_url");
    const String caCertificate = webServer.arg("tls_ca_certificate");
    const String clientCertificate = webServer.arg("tls_client_certificate");
    const String clientPrivateKey = webServer.arg("tls_client_private_key");
    const String backendToken = webServer.arg("backend_token");
    const String eventHmacSecret = webServer.arg("event_hmac_secret");
    const String metricsToken = webServer.arg("metrics_token");
    const String deviceId = webServer.arg("device_id");
    const String configSyncUrl = webServer.arg("config_sync_url");
    const String mqttHost = webServer.arg("mqtt_host");
    const String mqttUsername = webServer.arg("mqtt_username");
    const String mqttPassword = webServer.arg("mqtt_password");
    const String mqttBaseTopic = webServer.arg("mqtt_base_topic");
    if (backendUrl.length() >= sizeof(OutboundMessage::url) ||
        notificationUrl.length() >= sizeof(OutboundMessage::url) ||
        backendToken.length() >= sizeof(OutboundMessage::token) ||
        caCertificate.length() >= sizeof(OutboundMessage::caCertificate) ||
        clientCertificate.length() >= sizeof(OutboundMessage::clientCertificate) ||
        clientPrivateKey.length() >= sizeof(OutboundMessage::clientPrivateKey) ||
        deviceId.length() > 96 || configSyncUrl.length() >= sizeof(OutboundMessage::url) ||
        mqttHost.length() > 253 || mqttUsername.length() > 128 || mqttPassword.length() > 256 ||
        mqttBaseTopic.length() > 160 || eventHmacSecret.length() > 256 || metricsToken.length() > 256) {
      sendError(400, "Uno o piu campi superano i limiti del dispositivo");
      return;
    }
    if ((!backendUrl.isEmpty() && !backendUrl.startsWith("https://")) ||
        (!notificationUrl.isEmpty() && !notificationUrl.startsWith("https://")) ||
        (!configSyncUrl.isEmpty() && !configSyncUrl.startsWith("https://"))) {
      sendError(400, "Gli endpoint remoti devono usare HTTPS");
      return;
    }
    if ((!backendUrl.isEmpty() || !notificationUrl.isEmpty() || !configSyncUrl.isEmpty() ||
         parseBool(webServer.arg("mqtt_enabled"))) && caCertificate.isEmpty()) {
      sendError(400, "Certificato CA richiesto per verificare HTTPS");
      return;
    }
    if ((!eventHmacSecret.isEmpty() && eventHmacSecret.length() < 32) ||
        (!metricsToken.isEmpty() && metricsToken.length() < 24)) {
      sendError(400, "Le chiavi HMAC e metriche devono essere sufficientemente lunghe");
      return;
    }
    if (parseBool(webServer.arg("mqtt_enabled")) && (mqttHost.isEmpty() || mqttHost.indexOf("://") >= 0)) {
      sendError(400, "Host MQTT non valido");
      return;
    }
    if (mqttBaseTopic.indexOf('#') >= 0 || mqttBaseTopic.indexOf('+') >= 0) {
      sendError(400, "Il topic MQTT non puo contenere wildcard");
      return;
    }
    const String effectivePrivateKey = clientPrivateKey.isEmpty()
      ? configStore.get().tlsClientPrivateKey
      : clientPrivateKey;
    if (!clientCertificate.isEmpty() && effectivePrivateKey.isEmpty()) {
      sendError(400, "Chiave privata richiesta per abilitare mTLS");
      return;
    }
    DeviceConfig &config = configStore.mutableConfig();
    config.deviceId = deviceId;
    config.backendUrl = backendUrl;
    if (!backendToken.isEmpty()) config.backendToken = backendToken;
    if (!eventHmacSecret.isEmpty()) config.eventHmacSecret = eventHmacSecret;
    if (!metricsToken.isEmpty()) config.metricsToken = metricsToken;
    config.tlsCaCertificate = caCertificate;
    config.notificationUrl = notificationUrl;
    config.configSyncEnabled = parseBool(webServer.arg("config_sync_enabled"));
    config.configSyncUrl = configSyncUrl;
    config.configSyncSeconds = constrain(webServer.arg("config_sync_seconds").toInt(), 60, 86400);
    config.mqttEnabled = parseBool(webServer.arg("mqtt_enabled"));
    config.mqttHost = mqttHost;
    config.mqttPort = constrain(webServer.arg("mqtt_port").toInt(), 1, 65535);
    config.mqttUsername = mqttUsername;
    if (!mqttPassword.isEmpty()) config.mqttPassword = mqttPassword;
    config.mqttBaseTopic = mqttBaseTopic.isEmpty() ? "casklogic/pesalink" : mqttBaseTopic;
    config.mqttCommandsEnabled = parseBool(webServer.arg("mqtt_commands_enabled"));
    if (clientCertificate.isEmpty()) {
      config.tlsClientCertificate = "";
      config.tlsClientPrivateKey = "";
    } else {
      config.tlsClientCertificate = clientCertificate;
      config.tlsClientPrivateKey = effectivePrivateKey;
    }
    config.stableWindowMs = constrain(webServer.arg("stable_ms").toInt(), 100, 5000);
    config.heartbeatSeconds = constrain(webServer.arg("heartbeat_seconds").toInt(), 5, 3600);
    config.heartbeatWatchdogEnabled = parseBool(webServer.arg("heartbeat_watchdog_enabled"));
    config.heartbeatFailureThreshold = constrain(webServer.arg("heartbeat_failure_threshold").toInt(), 3, 20);
    config.heartbeatRestartSuppressed = false;
    heartbeatConsecutiveFailures = 0;
    stabilityTracker.setStableWindow(config.stableWindowMs);
    configStore.saveSettings();
    if (mqttClient.connected()) mqttClient.disconnect();
    configSyncAttemptedThisBoot = false;
    logSystem("info", "integration_settings_saved");
    sendJson("{\"ok\":true}");
  });

  webServer.on("/api/settings/system", HTTP_POST, [] {
    if (!authorized()) return;
    const String hostname = webServer.arg("hostname");
    const String ntpServer = webServer.arg("ntp_server");
    const String timezone = webServer.arg("timezone");
    const String adminUser = webServer.arg("admin_user");
    const String adminPassword = webServer.arg("admin_password");
    const uint16_t batteryMinMv = constrain(webServer.arg("battery_min_mv").toInt(), 2500, 4200);
    const uint16_t batteryMaxMv = constrain(webServer.arg("battery_max_mv").toInt(), 3500, 5000);
    if (hostname.isEmpty() || hostname.length() > 63 || ntpServer.isEmpty() ||
        ntpServer.length() > 128 || timezone.isEmpty() || timezone.length() > 128 ||
        adminUser.isEmpty() || adminUser.length() > 128) {
      sendError(400, "Nome host, NTP, fuso orario e utente sono obbligatori");
      return;
    }
    if (!adminPassword.isEmpty() && adminPassword.length() < 8) {
      sendError(400, "La password deve avere almeno 8 caratteri");
      return;
    }
    if (batteryMaxMv <= batteryMinMv) {
      sendError(400, "La tensione massima batteria deve superare la minima");
      return;
    }
    DeviceConfig &config = configStore.mutableConfig();
    config.hostname = hostname;
    config.ntpServer = ntpServer;
    config.timezone = timezone;
    config.adminUser = adminUser;
    if (!webServer.arg("admin_password").isEmpty()) {
      config.adminPassword = adminPassword;
    }
    config.displayDefaultOn = parseBool(webServer.arg("display_default_on"));
    config.speakerDefaultOn = parseBool(webServer.arg("speaker_default_on"));
    config.powerSenseEnabled = parseBool(webServer.arg("power_sense_enabled"));
    config.historyEnabled = parseBool(webServer.arg("history_enabled"));
    config.historyKeepForever = parseBool(webServer.arg("history_keep_forever"));
    config.historyRetentionDays = constrain(webServer.arg("history_retention_days").toInt(), 1, 3650);
    config.historyFileMaxMb = constrain(webServer.arg("history_file_max_mb").toInt(), 1, 256);
    config.systemLogFileMaxMb = constrain(webServer.arg("system_log_file_max_mb").toInt(), 1, 128);
    config.batterySenseEnabled = parseBool(webServer.arg("battery_sense_enabled"));
    config.batteryDividerMilli = constrain(webServer.arg("battery_divider_milli").toInt(), 1000, 10000);
    config.batteryMinMv = batteryMinMv;
    config.batteryMaxMv = batteryMaxMv;
    config.batteryCapacityMah = constrain(webServer.arg("battery_capacity_mah").toInt(), 100, 20000);
    configStore.saveSettings();
    displayOn = config.displayDefaultOn;
    display.setEnabled(displayOn);
    speakerOn = config.speakerDefaultOn;
    speaker.setEnabled(speakerOn);
    timeSynchronized = false;
    requestTimeSynchronization();
    pruneExpiredArchives();
    logSystem("info", "system_settings_saved");
    sendJson("{\"ok\":true}");
  });

  webServer.on("/api/wifi/scan", HTTP_GET, [] {
    if (!authorized()) return;
    if (accessPointActive) {
      sendJson(cachedWifiScanJson());
      return;
    }
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

  webServer.on("/api/ota/start", HTTP_POST, [] {
    if (!authorized()) return;
    const size_t signedSize = static_cast<size_t>(strtoull(webServer.arg("size").c_str(), nullptr, 10));
    const String target = webServer.arg("version");
    otaChunkUploadActive = beginSignedOta(signedSize, target);
    if (!otaChunkUploadActive) {
      sendError(400, otaErrorDetail);
      return;
    }
    sendJson("{\"ok\":true,\"chunk_bytes\":12288}");
  });

  webServer.on("/api/ota/chunk", HTTP_POST, [] {
    if (!authorized()) return;
    if (!otaChunkUploadActive || !Update.isRunning()) {
      sendError(409, "Nessun aggiornamento in corso");
      return;
    }
    const size_t offset = static_cast<size_t>(strtoull(webServer.arg("offset").c_str(), nullptr, 10));
    if (offset != otaReceivedBytes) {
      sendError(409, "Blocco fuori sequenza; atteso offset " + String(otaReceivedBytes));
      return;
    }
    const String encoded = webServer.arg("data");
    const size_t capacity = encoded.length() * 3 / 4 + 3;
    uint8_t *decoded = static_cast<uint8_t *>(malloc(capacity));
    if (decoded == nullptr) {
      sendError(503, "Memoria insufficiente per il blocco OTA");
      return;
    }
    size_t decodedLength = 0;
    const int decodeResult = mbedtls_base64_decode(
      decoded,
      capacity,
      &decodedLength,
      reinterpret_cast<const unsigned char *>(encoded.c_str()),
      encoded.length()
    );
    if (decodeResult != 0 || decodedLength == 0 || otaReceivedBytes + decodedLength > otaExpectedBytes) {
      free(decoded);
      sendError(400, "Blocco firmware non valido");
      return;
    }
    const size_t written = Update.write(decoded, decodedLength);
    free(decoded);
    if (written != decodedLength) {
      otaErrorDetail = Update.errorString();
      Update.abort();
      otaChunkUploadActive = false;
      sendError(500, otaErrorDetail);
      return;
    }
    otaReceivedBytes += written;
    sendJson("{\"ok\":true,\"received\":" + String(otaReceivedBytes) + "}");
  });

  webServer.on("/api/ota/finish", HTTP_POST, [] {
    if (!authorized()) return;
    if (!otaChunkUploadActive || otaReceivedBytes != otaExpectedBytes) {
      sendError(409, "Firmware incompleto");
      return;
    }
    otaChunkUploadActive = false;
    if (!finishSignedOta()) {
      sendError(500, otaErrorDetail);
      return;
    }
    sendJson("{\"ok\":true,\"restart_required\":true}");
    scheduledRestartMs = millis() + 1200;
  });

  webServer.on("/api/ota/abort", HTTP_POST, [] {
    if (!authorized()) return;
    if (Update.isRunning()) Update.abort();
    otaChunkUploadActive = false;
    otaErrorDetail = "Upload annullato";
    recordFirmwareUpdate("aborted", otaTargetLabel, otaErrorDetail);
    logSystem("warning", "ota_aborted");
    sendJson("{\"ok\":true}");
  });

  webServer.on(
    "/api/ota",
    HTTP_POST,
    [] {
      if (!otaUploadAuthorized) return;
      if (otaSucceeded) {
        sendJson("{\"ok\":true,\"restart_required\":true}");
        scheduledRestartMs = millis() + 1200;
      } else {
        sendError(500, otaErrorDetail.isEmpty() ? Update.errorString() : otaErrorDetail);
      }
      otaUploadAuthorized = false;
    },
    [] {
      HTTPUpload &upload = webServer.upload();
      if (upload.status == UPLOAD_FILE_START) {
        otaUploadAuthorized = authorized();
        if (!otaUploadAuthorized) return;
        String target = webServer.header("X-Firmware-Version");
        if (target.isEmpty()) target = upload.filename;
        const size_t signedSize = static_cast<size_t>(strtoull(webServer.header("X-Firmware-Size").c_str(), nullptr, 10));
        beginSignedOta(signedSize, target);
      } else if (!otaUploadAuthorized) {
        return;
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!Update.isRunning()) return;
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          otaErrorDetail = Update.errorString();
          Update.printError(Serial);
        } else {
          otaReceivedBytes += upload.currentSize;
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        finishSignedOta();
      } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Update.abort();
        otaErrorDetail = "Upload annullato";
        recordFirmwareUpdate("aborted", otaTargetLabel, otaErrorDetail);
        logSystem("warning", "ota_aborted");
      }
    }
  );

  webServer.onNotFound([] {
    if (!authorized()) return;
    if (webServer.uri().startsWith("/api/")) {
      sendError(404, "Endpoint non trovato");
      return;
    }
    sendSecurityHeaders();
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
  pinMode(kSdData3, OUTPUT);
  digitalWrite(kSdData3, HIGH);
  delay(10);
  sdReady = SD_MMC.setPins(kSdClock, kSdCommand, kSdData0, -1, -1, -1) &&
    SD_MMC.begin("/sdcard", true, false);
  if (sdReady) ensureSdDirectories();
}

void validatePendingOta() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  if (running == nullptr || esp_ota_get_state_partition(running, &state) != ESP_OK ||
      state != ESP_OTA_IMG_PENDING_VERIFY) return;
  if (ESP.getFreeHeap() < 50000 || outboundQueue == nullptr) {
    recordFirmwareUpdate("rollback", kFirmwareVersion, "Autotest di avvio non superato");
    delay(100);
    esp_ota_mark_app_invalid_rollback_and_reboot();
    return;
  }
  if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
    otaSignatureVerified = true;
    recordFirmwareUpdate("boot_validated", kFirmwareVersion, "Rollback annullato dopo autotest");
    logSystem("info", "ota_boot_validated", kFirmwareVersion);
  }
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

void readBatteryStatus() {
  const DeviceConfig &config = configStore.get();
  if (!config.batterySenseEnabled) {
    batteryVoltageMv = 0;
    return;
  }
  const BoardHardwareStatus &board = boardHardware.status();
  batteryVoltageMv = board.batteryAvailable ? board.batteryVoltageMv : 0;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(250);
  initializeIdentity();
  char csrf[33];
  snprintf(
    csrf,
    sizeof(csrf),
    "%08lX%08lX%08lX%08lX",
    static_cast<unsigned long>(esp_random()),
    static_cast<unsigned long>(esp_random()),
    static_cast<unsigned long>(esp_random()),
    static_cast<unsigned long>(esp_random())
  );
  csrfToken = csrf;
  configStore.begin(deviceSuffix);
  stabilityTracker.setStableWindow(configStore.get().stableWindowMs);

  pinMode(kMuxReset, OUTPUT);
  digitalWrite(kMuxReset, HIGH);
  pinMode(kPowerSensePin, INPUT);
  pinMode(kFactoryResetButtonPin, INPUT_PULLUP);
  pinMode(kBatteryPowerKeyPin, INPUT);
  pinMode(kBatteryPowerHoldPin, OUTPUT);
  digitalWrite(kBatteryPowerHoldPin, HIGH);
  analogReadResolution(12);
  Wire.begin(kI2cSda, kI2cScl, 100000);
  Wire.setTimeOut(20);
  boardHardware.begin();
  discoverMux();

  display.begin();
  displayOn = configStore.get().displayDefaultOn;
  display.setEnabled(displayOn);
  speakerOn = configStore.get().speakerDefaultOn;
  speaker.begin();
  speaker.setEnabled(speakerOn);
  initializeStorage();

  Serial.printf("Rescue AP: %s\n", kRescueSsid);
  Serial.printf("Rescue password: %s\n", kRescuePassword);
  Serial.printf("Web admin: %s (password hidden)\n", configStore.get().adminUser.c_str());

  outboundQueue = xQueueCreate(2, sizeof(OutboundMessage));
  xTaskCreate(integrationTask, "integration", 8192, nullptr, 1, nullptr);
  mqttClient.setCallback(mqttMessageReceived);
  mqttClient.setBufferSize(2048);
  mqttClient.setKeepAlive(30);
  mqttClient.setSocketTimeout(3);
  connectNetwork();
  printNetworkStatus();
  const bool showingRescueNetwork = accessPointActive;
  display.showNetworkInfo(
    showingRescueNetwork ? kRescueSsid : configStore.get().wifiSsid,
    showingRescueNetwork ? kRescuePassword : "",
    showingRescueNetwork ? WiFi.softAPIP().toString() : WiFi.localIP().toString(),
    configStore.get().adminUser,
    configStore.get().adminPassword
  );
  registerWebRoutes();
  runSdHealthCheck();
  logSystem("info", "device_started", "firmware=" + String(kFirmwareVersion));
  validatePendingOta();

  externalPowerPresent = !configStore.get().powerSenseEnabled ||
    ((digitalRead(kPowerSensePin) == HIGH) == configStore.get().powerSenseActiveHigh);
  previousExternalPowerPresent = externalPowerPresent;
  readBatteryStatus();
}

void loop() {
  const uint32_t now = millis();
  boardHardware.poll(now);
  checkFactoryResetButton(now);
  dnsServer.processNextRequest();
  webServer.handleClient();
  maintainRescueAccessPoint(now);
  processHeartbeatResult();
  pollTimeSynchronization(now);
  maintainMqtt(now);

  if (now - lastSensorReadMs >= kSensorIntervalMs) {
    lastSensorReadMs = now;
    scanSensors();
    currentSnapshot = stabilityTracker.update(
      sensorReadings,
      configStore.get().calibrations,
      now
    );
    if (currentSnapshot.changed) recordWeightEvent();
    const bool wifiConnected = WiFi.status() == WL_CONNECTED;
    const DeviceConfig &config = configStore.get();
    const String displayIp = wifiConnected ? WiFi.localIP().toString() :
      (accessPointActive ? WiFi.softAPIP().toString() : "-");
    const String displaySsid = wifiConnected ? WiFi.SSID() :
      (accessPointActive ? kRescueSsid : config.wifiSsid);
    DisplayStatus displayStatus;
    displayStatus.firmwareVersion = kFirmwareVersion;
    displayStatus.ssid = displaySsid.c_str();
    displayStatus.ipAddress = displayIp.c_str();
    displayStatus.rssi = wifiConnected ? WiFi.RSSI() : 0;
    displayStatus.wifiConnected = wifiConnected;
    displayStatus.accessPointActive = accessPointActive;
    displayStatus.sdReady = sdReady;
    displayStatus.sdUsedBytes = sdReady ? SD.usedBytes() : 0;
    displayStatus.sdTotalBytes = sdReady ? SD.totalBytes() : 0;
    displayStatus.timeSynchronized = timeSynchronized;
    displayStatus.externalPower = externalPowerPresent;
    displayStatus.batteryConfigured = config.batterySenseEnabled;
    displayStatus.batteryVoltageMv = batteryVoltageMv;
    displayStatus.batteryPercent = estimatedBatteryPercent();
    displayStatus.integrationConfigured = !config.backendUrl.isEmpty();
    displayStatus.integrationOnline = integrationLastOk;
    displayStatus.mqttEnabled = config.mqttEnabled;
    displayStatus.mqttConnected = mqttClient.connected();
    displayStatus.heartbeatFailures = heartbeatConsecutiveFailures;
    displayStatus.sequence = sequenceNumber;
    displayStatus.uptimeSeconds = now / 1000;
    displayStatus.freeHeap = ESP.getFreeHeap();
    displayStatus.chipTemperatureC = temperatureRead();
    displayStatus.speakerEnabled = speakerOn;
    displayStatus.speakerReady = speaker.ready();
    displayStatus.touchAvailable = display.touchAvailable();
    displayStatus.touchController = display.touchControllerName();
    const BoardHardwareStatus &board = boardHardware.status();
    displayStatus.imuAvailable = board.imuAvailable;
    displayStatus.accelerationX = board.accelerationX;
    displayStatus.accelerationY = board.accelerationY;
    displayStatus.accelerationZ = board.accelerationZ;
    displayStatus.rtcAvailable = board.rtcAvailable;
    displayStatus.rtcClockValid = board.rtcClockValid;
    displayStatus.rtcDateTime = board.rtcDateTime;
    display.render(sensorReadings, currentSnapshot, displayStatus);
  }

  if (now - lastScanCounterMs >= 1000) {
    scansPerSecond = scanCounter;
    scanCounter = 0;
    lastScanCounterMs = now;
    checkPowerSource();
    readBatteryStatus();
  }

  if (now - lastHeartbeatMs >= configStore.get().heartbeatSeconds * 1000UL) {
    lastHeartbeatMs = now;
    sendHeartbeat();
  }

  if (now - lastDiagnosticLogMs >= kDiagnosticLogIntervalMs) {
    lastDiagnosticLogMs = now;
    recordSensorDiagnostics();
  }

  if (now - lastSdCheckMs >= kSdCheckIntervalMs) {
    lastSdCheckMs = now;
    runSdHealthCheck();
  }

  const DeviceConfig &config = configStore.get();
  if (config.configSyncEnabled && timeSynchronized &&
      (!configSyncAttemptedThisBoot || now - lastConfigSyncMs >= config.configSyncSeconds * 1000UL)) {
    syncRemoteConfiguration();
  }

  if ((lastRetentionMs == 0 && time(nullptr) > 1700000000) ||
      now - lastRetentionMs >= kRetentionIntervalMs) {
    lastRetentionMs = now;
    pruneExpiredArchives();
  }

  if (WiFi.status() != WL_CONNECTED && now - lastReconnectMs >= kReconnectIntervalMs) {
    lastReconnectMs = now;
    if (!configStore.get().wifiSsid.isEmpty()) WiFi.reconnect();
    if (!accessPointActive) startRescueAccessPoint();
  }

  if (now - lastNetworkStatusMs >= kDiagnosticLogIntervalMs) {
    lastNetworkStatusMs = now;
    printNetworkStatus();
  }

  if (scheduledRestartMs != 0 && static_cast<int32_t>(now - scheduledRestartMs) >= 0) {
    delay(50);
    ESP.restart();
  }
  delay(1);
}
