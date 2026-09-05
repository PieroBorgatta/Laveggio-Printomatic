#pragma once

#include <Arduino.h>
#include <atomic>
#include <driver/i2s_std.h>

class SpeakerDriver {
 public:
  bool begin();
  void setEnabled(bool enabled) { enabled_ = enabled; }
  bool enabled() const { return enabled_; }
  bool ready() const { return ready_; }
  void confirmWeight();
  void testTone();
  void alert();

 private:
  static void taskEntry(void *argument);
  void taskLoop();
  void playConfirmation();
  void playTone(uint16_t frequency, uint16_t durationMs, uint8_t amplitude);
  std::atomic_bool enabled_{true};
  bool ready_ = false;
  i2s_chan_handle_t txChannel_ = nullptr;
  TaskHandle_t task_ = nullptr;
};
