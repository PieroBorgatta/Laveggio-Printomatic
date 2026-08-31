#include "DisplayDriver.h"
#include "DisplayLogo.h"

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

constexpr uint16_t kNavy = 0x1189;
constexpr uint16_t kBlue = 0x2AEF;
constexpr uint16_t kTeal = 0x2C73;
constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kSurface = 0xF7BF;
constexpr uint16_t kMuted = 0x63D1;
constexpr uint16_t kGreen = 0x2C4A;
constexpr uint16_t kAmber = 0xC423;
constexpr uint16_t kRed = 0xCA69;

const uint8_t *glyphFor(char value) {
  static const uint8_t blank[5] = {0, 0, 0, 0, 0};
  static const uint8_t dash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
  static const uint8_t dot[5] = {0x00, 0x60, 0x60, 0x00, 0x00};
  static const uint8_t underscore[5] = {0x40, 0x40, 0x40, 0x40, 0x40};
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
  if (value == '.') return dot;
  if (value == '_') return underscore;
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

void DisplayDriver::drawLogo(uint16_t x, uint16_t y) {
  setWindow(x, y, DISPLAY_LOGO_WIDTH, DISPLAY_LOGO_HEIGHT);
  SPI.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
  digitalWrite(kSdCs, HIGH);
  digitalWrite(kLcdCs, LOW);
  digitalWrite(kLcdDc, HIGH);
  for (uint16_t index = 0; index < DISPLAY_LOGO_WIDTH * DISPLAY_LOGO_HEIGHT; ++index) {
    const uint8_t bit = 1U << (7 - index % 8);
    const bool navy = (pgm_read_byte(DISPLAY_LOGO_NAVY + index / 8) & bit) != 0;
    const bool teal = (pgm_read_byte(DISPLAY_LOGO_TEAL + index / 8) & bit) != 0;
    const uint16_t pixel = teal ? kTeal : (navy ? kNavy : kWhite);
    SPI.transfer(pixel >> 8);
    SPI.transfer(pixel & 0xFF);
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
  fillRect(0, 0, kWidth, kHeight, kWhite);
  drawLogo(58, 8);
  fillRect(0, 70, kWidth, 2, kTeal);
  drawText(8, 82, "PESO", 2, kMuted, kWhite);
  fillRect(8, 106, 156, 70, kSurface);
  drawText(8, 190, "SENSORI", 2, kMuted, kWhite);
  lastWeightKg_ = UINT32_MAX;
  pageDirty_ = false;
}

void DisplayDriver::drawPageFrame(const char *title) {
  fillRect(0, 0, kWidth, kHeight, kWhite);
  fillRect(0, 0, kWidth, 30, kBlue);
  drawText(8, 7, "CASKLOGIC", 2, kWhite, kBlue);
  drawText(8, 43, title, 2, kNavy, kWhite);
  char pageLabel[8];
  snprintf(pageLabel, sizeof(pageLabel), "%u-%u", page_ + 1, kPageCount);
  drawText(124, 48, pageLabel, 1, kMuted, kWhite);
  fillRect(0, 67, kWidth, 2, kTeal);
}

void DisplayDriver::drawStatusRow(uint16_t y, const char *label, const String &value, uint16_t color) {
  fillRect(8, y - 1, 156, 9, kWhite);
  drawText(8, y, label, 1, kMuted, kWhite);
  String shown = value;
  shown.toUpperCase();
  if (shown.length() > 16) shown = shown.substring(0, 16);
  drawText(70, y, shown.c_str(), 1, color, kWhite);
}

void DisplayDriver::setEnabled(bool enabled) {
  enabled_ = enabled;
  if (enabled_) {
    page_ = 0;
    pageDirty_ = true;
    drawFrame();
    ledcWrite(kLcdBacklight, 307);
  } else {
    ledcWrite(kLcdBacklight, 0);
  }
}

void DisplayDriver::nextPage() {
  if (!enabled_ || resetProgressActive_) return;
  page_ = (page_ + 1) % kPageCount;
  networkInfoUntilMs_ = 0;
  lastRenderMs_ = 0;
  lastWeightKg_ = UINT32_MAX;
  pageDirty_ = true;
}

void DisplayDriver::drawWrappedText(
  int x,
  int y,
  const String &text,
  uint8_t maxRows,
  uint16_t color
) {
  String normalized = text;
  normalized.toUpperCase();
  constexpr size_t kCharactersPerRow = 26;
  for (uint8_t row = 0; row < maxRows; ++row) {
    const size_t start = row * kCharactersPerRow;
    if (start >= normalized.length()) break;
    const String line = normalized.substring(start, start + kCharactersPerRow);
    drawText(x, y + row * 11, line.c_str(), 1, color, kWhite);
  }
}

void DisplayDriver::showNetworkInfo(
  const String &ssid,
  const String &wifiPassword,
  const String &ipAddress,
  const String &adminUser,
  const String &adminPassword
) {
  if (!enabled_) return;
  networkInfoUntilMs_ = millis() + 30000;
  fillRect(0, 0, kWidth, kHeight, kWhite);
  fillRect(0, 0, kWidth, 30, kBlue);
  drawText(8, 7, "CASKLOGIC", 2, kWhite, kBlue);
  drawText(8, 42, "RETE WIFI", 2, kNavy, kWhite);
  drawWrappedText(8, 66, ssid, 2, kNavy);
  drawText(8, 96, "PASSWORD WIFI", 1, kMuted, kWhite);
  drawWrappedText(8, 110, wifiPassword.isEmpty() ? "NON VISUALIZZATA" : wifiPassword, 1, kGreen);
  drawText(8, 136, "INDIRIZZO", 1, kMuted, kWhite);
  drawWrappedText(8, 150, ipAddress, 1, kNavy);
  drawText(8, 178, "ACCESSO WEB", 2, kNavy, kWhite);
  drawText(8, 204, "UTENTE", 1, kMuted, kWhite);
  drawWrappedText(64, 204, adminUser, 1, kGreen);
  drawText(8, 226, "PASSWORD", 1, kMuted, kWhite);
  drawWrappedText(64, 226, adminPassword, 1, kGreen);
  drawText(8, 260, "APRI NEL BROWSER", 1, kMuted, kWhite);
  drawWrappedText(8, 276, String("HTTP ") + ipAddress, 2, kNavy);
}

void DisplayDriver::showFactoryReset() {
  resetProgressActive_ = true;
  networkInfoUntilMs_ = millis() + 3600000UL;
  ledcWrite(kLcdBacklight, 307);
  fillRect(0, 0, kWidth, kHeight, kWhite);
  fillRect(0, 0, kWidth, 30, kRed);
  drawText(8, 7, "CASKLOGIC", 2, kWhite, kRed);
  drawText(8, 84, "RIPRISTINO", 2, kNavy, kWhite);
  drawText(8, 112, "CONFIGURAZIONE", 2, kNavy, kWhite);
  drawText(8, 166, "RILASCIA BOOT", 2, kAmber, kWhite);
  drawText(8, 194, "PER RIAVVIARE", 2, kMuted, kWhite);
}

void DisplayDriver::showFactoryResetProgress(uint32_t elapsedMs, uint32_t totalMs) {
  if (totalMs == 0) return;
  const uint32_t now = millis();
  if (resetProgressActive_ && now - lastResetProgressMs_ < 100) return;
  lastResetProgressMs_ = now;
  if (!resetProgressActive_) {
    resetProgressActive_ = true;
    networkInfoUntilMs_ = millis() + 3600000UL;
    ledcWrite(kLcdBacklight, 307);
    fillRect(0, 0, kWidth, kHeight, kWhite);
    fillRect(0, 0, kWidth, 30, kAmber);
    drawText(8, 7, "CASKLOGIC", 2, kNavy, kAmber);
    drawText(8, 70, "RIPRISTINO", 2, kNavy, kWhite);
    drawText(8, 98, "TIENI PREMUTO", 2, kMuted, kWhite);
    drawText(8, 226, "RILASCIA PER", 1, kMuted, kWhite);
    drawText(8, 240, "ANNULLARE", 1, kMuted, kWhite);
  }
  elapsedMs = std::min(elapsedMs, totalMs);
  const uint16_t progressWidth = static_cast<uint16_t>((elapsedMs * 152ULL) / totalMs);
  fillRect(8, 176, 156, 22, kMuted);
  fillRect(10, 178, 152, 18, kSurface);
  if (progressWidth > 0) fillRect(10, 178, progressWidth, 18, kAmber);
  const uint32_t remainingSeconds = (totalMs - elapsedMs + 999) / 1000;
  fillRect(8, 130, 156, 28, kWhite);
  char countdown[18];
  snprintf(countdown, sizeof(countdown), "MANCANO %LU S", static_cast<unsigned long>(remainingSeconds));
  drawText(8, 136, countdown, 2, kNavy, kWhite);
}

void DisplayDriver::cancelFactoryResetProgress() {
  if (!resetProgressActive_) return;
  resetProgressActive_ = false;
  lastResetProgressMs_ = 0;
  networkInfoUntilMs_ = 0;
  lastRenderMs_ = 0;
  lastWeightKg_ = UINT32_MAX;
  pageDirty_ = true;
  if (enabled_) {
    fillRect(0, 0, kWidth, kHeight, kWhite);
    ledcWrite(kLcdBacklight, 307);
  } else {
    ledcWrite(kLcdBacklight, 0);
  }
}

void DisplayDriver::render(
  const laveggio::SensorReading readings[laveggio::kChannelCount],
  const laveggio::WeightSnapshot &snapshot,
  const DisplayStatus &status
) {
  if (!enabled_ || resetProgressActive_ || static_cast<int32_t>(networkInfoUntilMs_ - millis()) > 0) return;
  if (networkInfoUntilMs_ != 0) {
    networkInfoUntilMs_ = 0;
    pageDirty_ = true;
    lastRenderMs_ = 0;
  }
  const uint32_t refreshInterval = page_ == 0 ? 120 : 1000;
  if (millis() - lastRenderMs_ < refreshInterval) return;
  lastRenderMs_ = millis();

  switch (page_) {
    case 0: drawWeightPage(readings, snapshot, status); break;
    case 1: drawSensorsPage(readings); break;
    case 2: drawNetworkPage(status); break;
    case 3: drawSystemPage(status); break;
    default: drawServicesPage(status); break;
  }
  pageDirty_ = false;
}

void DisplayDriver::drawWeightPage(
  const laveggio::SensorReading readings[laveggio::kChannelCount],
  const laveggio::WeightSnapshot &snapshot,
  const DisplayStatus &status
) {
  if (pageDirty_ || lastWeightKg_ == UINT32_MAX) drawFrame();

  if (snapshot.weightKg != lastWeightKg_ || snapshot.valid != lastValid_) {
    fillRect(8, 106, 156, 70, kSurface);
    char weight[9];
    if (snapshot.valid) snprintf(weight, sizeof(weight), "%lu", snapshot.weightKg);
    else snprintf(weight, sizeof(weight), "-----");
    drawText(16, 121, weight, 4, snapshot.stable ? kGreen : kNavy, kSurface);
    drawText(128, 153, "KG", 2, kMuted, kSurface);
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
    const uint16_t y = 214 + channel * 21;
    drawText(8, y, row, 2, readings[channel].healthy() ? kGreen : kAmber, kWhite);
  }

  fillRect(0, 308, kWidth, 12, status.wifiConnected && status.sdReady ? kGreen : kRed);
}

void DisplayDriver::drawSensorsPage(const laveggio::SensorReading readings[laveggio::kChannelCount]) {
  if (pageDirty_) drawPageFrame("SENSORI");
  for (uint8_t channel = 0; channel < laveggio::kChannelCount; ++channel) {
    const uint16_t y = 80 + channel * 54;
    fillRect(8, y, 156, 14, kWhite);
    fillRect(8, y + 22, 156, 19, kWhite);
    char title[22];
    snprintf(title, sizeof(title), "S%u RAW %04U", channel + 1, readings[channel].raw);
    drawText(8, y, title, 2, readings[channel].healthy() ? kGreen : kAmber, kWhite);
    char detail[28];
    const char *magnet = !readings[channel].present ? "ASSENTE" :
      readings[channel].magnetWeak() ? "MAGNETE DEBOLE" :
      readings[channel].magnetStrong() ? "MAGNETE FORTE" :
      readings[channel].magnetDetected() ? "MAGNETE OK" : "NO MAGNETE";
    snprintf(detail, sizeof(detail), "%s", magnet);
    drawText(8, y + 22, detail, 1, readings[channel].healthy() ? kMuted : kAmber, kWhite);
    snprintf(detail, sizeof(detail), "MAG %u AGC %u", readings[channel].magnitude, readings[channel].agc);
    drawText(8, y + 34, detail, 1, kMuted, kWhite);
  }
}

void DisplayDriver::drawNetworkPage(const DisplayStatus &status) {
  if (pageDirty_) {
    drawPageFrame("RETE");
    drawText(8, 112, "SSID", 1, kMuted, kWhite);
    drawText(8, 280, "BOOT BREVE PAGINA", 1, kMuted, kWhite);
  }
  drawStatusRow(82, "STATO", status.wifiConnected ? "CONNESSA" : (status.accessPointActive ? "ACCESS POINT" : "OFFLINE"), status.wifiConnected ? kGreen : kAmber);
  fillRect(8, 127, 156, 23, kWhite);
  drawWrappedText(8, 128, status.ssid, 2, kNavy);
  drawStatusRow(170, "IP", status.ipAddress, kNavy);
  drawStatusRow(194, "RSSI", status.wifiConnected ? String(status.rssi) + " DBM" : "--", status.rssi >= -70 ? kGreen : kAmber);
  drawStatusRow(218, "ORA", status.timeSynchronized ? "SINCRONIZZATA" : "IN ATTESA", status.timeSynchronized ? kGreen : kAmber);
}

void DisplayDriver::drawSystemPage(const DisplayStatus &status) {
  if (pageDirty_) drawPageFrame("SISTEMA");
  drawStatusRow(82, "FIRMWARE", status.firmwareVersion, kNavy);
  drawStatusRow(108, "UPTIME", String(status.uptimeSeconds / 3600) + " H", kNavy);
  drawStatusRow(134, "MEMORIA", String(status.freeHeap / 1024) + " KB", status.freeHeap > 100000 ? kGreen : kAmber);
  drawStatusRow(160, "TEMP", String(status.chipTemperatureC, 1) + " C", status.chipTemperatureC < 70 ? kGreen : kAmber);
  drawStatusRow(186, "SD", status.sdReady ? "PRONTA" : "ERRORE", status.sdReady ? kGreen : kRed);
  if (status.sdReady && status.sdTotalBytes > 0) {
    const uint32_t usedMb = static_cast<uint32_t>(status.sdUsedBytes / (1024ULL * 1024ULL));
    const uint32_t totalMb = static_cast<uint32_t>(status.sdTotalBytes / (1024ULL * 1024ULL));
    drawStatusRow(212, "SPAZIO", String(usedMb) + "-" + String(totalMb) + " MB", kNavy);
  } else {
    drawStatusRow(212, "SPAZIO", "--", kMuted);
  }
  drawStatusRow(238, "ALIMENT", status.externalPower ? "RETE" : "BATTERIA", status.externalPower ? kGreen : kAmber);
  drawStatusRow(264, "BATTERIA", status.batteryConfigured ? String(status.batteryPercent) + " PCT " + String(status.batteryVoltageMv) + " MV" : "NON CONFIG", status.batteryConfigured ? kNavy : kMuted);
}

void DisplayDriver::drawServicesPage(const DisplayStatus &status) {
  if (pageDirty_) {
    drawPageFrame("GESTIONALE");
    drawText(8, 280, "BOOT BREVE PAGINA", 1, kMuted, kWhite);
  }
  drawStatusRow(82, "API", !status.integrationConfigured ? "NON CONFIG" : (status.integrationOnline ? "ONLINE" : "OFFLINE"), status.integrationOnline ? kGreen : kAmber);
  drawStatusRow(112, "MQTT", !status.mqttEnabled ? "DISATTIVO" : (status.mqttConnected ? "CONNESSO" : "OFFLINE"), status.mqttConnected ? kGreen : kAmber);
  drawStatusRow(142, "HEARTBEAT", String(status.heartbeatFailures) + " ERRORI", status.heartbeatFailures == 0 ? kGreen : kAmber);
  drawStatusRow(172, "SEQUENZA", String(status.sequence), kNavy);
  drawStatusRow(202, "WIFI", status.wifiConnected ? "ONLINE" : "OFFLINE", status.wifiConnected ? kGreen : kRed);
  drawStatusRow(232, "MICROSD", status.sdReady ? "ONLINE" : "OFFLINE", status.sdReady ? kGreen : kRed);
}
