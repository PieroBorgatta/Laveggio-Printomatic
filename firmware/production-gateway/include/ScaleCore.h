#pragma once

#include <stdint.h>

namespace laveggio {

constexpr uint8_t kChannelCount = 4;
constexpr uint8_t kPositionCount = 10;
constexpr uint16_t kAngleModulo = 4096;

struct SensorReading {
  bool present = false;
  uint16_t raw = 0;
  uint8_t status = 0;
  uint8_t agc = 0;
  uint16_t magnitude = 0;

  bool magnetDetected() const { return (status & 0x20U) != 0; }
  bool magnetWeak() const { return (status & 0x10U) != 0; }
  bool magnetStrong() const { return (status & 0x08U) != 0; }
  bool healthy() const { return present && magnetDetected() && !magnetWeak() && !magnetStrong(); }
};

struct CalibrationPoint {
  bool enabled = false;
  uint16_t raw = 0;
};

struct ChannelCalibration {
  uint32_t multiplierKg = 1;
  uint16_t tolerance = 90;
  uint16_t hysteresis = 28;
  CalibrationPoint points[kPositionCount];
};

struct DecodeResult {
  bool valid = false;
  uint8_t position = 0;
  uint16_t distance = kAngleModulo;
};

struct WeightSnapshot {
  bool valid = false;
  bool stable = false;
  bool changed = false;
  uint8_t digits[kChannelCount] = {0, 0, 0, 0};
  uint32_t weightKg = 0;
  uint32_t stableForMs = 0;
};

uint16_t circularDistance(uint16_t a, uint16_t b);
DecodeResult decodePosition(
  uint16_t raw,
  const ChannelCalibration &calibration,
  int8_t previousPosition
);

class StabilityTracker {
 public:
  explicit StabilityTracker(uint32_t stableWindowMs = 600);
  void setStableWindow(uint32_t stableWindowMs);
  WeightSnapshot update(
    const SensorReading readings[kChannelCount],
    const ChannelCalibration calibrations[kChannelCount],
    uint32_t nowMs
  );
  const WeightSnapshot &snapshot() const { return snapshot_; }

 private:
  uint32_t stableWindowMs_;
  uint32_t candidateSinceMs_ = 0;
  bool candidateValid_ = false;
  uint8_t candidateDigits_[kChannelCount] = {0, 0, 0, 0};
  int8_t previousPositions_[kChannelCount] = {-1, -1, -1, -1};
  WeightSnapshot snapshot_;
};

}  // namespace laveggio
