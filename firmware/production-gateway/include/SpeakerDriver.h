#pragma once

#include <Arduino.h>
#include <driver/i2s_std.h>

class SpeakerDriver {
 public:
  bool begin();
  void setEnabled(bool enabled) { enabled_ = enabled; }
  void setVolume(uint8_t percent);
  bool enabled() const { return enabled_; }
  bool ready() const { return ready_; }
  void confirmWeight();
  void testTone();

 private:
  static void taskEntry(void *argument);
  bool initializeChannel();
  void taskLoop();
  void playConfirmation();
  void playTestTone();
  void playTone(uint16_t frequency, uint16_t durationMs, uint8_t amplitude);
  void stopOutput();
  bool enabled_ = true;
  uint8_t volumePercent_ = 100;
  bool ready_ = false;
  i2s_chan_handle_t txChannel_ = nullptr;
  TaskHandle_t task_ = nullptr;
};
