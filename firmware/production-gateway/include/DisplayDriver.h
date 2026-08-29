#pragma once

#include <Arduino.h>

#include "ScaleCore.h"

class DisplayDriver {
 public:
  void begin();
  void setEnabled(bool enabled);
  bool enabled() const { return enabled_; }
  void render(
    const laveggio::SensorReading readings[laveggio::kChannelCount],
    const laveggio::WeightSnapshot &snapshot,
    bool wifiConnected,
    bool sdReady
  );

 private:
  bool enabled_ = false;
  uint32_t lastRenderMs_ = 0;
  uint32_t lastWeightKg_ = UINT32_MAX;
  bool lastValid_ = false;
  void command(uint8_t value);
  void data(uint8_t value);
  void setWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
  void fillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
  void drawText(int x, int y, const char *text, uint8_t scale, uint16_t color, uint16_t background);
  void drawFrame();
};
