#include "ScaleCore.h"

#include <algorithm>
#include <cstring>

namespace laveggio {

uint16_t circularDistance(uint16_t a, uint16_t b) {
  const uint16_t direct = a > b ? a - b : b - a;
  return std::min<uint16_t>(direct, kAngleModulo - direct);
}

DecodeResult decodePosition(
  uint16_t raw,
  const ChannelCalibration &calibration,
  int8_t previousPosition
) {
  if (previousPosition >= 0 && previousPosition < kPositionCount) {
    const CalibrationPoint &previous = calibration.points[previousPosition];
    if (previous.enabled) {
      const uint16_t previousDistance = circularDistance(raw, previous.raw);
      const uint16_t retentionDistance = std::min<uint16_t>(
        kAngleModulo / 2,
        calibration.tolerance + calibration.hysteresis
      );
      if (previousDistance <= retentionDistance) {
        return {true, static_cast<uint8_t>(previousPosition), previousDistance};
      }
    }
  }

  DecodeResult result;
  for (uint8_t position = 0; position < kPositionCount; ++position) {
    const CalibrationPoint &point = calibration.points[position];
    if (!point.enabled) continue;
    const uint16_t distance = circularDistance(raw, point.raw);
    if (distance < result.distance) {
      result.position = position;
      result.distance = distance;
    }
  }

  if (result.distance == kAngleModulo) return result;
  result.valid = result.distance <= calibration.tolerance;
  return result;
}

StabilityTracker::StabilityTracker(uint32_t stableWindowMs)
    : stableWindowMs_(stableWindowMs) {}

void StabilityTracker::setStableWindow(uint32_t stableWindowMs) {
  stableWindowMs_ = std::max<uint32_t>(100, stableWindowMs);
}

WeightSnapshot StabilityTracker::update(
  const SensorReading readings[kChannelCount],
  const ChannelCalibration calibrations[kChannelCount],
  uint32_t nowMs
) {
  uint8_t decoded[kChannelCount] = {0, 0, 0, 0};
  bool allValid = true;
  uint32_t weightKg = 0;

  for (uint8_t channel = 0; channel < kChannelCount; ++channel) {
    if (!readings[channel].healthy()) {
      allValid = false;
      previousPositions_[channel] = -1;
      continue;
    }
    const DecodeResult result = decodePosition(
      readings[channel].raw,
      calibrations[channel],
      previousPositions_[channel]
    );
    if (!result.valid) {
      allValid = false;
      previousPositions_[channel] = -1;
      continue;
    }
    decoded[channel] = result.position;
    previousPositions_[channel] = result.position;
    weightKg += static_cast<uint32_t>(result.position) * calibrations[channel].multiplierKg;
  }

  if (!allValid) {
    candidateValid_ = false;
    snapshot_.valid = false;
    snapshot_.stable = false;
    snapshot_.changed = false;
    snapshot_.stableForMs = 0;
    return snapshot_;
  }

  const bool sameCandidate = candidateValid_ &&
    memcmp(candidateDigits_, decoded, sizeof(candidateDigits_)) == 0;
  if (!sameCandidate) {
    memcpy(candidateDigits_, decoded, sizeof(candidateDigits_));
    candidateSinceMs_ = nowMs;
    candidateValid_ = true;
  }

  const uint32_t stableForMs = nowMs - candidateSinceMs_;
  const bool reachedStability = stableForMs >= stableWindowMs_;
  const bool stableChanged = reachedStability &&
    (!snapshot_.valid || !snapshot_.stable ||
     memcmp(snapshot_.digits, decoded, sizeof(snapshot_.digits)) != 0);

  snapshot_.valid = true;
  snapshot_.stable = reachedStability;
  snapshot_.changed = stableChanged;
  snapshot_.weightKg = weightKg;
  snapshot_.stableForMs = stableForMs;
  memcpy(snapshot_.digits, decoded, sizeof(snapshot_.digits));
  return snapshot_;
}

}  // namespace laveggio
