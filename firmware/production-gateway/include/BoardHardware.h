#pragma once

#include <Arduino.h>

struct BoardHardwareStatus {
  bool batteryAvailable = false;
  uint16_t batteryVoltageMv = 0;
  bool imuAvailable = false;
  float accelerationX = 0;
  float accelerationY = 0;
  float accelerationZ = 0;
  float boardTemperatureC = 0;
  time_t rtcEpoch = 0;
  bool rtcUtcWritten = false;
  bool rtcAvailable = false;
  bool rtcClockValid = false;
  char rtcDateTime[20] = "";
};

class BoardHardware {
 public:
  void begin();
  void poll(uint32_t nowMs);
  bool pollMotion(uint32_t nowMs, bool enabled);
  const BoardHardwareStatus &status() const { return status_; }
  void synchronizeRtc(time_t epoch);

 private:
  BoardHardwareStatus status_;
  uint32_t lastPollMs_ = 0;
  uint32_t lastMotionMs_ = 0;
  bool readRegister(uint8_t address, uint8_t reg, uint8_t *data, size_t length);
  bool writeRegister(uint8_t address, uint8_t reg, uint8_t value);
  void initializeImu();
  void readBattery();
  void readImu();
  void readRtc();
};
