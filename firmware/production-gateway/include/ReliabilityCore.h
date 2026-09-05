#pragma once
#include <stdint.h>
#include "ScaleCore.h"

namespace laveggio {
struct ClosureConfig {
  bool enabled = false;
  bool completeWeight = false;
  float thresholdG = 0.35f;
  float quietG = 0.08f;
  uint32_t quietMs = 400;
  uint32_t timeoutMs = 3000;
  uint32_t cooldownMs = 2000;
};
// A candidate requires an impulse, subsequent quiet and a fresh stable reading.
// This is a mechanical hypothesis, never a metrological confirmation.
class ClosureDetector {
 public:
  bool update(float x, float y, float z, bool valid, bool stable, uint32_t now, const ClosureConfig &config);
  void reset();
  float vibrationG = 0;
  float peakG = 0;
  uint32_t count = 0;
  uint32_t lastDetectedMs = 0;
  bool pending = false;
 private:
  float bx_ = 0, by_ = 0, bz_ = 0;
  bool initialized_ = false, cooling_ = false, quiet_ = false;
  uint32_t previousMs_ = 0, impulseMs_ = 0, quietSince_ = 0;
};
bool validSensorOrder(const uint8_t order[kChannelCount]);
bool calibrationSeparated(const ChannelCalibration &calibration);
uint8_t estimateBatteryPercent(uint16_t mv, uint16_t minimum, uint16_t maximum);
int64_t rtcUtcEpoch(const uint8_t raw[7], uint8_t control);
}
