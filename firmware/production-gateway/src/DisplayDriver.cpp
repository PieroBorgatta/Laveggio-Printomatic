#include "DisplayDriver.h"
#include "DisplayLogo.h"

#include <SPI.h>
#include <Wire.h>
#include <algorithm>
#include <cstring>

namespace {

constexpr int kLcdMiso = -1;
constexpr int kLcdMosi = 45;
constexpr int kLcdSclk = 40;
constexpr int kLcdCs = 42;
constexpr int kLcdDc = 41;
constexpr int kLcdRst = 39;
constexpr int kLcdBacklight = 5;
constexpr int kTouchSda = 1;
constexpr int kTouchScl = 3;
constexpr int kTouchInterrupt = 4;
constexpr int kTouchReset = 2;
constexpr uint8_t kCst328Address = 0x1A;
constexpr uint8_t kCst3530Address = 0x58;
constexpr uint16_t kWidth = 240;
constexpr uint16_t kHeight = 320;

constexpr uint16_t kNavy = 0x1189;
constexpr uint16_t kBlue = 0x2AEF;
constexpr uint16_t kLightBlue = 0xDDFB;
constexpr uint16_t kTeal = 0x2C73;
constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kSurface = 0xF7BF;
constexpr uint16_t kSurfaceBlue = 0xEFFD;
constexpr uint16_t kMuted = 0x63D1;
constexpr uint16_t kGreen = 0x2C4A;
constexpr uint16_t kGreenSurface = 0xE747;
constexpr uint16_t kAmber = 0xC423;
constexpr uint16_t kAmberSurface = 0xF6A4;
constexpr uint16_t kRed = 0xCA69;
constexpr uint16_t kRedSurface = 0xF3AE;

const uint8_t *glyphFor(char value) {
  static const uint8_t blank[5] = {0, 0, 0, 0, 0};
  static const uint8_t dash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
  static const uint8_t dot[5] = {0x00, 0x60, 0x60, 0x00, 0x00};
  static const uint8_t slash[5] = {0x60, 0x18, 0x06, 0x01, 0x00};
  static const uint8_t colon[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
  static const uint8_t percent[5] = {0x63, 0x13, 0x08, 0x64, 0x63};
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
  if (value >= 'a' && value <= 'z') value -= 32;
  if (value >= 'A' && value <= 'Z') return letters[value - 'A'];
  if (value == '-') return dash;
  if (value == '.') return dot;
  if (value == '/') return slash;
  if (value == ':') return colon;
  if (value == '%') return percent;
  return blank;
}

}  // namespace

void DisplayDriver::command(uint8_t value) {
  SPI.beginTransaction(SPISettings(80000000, MSBFIRST, SPI_MODE0));
  digitalWrite(kLcdCs, LOW);
  digitalWrite(kLcdDc, LOW);
  SPI.transfer(value);
  digitalWrite(kLcdCs, HIGH);
  SPI.endTransaction();
}

void DisplayDriver::data(uint8_t value) {
  SPI.beginTransaction(SPISettings(80000000, MSBFIRST, SPI_MODE0));
  digitalWrite(kLcdCs, LOW);
  digitalWrite(kLcdDc, HIGH);
  SPI.transfer(value);
  digitalWrite(kLcdCs, HIGH);
  SPI.endTransaction();
}

void DisplayDriver::setWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
  const uint16_t x1 = x + width - 1;
  const uint16_t y1 = y + height - 1;
  command(0x2A);
  data(x >> 8); data(x); data(x1 >> 8); data(x1);
  command(0x2B);
  data(y >> 8); data(y); data(y1 >> 8); data(y1);
  command(0x2C);
}

void DisplayDriver::fillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
  if (x >= kWidth || y >= kHeight || width == 0 || height == 0) return;
  width = std::min<uint16_t>(width, kWidth - x);
  height = std::min<uint16_t>(height, kHeight - y);
  setWindow(x, y, width, height);
  SPI.beginTransaction(SPISettings(80000000, MSBFIRST, SPI_MODE0));
  digitalWrite(kLcdCs, LOW);
  digitalWrite(kLcdDc, HIGH);
  const uint8_t high = color >> 8;
  const uint8_t low = color;
  for (uint32_t index = 0; index < static_cast<uint32_t>(width) * height; ++index) {
    SPI.transfer(high); SPI.transfer(low);
  }
  digitalWrite(kLcdCs, HIGH);
  SPI.endTransaction();
}

void DisplayDriver::fillCard(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
  fillRect(x + 3, y, width - 6, height, color);
  fillRect(x + 1, y + 2, width - 2, height - 4, color);
  fillRect(x, y + 4, width, height - 8, color);
}

void DisplayDriver::drawLogo(uint16_t x, uint16_t y) {
  setWindow(x, y, DISPLAY_LOGO_WIDTH, DISPLAY_LOGO_HEIGHT);
  SPI.beginTransaction(SPISettings(80000000, MSBFIRST, SPI_MODE0));
  digitalWrite(kLcdCs, LOW);
  digitalWrite(kLcdDc, HIGH);
  for (uint16_t index = 0; index < DISPLAY_LOGO_WIDTH * DISPLAY_LOGO_HEIGHT; ++index) {
    const uint8_t bit = 1U << (7 - index % 8);
    const bool navy = (pgm_read_byte(DISPLAY_LOGO_NAVY + index / 8) & bit) != 0;
    const bool teal = (pgm_read_byte(DISPLAY_LOGO_TEAL + index / 8) & bit) != 0;
    const uint16_t pixel = teal ? kTeal : (navy ? kNavy : kWhite);
    SPI.transfer(pixel >> 8); SPI.transfer(pixel);
  }
  digitalWrite(kLcdCs, HIGH);
  SPI.endTransaction();
}

void DisplayDriver::drawText(int x, int y, const char *text, uint8_t scale, uint16_t color, uint16_t background) {
  if (x < 0 || y < 0 || x >= kWidth || y >= kHeight || scale == 0 || text == nullptr) return;
  const size_t length = strlen(text);
  if (length == 0) return;
  const uint16_t width = std::min<uint16_t>(kWidth - x, length * 6U * scale);
  const uint16_t height = std::min<uint16_t>(kHeight - y, 7U * scale);
  setWindow(x, y, width, height);
  SPI.beginTransaction(SPISettings(80000000, MSBFIRST, SPI_MODE0));
  digitalWrite(kLcdCs, LOW);
  digitalWrite(kLcdDc, HIGH);
  for (uint16_t pixelY = 0; pixelY < height; ++pixelY) {
    const uint8_t row = pixelY / scale;
    for (uint16_t pixelX = 0; pixelX < width; ++pixelX) {
      const size_t character = pixelX / (6U * scale);
      const uint8_t column = (pixelX / scale) % 6U;
      const uint8_t *glyph = glyphFor(text[character]);
      const uint16_t pixel = column < 5 && (glyph[column] & (1U << row)) ? color : background;
      SPI.transfer(pixel >> 8); SPI.transfer(pixel);
    }
  }
  digitalWrite(kLcdCs, HIGH);
  SPI.endTransaction();
}

bool DisplayDriver::touchProbe(uint8_t address) {
  Wire1.beginTransmission(address);
  return Wire1.endTransmission(true) == 0;
}

bool DisplayDriver::touchRead16(uint8_t address, uint16_t reg, uint8_t *buffer, size_t length) {
  Wire1.beginTransmission(address); Wire1.write(reg >> 8); Wire1.write(reg);
  if (Wire1.endTransmission(true) != 0) return false;
  if (Wire1.requestFrom(address, length, true) != length) return false;
  for (size_t index = 0; index < length; ++index) buffer[index] = Wire1.read();
  return true;
}

bool DisplayDriver::touchWrite16(uint8_t address, uint16_t reg, const uint8_t *buffer, size_t length) {
  Wire1.beginTransmission(address); Wire1.write(reg >> 8); Wire1.write(reg);
  for (size_t index = 0; index < length; ++index) Wire1.write(buffer[index]);
  return Wire1.endTransmission(true) == 0;
}

bool DisplayDriver::touchRead32(uint8_t address, uint32_t reg, uint8_t *buffer, size_t length) {
  Wire1.beginTransmission(address);
  Wire1.write(reg >> 24); Wire1.write(reg >> 16); Wire1.write(reg >> 8); Wire1.write(reg);
  if (Wire1.endTransmission(false) != 0) return false;
  if (Wire1.requestFrom(address, length, true) != length) return false;
  for (size_t index = 0; index < length; ++index) buffer[index] = Wire1.read();
  return true;
}

bool DisplayDriver::touchWrite32(uint8_t address, uint32_t reg) {
  Wire1.beginTransmission(address);
  Wire1.write(reg >> 24); Wire1.write(reg >> 16); Wire1.write(reg >> 8); Wire1.write(reg);
  return Wire1.endTransmission(true) == 0;
}

bool DisplayDriver::beginTouch() {
  Wire1.begin(kTouchSda, kTouchScl, 400000);
  pinMode(kTouchInterrupt, INPUT);
  pinMode(kTouchReset, OUTPUT);
#ifdef TOUCH_CST328_PREFERRED
  const uint8_t preferred = 1;
#else
  const uint8_t preferred = 2;
#endif
  for (uint8_t attempt = 0; attempt < 2; ++attempt) {
    const uint8_t candidate = attempt == 0 ? preferred : (preferred == 1 ? 2 : 1);
    if (candidate == 1) {
      digitalWrite(kTouchReset, HIGH); delay(50); digitalWrite(kTouchReset, LOW); delay(5);
      digitalWrite(kTouchReset, HIGH); delay(80);
      if (touchProbe(kCst328Address)) { touchController_ = 1; return true; }
    } else {
      digitalWrite(kTouchReset, LOW); delay(100); digitalWrite(kTouchReset, HIGH); delay(500);
      if (touchProbe(kCst3530Address)) { touchController_ = 2; return true; }
    }
  }
  touchController_ = 0;
  return false;
}

bool DisplayDriver::readTouchPoint(uint16_t &x, uint16_t &y) {
  if (touchController_ == 1) {
    uint8_t count = 0;
    if (!touchRead16(kCst328Address, 0xD005, &count, 1)) return false;
    count &= 0x0F;
    if (count == 0 || count > 5) {
      const uint8_t clear = 0; touchWrite16(kCst328Address, 0xD005, &clear, 1); return false;
    }
    uint8_t raw[27] = {0};
    const bool ok = touchRead16(kCst328Address, 0xD000, raw, sizeof(raw));
    const uint8_t clear = 0; touchWrite16(kCst328Address, 0xD005, &clear, 1);
    if (!ok) return false;
    x = (static_cast<uint16_t>(raw[1]) << 4) | ((raw[3] & 0xF0) >> 4);
    y = (static_cast<uint16_t>(raw[2]) << 4) | (raw[3] & 0x0F);
  } else if (touchController_ == 2) {
    uint8_t raw[9] = {0};
    if (!touchRead32(kCst3530Address, 0xD0070000, raw, sizeof(raw))) return false;
    const uint8_t count = raw[3] & 0x0F;
    const bool valid = count > 0 && count <= 5 && (raw[8] & 0xF0) != 0;
    touchWrite32(kCst3530Address, 0xD00002AB);
    if (!valid) return false;
    x = (static_cast<uint16_t>(raw[7] & 0x0F) << 8) | raw[4];
    y = (static_cast<uint16_t>(raw[7] & 0xF0) << 4) | raw[5];
  } else return false;
  x = std::min<uint16_t>(x, kWidth - 1); y = std::min<uint16_t>(y, kHeight - 1);
  return true;
}

void DisplayDriver::pollTouch() {
  const uint32_t now = millis();
  if (!enabled_ || touchController_ == 0 || now - lastTouchPollMs_ < 25) return;
  lastTouchPollMs_ = now;
  uint16_t x = 0, y = 0;
  const bool pressed = readTouchPoint(x, y);
  if (pressed) {
    if (!touchPressed_) { touchStartX_ = touchLastX_ = x; touchStartY_ = touchLastY_ = y; touchPressed_ = true; }
    else { touchLastX_ = x; touchLastY_ = y; }
    return;
  }
  if (!touchPressed_) return;
  touchPressed_ = false;
  const int16_t deltaX = touchLastX_ - touchStartX_;
  const int16_t deltaY = touchLastY_ - touchStartY_;
  if (abs(deltaX) > 45 && abs(deltaX) > abs(deltaY)) deltaX < 0 ? nextPage() : previousPage();
  else if (abs(deltaY) > 35) {
    if (deltaY < 0 && scrollRow_ < 3) ++scrollRow_;
    if (deltaY > 0 && scrollRow_ > 0) --scrollRow_;
    pageDirty_ = true;
  } else if (touchStartY_ > 292) {
    const uint8_t target = std::min<uint8_t>(kPageCount - 1, touchStartX_ / 48);
    if (target != page_) { page_ = target; scrollRow_ = 0; pageDirty_ = true; }
  }
}

void DisplayDriver::begin() {
  pinMode(kLcdCs, OUTPUT); pinMode(kLcdDc, OUTPUT); pinMode(kLcdRst, OUTPUT);
  digitalWrite(kLcdCs, HIGH);
  SPI.begin(kLcdSclk, kLcdMiso, kLcdMosi, kLcdCs);
  digitalWrite(kLcdRst, LOW); delay(50); digitalWrite(kLcdRst, HIGH); delay(120);
  command(0x11); delay(120); command(0x36); data(0x00); command(0x3A); data(0x05);
  command(0xB0); data(0x00); data(0xE8);
  command(0xB2); data(0x0C); data(0x0C); data(0x00); data(0x33); data(0x33);
  command(0xB7); data(0x75); command(0xBB); data(0x1A); command(0xC0); data(0x2C);
  command(0xC2); data(0x01); data(0xFF); command(0xC3); data(0x13); command(0xC4); data(0x20);
  command(0xC6); data(0x0F); command(0xD0); data(0xA4); data(0xA1); command(0xD6); data(0xA1);
  command(0x21); command(0x29);
  ledcAttach(kLcdBacklight, 20000, 10); ledcWrite(kLcdBacklight, 0);
  beginTouch();
}

void DisplayDriver::drawPill(uint16_t x, uint16_t y, const char *text, uint16_t color) {
  const uint16_t width = strlen(text) * 6 + 12;
  fillCard(x, y, width, 17, color); drawText(x + 6, y + 5, text, 1, kWhite, color);
}

void DisplayDriver::drawFooter() {
  fillRect(0, 292, kWidth, 28, kNavy);
  for (uint8_t index = 0; index < kPageCount; ++index) {
    const uint16_t x = index * 48;
    if (index == page_) fillCard(x + 13, 301, 22, 8, kTeal);
    else fillCard(x + 20, 303, 8, 4, kMuted);
  }
}

void DisplayDriver::drawPageFrame(const char *title) {
  fillRect(0, 0, kWidth, kHeight, kWhite); fillRect(0, 0, kWidth, 50, kNavy); fillRect(0, 46, kWidth, 4, kTeal);
  drawText(12, 10, "CASKLOGIC", 2, kWhite, kNavy); drawText(12, 30, title, 1, kLightBlue, kNavy);
  char pageLabel[8]; snprintf(pageLabel, sizeof(pageLabel), "%u/%u", page_ + 1, kPageCount);
  drawText(198, 18, pageLabel, 1, kWhite, kNavy); drawFooter();
}

void DisplayDriver::drawFrame() {
  drawPageFrame("PESATURA LIVE"); fillCard(10, 61, 220, 108, kSurfaceBlue);
  drawText(22, 72, "PESO RILEVATO", 1, kMuted, kSurfaceBlue); drawText(184, 143, "KG", 2, kMuted, kSurfaceBlue);
  drawText(12, 179, "SENSORI", 1, kMuted, kWhite); lastWeightKg_ = UINT32_MAX; pageDirty_ = false;
}

void DisplayDriver::drawStatusRow(uint16_t y, const char *label, const String &value, uint16_t color) {
  fillCard(10, y, 220, 31, kSurface); drawText(20, y + 6, label, 1, kMuted, kSurface);
  String shown = value; shown.toUpperCase(); if (shown.length() > 19) shown = shown.substring(0, 19);
  drawText(104, y + 6, shown.c_str(), 1, color, kSurface);
}

void DisplayDriver::setEnabled(bool enabled) {
  enabled_ = enabled;
  if (enabled_) { page_ = 0; scrollRow_ = 0; pageDirty_ = true; command(0x29); ledcWrite(kLcdBacklight, 650); }
  else { command(0x28); ledcWrite(kLcdBacklight, 0); }
}

void DisplayDriver::nextPage() {
  if (!enabled_ || resetProgressActive_) return;
  page_ = (page_ + 1) % kPageCount; scrollRow_ = 0; networkInfoUntilMs_ = 0; lastRenderMs_ = 0;
  lastWeightKg_ = UINT32_MAX; pageDirty_ = true;
}

void DisplayDriver::previousPage() {
  if (!enabled_ || resetProgressActive_) return;
  page_ = page_ == 0 ? kPageCount - 1 : page_ - 1; scrollRow_ = 0; networkInfoUntilMs_ = 0;
  lastRenderMs_ = 0; lastWeightKg_ = UINT32_MAX; pageDirty_ = true;
}

void DisplayDriver::drawWrappedText(int x, int y, const String &text, uint8_t maxRows, uint16_t color) {
  String normalized = text; normalized.toUpperCase(); constexpr size_t kCharactersPerRow = 35;
  for (uint8_t row = 0; row < maxRows; ++row) {
    const size_t start = row * kCharactersPerRow; if (start >= normalized.length()) break;
    const String line = normalized.substring(start, start + kCharactersPerRow);
    drawText(x, y + row * 12, line.c_str(), 1, color, kWhite);
  }
}

void DisplayDriver::showNetworkInfo(const String &ssid, const String &wifiPassword, const String &ipAddress, const String &adminUser, const String &adminPassword) {
  if (!enabled_) return;
  networkInfoUntilMs_ = millis() + 30000; drawPageFrame("PRIMO ACCESSO");
  fillCard(10, 62, 220, 58, kSurfaceBlue); drawText(20, 72, "RETE WIFI", 1, kMuted, kSurfaceBlue); drawWrappedText(20, 91, ssid, 2, kNavy);
  fillCard(10, 128, 220, 54, kGreenSurface); drawText(20, 138, "APRI NEL BROWSER", 1, kGreen, kGreenSurface);
  drawWrappedText(20, 157, String("HTTP://") + ipAddress, 1, kNavy);
  fillCard(10, 190, 220, 88, kSurface); drawText(20, 201, "UTENTE", 1, kMuted, kSurface); drawWrappedText(82, 201, adminUser, 1, kNavy);
  drawText(20, 223, "PASSWORD", 1, kMuted, kSurface); drawWrappedText(82, 223, adminPassword, 1, kNavy);
  if (!wifiPassword.isEmpty()) { drawText(20, 249, "PASSWORD WIFI", 1, kMuted, kSurface); drawWrappedText(116, 249, wifiPassword, 1, kNavy); }
}

void DisplayDriver::showFactoryReset() {
  resetProgressActive_ = true; networkInfoUntilMs_ = millis() + 3600000UL; ledcWrite(kLcdBacklight, 650);
  fillRect(0, 0, kWidth, kHeight, kWhite); fillRect(0, 0, kWidth, 54, kRed); drawText(14, 17, "RIPRISTINO", 2, kWhite, kRed);
  fillCard(14, 92, 212, 110, kRedSurface); drawText(28, 112, "CONFIGURAZIONE", 2, kNavy, kRedSurface);
  drawText(28, 146, "AZZERATA", 3, kRed, kRedSurface); drawText(28, 220, "RILASCIA BOOT", 2, kAmber, kWhite);
  drawText(28, 248, "PER RIAVVIARE", 2, kMuted, kWhite);
}

void DisplayDriver::showFactoryResetProgress(uint32_t elapsedMs, uint32_t totalMs) {
  if (totalMs == 0) return;
  const uint32_t now = millis(); if (resetProgressActive_ && now - lastResetProgressMs_ < 100) return; lastResetProgressMs_ = now;
  if (!resetProgressActive_) {
    resetProgressActive_ = true; networkInfoUntilMs_ = millis() + 3600000UL; ledcWrite(kLcdBacklight, 650);
    fillRect(0, 0, kWidth, kHeight, kWhite); fillRect(0, 0, kWidth, 54, kAmber); drawText(14, 17, "RIPRISTINO", 2, kNavy, kAmber);
    drawText(18, 82, "TIENI PREMUTO BOOT", 2, kNavy, kWhite); drawText(18, 245, "RILASCIA PER ANNULLARE", 1, kMuted, kWhite);
  }
  elapsedMs = std::min(elapsedMs, totalMs); const uint16_t progressWidth = static_cast<uint16_t>((elapsedMs * 204ULL) / totalMs);
  fillCard(16, 172, 208, 28, kMuted); fillCard(18, 174, 204, 24, kSurface); if (progressWidth > 0) fillCard(18, 174, progressWidth, 24, kAmber);
  fillRect(18, 120, 204, 35, kWhite); char countdown[20];
  snprintf(countdown, sizeof(countdown), "MANCANO %LU S", static_cast<unsigned long>((totalMs - elapsedMs + 999) / 1000));
  drawText(28, 130, countdown, 2, kNavy, kWhite);
}

void DisplayDriver::cancelFactoryResetProgress() {
  if (!resetProgressActive_) return;
  resetProgressActive_ = false; lastResetProgressMs_ = 0; networkInfoUntilMs_ = 0; lastRenderMs_ = 0;
  lastWeightKg_ = UINT32_MAX; pageDirty_ = true; ledcWrite(kLcdBacklight, enabled_ ? 650 : 0);
}

void DisplayDriver::render(const laveggio::SensorReading readings[laveggio::kChannelCount], const laveggio::WeightSnapshot &snapshot, const DisplayStatus &status) {
  pollTouch();
  if (!enabled_ || resetProgressActive_ || static_cast<int32_t>(networkInfoUntilMs_ - millis()) > 0) return;
  if (networkInfoUntilMs_ != 0) { networkInfoUntilMs_ = 0; pageDirty_ = true; }
  const uint32_t interval = page_ == 0 ? 120 : 850;
  if (!pageDirty_ && millis() - lastRenderMs_ < interval) return; lastRenderMs_ = millis();
  switch (page_) { case 0: drawWeightPage(readings, snapshot, status); break; case 1: drawSensorsPage(readings); break;
    case 2: drawNetworkPage(status); break; case 3: drawSystemPage(status); break; default: drawServicesPage(status); break; }
  pageDirty_ = false;
}

void DisplayDriver::drawWeightPage(const laveggio::SensorReading readings[laveggio::kChannelCount], const laveggio::WeightSnapshot &snapshot, const DisplayStatus &status) {
  if (pageDirty_ || lastWeightKg_ == UINT32_MAX) drawFrame();
  if (snapshot.weightKg != lastWeightKg_ || snapshot.valid != lastValid_) {
    const uint16_t background = snapshot.stable ? kGreenSurface : kSurfaceBlue;
    fillCard(10, 61, 220, 108, background); drawText(22, 72, "PESO RILEVATO", 1, kMuted, background);
    char weight[10]; if (snapshot.valid) snprintf(weight, sizeof(weight), "%lu", snapshot.weightKg); else snprintf(weight, sizeof(weight), "-----");
    drawText(20, 99, weight, 5, snapshot.stable ? kGreen : kNavy, background); drawText(184, 143, "KG", 2, kMuted, background);
    drawPill(146, 72, snapshot.stable ? "CONFERMATA" : "IN LETTURA", snapshot.stable ? kGreen : kBlue);
    lastWeightKg_ = snapshot.weightKg; lastValid_ = snapshot.valid;
  }
  for (uint8_t channel = 0; channel < laveggio::kChannelCount; ++channel) {
    const uint16_t x = 10 + (channel % 2) * 112; const uint16_t y = 193 + (channel / 2) * 43;
    const uint16_t background = readings[channel].healthy() ? kGreenSurface : kAmberSurface; fillCard(x, y, 108, 36, background);
    char sensor[16]; snprintf(sensor, sizeof(sensor), "S%u  %04u", channel + 1, readings[channel].raw);
    drawText(x + 10, y + 8, sensor, 1, readings[channel].healthy() ? kGreen : kAmber, background);
    drawText(x + 10, y + 20, readings[channel].healthy() ? "REGOLARE" : "VERIFICA", 1, kMuted, background);
  }
  fillRect(0, 288, kWidth, 4, status.wifiConnected && status.sdReady ? kGreen : kRed); drawFooter();
}

void DisplayDriver::drawSensorsPage(const laveggio::SensorReading readings[laveggio::kChannelCount]) {
  if (pageDirty_) drawPageFrame("SENSORI AS5600"); fillRect(0, 50, kWidth, 242, kWhite);
  const uint8_t first = std::min<uint8_t>(scrollRow_, 1);
  for (uint8_t visible = 0; visible < 4 && first + visible < laveggio::kChannelCount; ++visible) {
    const uint8_t channel = first + visible; const uint16_t y = 58 + visible * 57;
    const uint16_t background = readings[channel].healthy() ? kGreenSurface : kAmberSurface; fillCard(10, y, 220, 49, background);
    char title[24]; snprintf(title, sizeof(title), "SENSORE %u   RAW %04U", channel + 1, readings[channel].raw);
    drawText(20, y + 8, title, 1, readings[channel].healthy() ? kGreen : kAmber, background);
    const char *magnet = !readings[channel].present ? "ASSENTE" : readings[channel].magnetWeak() ? "MAGNETE DEBOLE" : readings[channel].magnetStrong() ? "MAGNETE FORTE" : "MAGNETE REGOLARE";
    drawText(20, y + 23, magnet, 1, kNavy, background); char detail[28];
    snprintf(detail, sizeof(detail), "MAG %U  AGC %U", readings[channel].magnitude, readings[channel].agc);
    drawText(20, y + 35, detail, 1, kMuted, background);
  }
  drawFooter();
}

void DisplayDriver::drawNetworkPage(const DisplayStatus &status) {
  if (pageDirty_) drawPageFrame("RETE E SINCRONIA"); fillRect(0, 50, kWidth, 242, kWhite);
  struct Row { const char *label; String value; uint16_t color; } rows[] = {
    {"STATO", status.wifiConnected ? "CONNESSA" : (status.accessPointActive ? "ACCESS POINT" : "OFFLINE"), status.wifiConnected ? kGreen : kAmber},
    {"SSID", status.ssid, kNavy}, {"INDIRIZZO", status.ipAddress, kNavy},
    {"SEGNALE", status.wifiConnected ? String(status.rssi) + " DBM" : "--", status.rssi >= -70 ? kGreen : kAmber},
    {"ORA", status.timeSynchronized ? "SINCRONIZZATA" : "IN ATTESA", status.timeSynchronized ? kGreen : kAmber},
    {"RTC", !status.rtcAvailable ? "NON RILEVATO" : (status.rtcClockValid ? status.rtcDateTime : "DA SINCRONIZZARE"), status.rtcClockValid ? kGreen : kAmber}
  };
  const uint8_t first = std::min<uint8_t>(scrollRow_, 2);
  for (uint8_t visible = 0; visible < 5 && first + visible < 6; ++visible) drawStatusRow(58 + visible * 44, rows[first + visible].label, rows[first + visible].value, rows[first + visible].color);
  drawFooter();
}

void DisplayDriver::drawSystemPage(const DisplayStatus &status) {
  if (pageDirty_) drawPageFrame("SISTEMA E BATTERIA"); fillRect(0, 50, kWidth, 242, kWhite);
  const uint16_t batteryBackground = status.batteryConfigured ? kGreenSurface : kSurface; fillCard(10, 58, 220, 64, batteryBackground);
  drawText(20, 68, "BATTERIA", 1, kMuted, batteryBackground); char battery[28];
  if (status.batteryConfigured) snprintf(battery, sizeof(battery), "%U%%  %U MV", status.batteryPercent, status.batteryVoltageMv); else snprintf(battery, sizeof(battery), "NON RILEVATA");
  drawText(20, 88, battery, 2, status.batteryConfigured ? kGreen : kMuted, batteryBackground);
  if (status.batteryConfigured) { fillCard(144, 88, 70, 12, kMuted); fillCard(146, 90, status.batteryPercent * 66 / 100, 8, status.batteryPercent < 20 ? kRed : kTeal); }
  struct Row { const char *label; String value; uint16_t color; } rows[] = {
    {"MICROSD", status.sdReady ? "PRONTA" : "ERRORE", status.sdReady ? kGreen : kRed},
    {"MEMORIA", String(status.freeHeap / 1024) + " KB", status.freeHeap > 100000 ? kGreen : kAmber},
    {"CPU", String(status.chipTemperatureC, 1) + " C", status.chipTemperatureC < 70 ? kGreen : kAmber},
    {"IMU", status.imuAvailable ? "QMI8658 OK" : "NON RILEVATA", status.imuAvailable ? kGreen : kAmber},
    {"ASSETTO", status.imuAvailable ? String(status.accelerationX, 1) + "/" + String(status.accelerationY, 1) + "/" + String(status.accelerationZ, 1) + " G" : "--", kNavy},
    {"UPTIME", String(status.uptimeSeconds / 3600) + " H", kNavy},
    {"TOUCH", status.touchAvailable ? status.touchController : "NON RILEVATO", status.touchAvailable ? kGreen : kRed}
  };
  const uint8_t first = std::min<uint8_t>(scrollRow_, 3);
  for (uint8_t visible = 0; visible < 4 && first + visible < 7; ++visible) drawStatusRow(130 + visible * 39, rows[first + visible].label, rows[first + visible].value, rows[first + visible].color);
  drawFooter();
}

void DisplayDriver::drawServicesPage(const DisplayStatus &status) {
  if (pageDirty_) drawPageFrame("SERVIZI CASKLOGIC"); fillRect(0, 50, kWidth, 242, kWhite);
  struct Row { const char *label; String value; uint16_t color; } rows[] = {
    {"API", !status.integrationConfigured ? "NON CONFIGURATA" : (status.integrationOnline ? "ONLINE" : "OFFLINE"), status.integrationOnline ? kGreen : kAmber},
    {"MQTT", !status.mqttEnabled ? "DISATTIVO" : (status.mqttConnected ? "CONNESSO" : "OFFLINE"), status.mqttConnected ? kGreen : kAmber},
    {"HEARTBEAT", String(status.heartbeatFailures) + " ERRORI", status.heartbeatFailures == 0 ? kGreen : kAmber},
    {"SEQUENZA", String(status.sequence), kNavy},
    {"SPEAKER", !status.speakerReady ? "NON RILEVATO" : (status.speakerEnabled ? "ATTIVO" : "SILENZIOSO"), status.speakerEnabled ? kGreen : kMuted},
    {"DISPLAY", enabled_ ? "ATTIVO" : "SPENTO", kGreen}, {"FIRMWARE", status.firmwareVersion, kNavy}
  };
  const uint8_t first = std::min<uint8_t>(scrollRow_, 2);
  for (uint8_t visible = 0; visible < 5 && first + visible < 7; ++visible) drawStatusRow(58 + visible * 44, rows[first + visible].label, rows[first + visible].value, rows[first + visible].color);
  drawFooter();
}
