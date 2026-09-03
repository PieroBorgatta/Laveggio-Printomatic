#pragma once

#include <Arduino.h>

#include "ScaleCore.h"

struct DisplayStatus {
  const char *firmwareVersion = "";
  const char *ssid = "";
  const char *ipAddress = "";
  int32_t rssi = 0;
  bool wifiConnected = false;
  bool accessPointActive = false;
  bool sdReady = false;
  uint64_t sdUsedBytes = 0;
  uint64_t sdTotalBytes = 0;
  bool timeSynchronized = false;
  bool externalPower = true;
  bool batteryConfigured = false;
  uint16_t batteryVoltageMv = 0;
  uint8_t batteryPercent = 0;
  bool integrationConfigured = false;
  bool integrationOnline = false;
  bool mqttEnabled = false;
  bool mqttConnected = false;
  uint8_t heartbeatFailures = 0;
  uint32_t sequence = 0;
  uint32_t uptimeSeconds = 0;
  uint32_t freeHeap = 0;
  float chipTemperatureC = 0;
  bool speakerEnabled = false;
  bool speakerReady = false;
  bool touchAvailable = false;
  const char *touchController = "";
  bool imuAvailable = false;
  float accelerationX = 0;
  float accelerationY = 0;
  float accelerationZ = 0;
  bool rtcAvailable = false;
  bool rtcClockValid = false;
  const char *rtcDateTime = "";
};

class DisplayDriver {
 public:
  void begin();
  void setEnabled(bool enabled);
  bool enabled() const { return enabled_; }
  bool touchAvailable() const { return touchController_ != 0; }
  const char *touchControllerName() const {
    return touchController_ == 1 ? "CST328" : (touchController_ == 2 ? "CST3530" : "");
  }
  void showNetworkInfo(
    const String &ssid,
    const String &wifiPassword,
    const String &ipAddress,
    const String &adminUser,
    const String &adminPassword
  );
  void nextPage();
  void previousPage();
  void showFactoryResetProgress(uint32_t elapsedMs, uint32_t totalMs);
  void cancelFactoryResetProgress();
  void showFactoryReset();
  void render(
    const laveggio::SensorReading readings[laveggio::kChannelCount],
    const laveggio::WeightSnapshot &snapshot,
    const DisplayStatus &status
  );

 private:
  static constexpr uint8_t kPageCount = 5;
  bool enabled_ = false;
  bool resetProgressActive_ = false;
  bool pageDirty_ = true;
  uint8_t page_ = 0;
  uint32_t networkInfoUntilMs_ = 0;
  uint32_t lastRenderMs_ = 0;
  uint32_t lastResetProgressMs_ = 0;
  uint32_t lastWeightKg_ = UINT32_MAX;
  bool lastValid_ = false;
  uint8_t scrollRow_ = 0;
  uint8_t touchController_ = 0;
  bool touchPressed_ = false;
  int16_t touchStartX_ = 0;
  int16_t touchStartY_ = 0;
  int16_t touchLastX_ = 0;
  int16_t touchLastY_ = 0;
  uint32_t lastTouchPollMs_ = 0;
  void command(uint8_t value);
  void data(uint8_t value);
  void setWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
  void fillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
  void fillCard(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
  void drawLogo(uint16_t x, uint16_t y);
  void drawText(int x, int y, const char *text, uint8_t scale, uint16_t color, uint16_t background);
  void drawWrappedText(int x, int y, const String &text, uint8_t maxRows, uint16_t color);
  void drawFrame();
  void drawPageFrame(const char *title);
  void drawStatusRow(uint16_t y, const char *label, const String &value, uint16_t color);
  void drawFooter();
  void drawPill(uint16_t x, uint16_t y, const char *text, uint16_t color);
  void pollTouch();
  bool beginTouch();
  bool readTouchPoint(uint16_t &x, uint16_t &y);
  bool touchProbe(uint8_t address);
  bool touchRead16(uint8_t address, uint16_t reg, uint8_t *data, size_t length);
  bool touchWrite16(uint8_t address, uint16_t reg, const uint8_t *data, size_t length);
  bool touchRead32(uint8_t address, uint32_t reg, uint8_t *data, size_t length);
  bool touchWrite32(uint8_t address, uint32_t reg);
  void drawWeightPage(
    const laveggio::SensorReading readings[laveggio::kChannelCount],
    const laveggio::WeightSnapshot &snapshot,
    const DisplayStatus &status
  );
  void drawSensorsPage(const laveggio::SensorReading readings[laveggio::kChannelCount]);
  void drawNetworkPage(const DisplayStatus &status);
  void drawSystemPage(const DisplayStatus &status);
  void drawServicesPage(const DisplayStatus &status);
};
