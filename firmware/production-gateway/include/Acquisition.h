#pragma once
#include <Arduino.h>
#include <atomic>
#include "BoardHardware.h"
#include "DeviceConfig.h"

struct AcquisitionConfig {
  laveggio::ChannelCalibration calibration[4];
  uint8_t order[4];
  uint32_t revision;
  uint32_t stableMs;
  laveggio::ClosureConfig closure;
};
struct AcquisitionState {
  laveggio::SensorReading sensors[4]; // Physical mux channels; calibration stays with the magnet.
  laveggio::WeightSnapshot weight;
  BoardHardwareStatus board;
  uint32_t readFailures[4]={}, missingSamples[4]={}, weakSamples[4]={}, strongSamples[4]={}, unhealthyTransitions[4]={};
  uint16_t noise[4] = {};
  uint16_t samples[4] = {};
  uint32_t capturedMs = 0, scans = 0, maxGapMs = 0, overruns = 0, droppedEvents = 0;
  uint32_t closureCount = 0, closureAtMs = 0;
  float vibrationG = 0, closurePeakG = 0;
  bool closurePending = false;
  int8_t muxAddress = -1;
};
struct CapturePacket {
  AcquisitionState state;
  uint32_t revision;
  uint32_t multipliers[4];
  uint8_t order[4];
  time_t epoch;
  bool completed = false;
  bool closure = false;
  uint8_t timeSource = 0;
};
class Acquisition {
 public:
  bool begin(BoardHardware *board, const DeviceConfig &config);
  void configure(const DeviceConfig &config);
  AcquisitionState snapshot();
  bool take(CapturePacket &packet, TickType_t wait = 0);
  void syncRtc(time_t epoch);
  void setTimeSource(uint8_t source) { timeSource_=source; }
 private:
  static void entry(void *self);
  void run();
  bool read(uint8_t reg, uint8_t *data, size_t n);
  QueueHandle_t configQueue_ = nullptr, eventQueue_ = nullptr, rtcQueue_ = nullptr;
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  AcquisitionState published_;
  BoardHardware *board_ = nullptr;
  std::atomic<uint8_t> timeSource_{0};
};
