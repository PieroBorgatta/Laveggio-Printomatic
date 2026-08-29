#include "DisplayDriver.h"

#include <SPI.h>
#include <algorithm>
#include <cstring>

namespace {

constexpr int kLcdMiso = 5;
constexpr int kLcdMosi = 6;
constexpr int kLcdSclk = 7;
constexpr int kLcdCs = 14;
constexpr int kLcdDc = 15;
constexpr int kLcdRst = 21;
constexpr int kLcdBacklight = 22;
constexpr int kSdCs = 4;
constexpr uint16_t kWidth = 172;
constexpr uint16_t kHeight = 320;
constexpr uint16_t kXOffset = 34;

constexpr uint16_t kNavy = 0x08A4;
constexpr uint16_t kBlue = 0x2AEB;
constexpr uint16_t kTeal = 0x7D14;
constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kMuted = 0x9CF3;
constexpr uint16_t kGreen = 0x2C4A;
constexpr uint16_t kAmber = 0xD443;
constexpr uint16_t kRed = 0xC249;

const uint8_t *glyphFor(char value) {
  static const uint8_t blank[5] = {0, 0, 0, 0, 0};
  static const uint8_t dash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
  static const uint8_t digits[10][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E}
  };
  static const uint8_t letters[26][5] = {
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36},
    {0x3E, 0x41, 0x41, 0x41, 0x22}, {0x7F, 0x41, 0x41, 0x22, 0x1C},
    {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, {0x7F, 0x08, 0x08, 0x08, 0x7F},
    {0x00, 0x41, 0x7F, 0x41, 0x00}, {0x20, 0x40, 0x41, 0x3F, 0x01},
    {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F},
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, {0x7F, 0x09, 0x09, 0x09, 0x06},
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01},
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, {0x1F, 0x20, 0x40, 0x20, 0x1F},
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43}
  };
  if (value >= '0' && value <= '9') return digits[value - '0'];
  if (value >= 'A' && value <= 'Z') return letters[value - 'A'];
  if (value == '-') return dash;
  return blank;
}

}  // namespace

void DisplayDriver::command(uint8_t value) {
  SPI.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
  digitalWrite(kSdCs, HIGH);
  digitalWrite(kLcdCs, LOW);
  digitalWrite(kLcdDc, LOW);
  SPI.transfer(value);
  digitalWrite(kLcdCs, HIGH);
  SPI.endTransaction();
}

void DisplayDriver::data(uint8_t value) {
  SPI.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
  digitalWrite(kSdCs, HIGH);
  digitalWrite(kLcdCs, LOW);
  digitalWrite(kLcdDc, HIGH);
  SPI.transfer(value);
  digitalWrite(kLcdCs, HIGH);
  SPI.endTransaction();
}

void DisplayDriver::setWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
  const uint16_t x0 = x + kXOffset;
  const uint16_t x1 = x0 + width - 1;
  const uint16_t y1 = y + height - 1;
  command(0x2A);
  data(x0 >> 8); data(x0 & 0xFF); data(x1 >> 8); data(x1 & 0xFF);
  command(0x2B);
  data(y >> 8); data(y & 0xFF); data(y1 >> 8); data(y1 & 0xFF);
  command(0x2C);
}

void DisplayDriver::fillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
  if (x >= kWidth || y >= kHeight || width == 0 || height == 0) return;
  if (x + width > kWidth) width = kWidth - x;
  if (y + height > kHeight) height = kHeight - y;
  setWindow(x, y, width, height);
  SPI.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
  digitalWrite(kSdCs, HIGH);
  digitalWrite(kLcdCs, LOW);
  digitalWrite(kLcdDc, HIGH);
  const uint8_t high = color >> 8;
  const uint8_t low = color & 0xFF;
  for (uint32_t index = 0; index < static_cast<uint32_t>(width) * height; ++index) {
    SPI.transfer(high);
    SPI.transfer(low);
  }
  digitalWrite(kLcdCs, HIGH);
  SPI.endTransaction();
}

void DisplayDriver::drawText(
  int x,
  int y,
  const char *text,
  uint8_t scale,
  uint16_t color,
  uint16_t background
) {
  if (x < 0 || y < 0 || x >= kWidth || y >= kHeight || scale == 0) return;
  const size_t length = strlen(text);
  if (length == 0) return;
  const uint16_t width = std::min<uint16_t>(kWidth - x, length * 6U * scale);
  const uint16_t height = std::min<uint16_t>(kHeight - y, 7U * scale);
  setWindow(x, y, width, height);

  SPI.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
  digitalWrite(kSdCs, HIGH);
  digitalWrite(kLcdCs, LOW);
  digitalWrite(kLcdDc, HIGH);
  for (uint16_t pixelY = 0; pixelY < height; ++pixelY) {
    const uint8_t glyphRow = pixelY / scale;
    for (uint16_t pixelX = 0; pixelX < width; ++pixelX) {
      const size_t characterIndex = pixelX / (6U * scale);
      const uint8_t glyphColumn = (pixelX / scale) % 6U;
      const uint8_t *glyph = glyphFor(text[characterIndex]);
      const bool set = glyphColumn < 5 && (glyph[glyphColumn] & (1U << glyphRow));
      const uint16_t pixel = set ? color : background;
      SPI.transfer(pixel >> 8);
      SPI.transfer(pixel & 0xFF);
    }
  }
  digitalWrite(kLcdCs, HIGH);
  SPI.endTransaction();
}

void DisplayDriver::begin() {
  pinMode(kSdCs, OUTPUT);
  pinMode(kLcdCs, OUTPUT);
  pinMode(kLcdDc, OUTPUT);
  pinMode(kLcdRst, OUTPUT);
  digitalWrite(kSdCs, HIGH);
  digitalWrite(kLcdCs, HIGH);
  SPI.begin(kLcdSclk, kLcdMiso, kLcdMosi, kLcdCs);
  digitalWrite(kLcdRst, LOW);
  delay(50);
  digitalWrite(kLcdRst, HIGH);
  delay(120);
  command(0x11); delay(120);
  command(0x36); data(0x00);
  command(0x3A); data(0x05);
  command(0xB0); data(0x00); data(0xE8);
  command(0xB2); data(0x0C); data(0x0C); data(0x00); data(0x33); data(0x33);
  command(0xB7); data(0x35);
  command(0xBB); data(0x35);
  command(0xC0); data(0x2C);
  command(0xC2); data(0x01);
  command(0xC3); data(0x13);
  command(0xC4); data(0x20);
  command(0xC6); data(0x0F);
  command(0xD0); data(0xA4); data(0xA1);
  command(0xD6); data(0xA1);
  command(0x21);
  command(0x29);
  ledcAttach(kLcdBacklight, 1000, 10);
  ledcWrite(kLcdBacklight, 0);
  drawFrame();
}

void DisplayDriver::drawFrame() {
  fillRect(0, 0, kWidth, kHeight, kNavy);
  fillRect(0, 0, kWidth, 30, kBlue);
  drawText(8, 7, "CASKLOGIC", 2, kWhite, kBlue);
  drawText(8, 46, "PESO", 2, kMuted, kNavy);
  fillRect(8, 74, 156, 78, 0x10C6);
  drawText(8, 174, "SENSORI", 2, kMuted, kNavy);
  lastWeightKg_ = UINT32_MAX;
}

void DisplayDriver::setEnabled(bool enabled) {
  enabled_ = enabled;
  if (enabled_) {
    drawFrame();
    ledcWrite(kLcdBacklight, 307);
  } else {
    ledcWrite(kLcdBacklight, 0);
  }
}

void DisplayDriver::render(
  const laveggio::SensorReading readings[laveggio::kChannelCount],
  const laveggio::WeightSnapshot &snapshot,
  bool wifiConnected,
  bool sdReady
) {
  if (!enabled_ || millis() - lastRenderMs_ < 120) return;
  lastRenderMs_ = millis();

  if (snapshot.weightKg != lastWeightKg_ || snapshot.valid != lastValid_) {
    fillRect(8, 74, 156, 78, 0x10C6);
    char weight[9];
    if (snapshot.valid) snprintf(weight, sizeof(weight), "%lu", snapshot.weightKg);
    else snprintf(weight, sizeof(weight), "-----");
    drawText(16, 91, weight, 4, snapshot.stable ? kGreen : kWhite, 0x10C6);
    drawText(128, 131, "KG", 2, kMuted, 0x10C6);
    lastWeightKg_ = snapshot.weightKg;
    lastValid_ = snapshot.valid;
  }

  for (uint8_t channel = 0; channel < laveggio::kChannelCount; ++channel) {
    char row[18];
    snprintf(
      row,
      sizeof(row),
      "S%u %04u %s",
      channel + 1,
      readings[channel].raw,
      readings[channel].healthy() ? "OK" : "NO"
    );
    drawText(8, 202 + channel * 23, row, 2, readings[channel].healthy() ? kGreen : kAmber, kNavy);
  }

  fillRect(0, 308, kWidth, 12, wifiConnected && sdReady ? kGreen : kRed);
}
