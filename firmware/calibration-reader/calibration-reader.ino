// Laveggio Printomatic diagnostic and calibration reader.
// SPDX-License-Identifier: CC-BY-4.0

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

// Waveshare ESP32-C6-LCD-1.47 official LCD pinout.
static constexpr int LCD_MISO = 5;
static constexpr int LCD_MOSI = 6;
static constexpr int LCD_SCLK = 7;
static constexpr int LCD_CS = 14;
static constexpr int LCD_DC = 15;
static constexpr int LCD_RST = 21;
static constexpr int LCD_BL = 22;

// Free GPIOs selected for the scale sensor test bus.
static constexpr int I2C_SDA = 1;
static constexpr int I2C_SCL = 2;
static constexpr int MUX_RST = 3;

static constexpr uint8_t AS5600_ADDRESS = 0x36;
static constexpr uint32_t I2C_CLOCK_HZ = 100000;
static constexpr unsigned long SENSOR_READ_INTERVAL_MS = 10;
static constexpr unsigned long SENSOR_HEALTH_INTERVAL_MS = 250;
static constexpr uint16_t LCD_WIDTH = 172;
static constexpr uint16_t LCD_HEIGHT = 320;
static constexpr uint16_t LCD_X_OFFSET = 34;

static constexpr uint16_t BLACK = 0x0000;
static constexpr uint16_t WHITE = 0xFFFF;
static constexpr uint16_t RED = 0xF800;
static constexpr uint16_t GREEN = 0x07E0;
static constexpr uint16_t BLUE = 0x001F;
static constexpr uint16_t CYAN = 0x07FF;
static constexpr uint16_t YELLOW = 0xFFE0;
static constexpr uint16_t ORANGE = 0xFD20;
static constexpr uint16_t NAVY = 0x000F;
static constexpr uint16_t DARK_GREY = 0x4208;

struct ChannelState {
  bool present = false;
  uint16_t rawAngle = 0;
  uint8_t status = 0;
  uint8_t agc = 0;
  uint16_t magnitude = 0;
};

ChannelState channels[4];
ChannelState displayedChannels[4];
int muxAddress = -1;
int displayedMuxAddress = -2;
int activeSdaPin = I2C_SDA;
int activeSclPin = I2C_SCL;
unsigned long lastReadMs = 0;
unsigned long lastSerialMs = 0;
unsigned long lastHealthReadMs = 0;
unsigned long scanCounter = 0;
unsigned long lastPrintedScanCounter = 0;
unsigned long lastScanDurationUs = 0;
int stuckChannel = -1;
int stuckSdaLevel = -1;
int stuckSclLevel = -1;
bool stuckReported = false;

void lcdCommand(uint8_t value) {
  SPI.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
  digitalWrite(LCD_CS, LOW);
  digitalWrite(LCD_DC, LOW);
  SPI.transfer(value);
  digitalWrite(LCD_CS, HIGH);
  SPI.endTransaction();
}

void lcdData(uint8_t value) {
  SPI.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
  digitalWrite(LCD_CS, LOW);
  digitalWrite(LCD_DC, HIGH);
  SPI.transfer(value);
  digitalWrite(LCD_CS, HIGH);
  SPI.endTransaction();
}

void lcdSetWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
  const uint16_t x0 = x + LCD_X_OFFSET;
  const uint16_t x1 = x0 + width - 1;
  const uint16_t y0 = y;
  const uint16_t y1 = y0 + height - 1;

  lcdCommand(0x2A);
  lcdData(x0 >> 8);
  lcdData(x0 & 0xFF);
  lcdData(x1 >> 8);
  lcdData(x1 & 0xFF);

  lcdCommand(0x2B);
  lcdData(y0 >> 8);
  lcdData(y0 & 0xFF);
  lcdData(y1 >> 8);
  lcdData(y1 & 0xFF);
  lcdCommand(0x2C);
}

void lcdFillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
  if (x >= LCD_WIDTH || y >= LCD_HEIGHT || width == 0 || height == 0) return;
  if (x + width > LCD_WIDTH) width = LCD_WIDTH - x;
  if (y + height > LCD_HEIGHT) height = LCD_HEIGHT - y;

  lcdSetWindow(x, y, width, height);
  SPI.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
  digitalWrite(LCD_CS, LOW);
  digitalWrite(LCD_DC, HIGH);
  const uint8_t hi = color >> 8;
  const uint8_t lo = color & 0xFF;
  for (uint32_t i = 0; i < static_cast<uint32_t>(width) * height; ++i) {
    SPI.transfer(hi);
    SPI.transfer(lo);
  }
  digitalWrite(LCD_CS, HIGH);
  SPI.endTransaction();
}

const uint8_t *glyphFor(char value) {
  static const uint8_t blank[5] = {0, 0, 0, 0, 0};
  static const uint8_t dash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
  static const uint8_t colon[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
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
  if (value == ':') return colon;
  return blank;
}

void lcdDrawChar(int x, int y, char value, uint8_t scale, uint16_t color) {
  const uint8_t *glyph = glyphFor(value);
  for (uint8_t column = 0; column < 5; ++column) {
    for (uint8_t row = 0; row < 7; ++row) {
      if (glyph[column] & (1U << row)) {
        lcdFillRect(x + column * scale, y + row * scale, scale, scale, color);
      }
    }
  }
}

void lcdDrawText(int x, int y, const char *text, uint8_t scale, uint16_t color) {
  while (*text) {
    lcdDrawChar(x, y, *text, scale, color);
    x += 6 * scale;
    ++text;
  }
}

void lcdDrawCharOpaque(
  int x,
  int y,
  char value,
  uint8_t scale,
  uint16_t foreground,
  uint16_t background
) {
  const uint8_t *glyph = glyphFor(value);
  const uint16_t width = 6U * scale;
  const uint16_t height = 7U * scale;
  lcdSetWindow(x, y, width, height);

  SPI.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
  digitalWrite(LCD_CS, LOW);
  digitalWrite(LCD_DC, HIGH);
  for (uint16_t py = 0; py < height; ++py) {
    const uint8_t glyphRow = py / scale;
    for (uint16_t px = 0; px < width; ++px) {
      const uint8_t glyphColumn = px / scale;
      const bool set = glyphColumn < 5 && (glyph[glyphColumn] & (1U << glyphRow));
      const uint16_t color = set ? foreground : background;
      SPI.transfer(color >> 8);
      SPI.transfer(color & 0xFF);
    }
  }
  digitalWrite(LCD_CS, HIGH);
  SPI.endTransaction();
}

void lcdDrawTextOpaque(
  int x,
  int y,
  const char *text,
  uint8_t scale,
  uint16_t foreground,
  uint16_t background
) {
  while (*text) {
    lcdDrawCharOpaque(x, y, *text, scale, foreground, background);
    x += 6 * scale;
    ++text;
  }
}

void lcdInit() {
  pinMode(LCD_CS, OUTPUT);
  pinMode(LCD_DC, OUTPUT);
  pinMode(LCD_RST, OUTPUT);
  digitalWrite(LCD_CS, HIGH);

  SPI.begin(LCD_SCLK, LCD_MISO, LCD_MOSI, LCD_CS);

  digitalWrite(LCD_RST, LOW);
  delay(50);
  digitalWrite(LCD_RST, HIGH);
  delay(120);

  lcdCommand(0x11);
  delay(120);
  lcdCommand(0x36);
  lcdData(0x00);
  lcdCommand(0x3A);
  lcdData(0x05);
  lcdCommand(0xB0);
  lcdData(0x00);
  lcdData(0xE8);
  lcdCommand(0xB2);
  lcdData(0x0C);
  lcdData(0x0C);
  lcdData(0x00);
  lcdData(0x33);
  lcdData(0x33);
  lcdCommand(0xB7);
  lcdData(0x35);
  lcdCommand(0xBB);
  lcdData(0x35);
  lcdCommand(0xC0);
  lcdData(0x2C);
  lcdCommand(0xC2);
  lcdData(0x01);
  lcdCommand(0xC3);
  lcdData(0x13);
  lcdCommand(0xC4);
  lcdData(0x20);
  lcdCommand(0xC6);
  lcdData(0x0F);
  lcdCommand(0xD0);
  lcdData(0xA4);
  lcdData(0xA1);
  lcdCommand(0xD6);
  lcdData(0xA1);
  lcdCommand(0x21);
  lcdCommand(0x29);
  delay(20);

  ledcAttach(LCD_BL, 1000, 10);
  ledcWrite(LCD_BL, 307);  // About 30% brightness.
}

void drawSevenSegmentDigit(uint8_t digit, int x, int y, int thickness, uint16_t color) {
  static const uint8_t masks[10] = {
    0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110,
    0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111
  };
  const int length = 34;
  const uint8_t mask = masks[digit % 10];
  if (mask & 0x01) lcdFillRect(x + thickness, y, length, thickness, color);
  if (mask & 0x02) lcdFillRect(x + length + thickness, y + thickness, thickness, length, color);
  if (mask & 0x04) lcdFillRect(x + length + thickness, y + length + 2 * thickness, thickness, length, color);
  if (mask & 0x08) lcdFillRect(x + thickness, y + 2 * length + 2 * thickness, length, thickness, color);
  if (mask & 0x10) lcdFillRect(x, y + length + 2 * thickness, thickness, length, color);
  if (mask & 0x20) lcdFillRect(x, y + thickness, thickness, length, color);
  if (mask & 0x40) lcdFillRect(x + thickness, y + length + thickness, length, thickness, color);
}

bool i2cProbe(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool muxSelect(uint8_t channel) {
  if (muxAddress < 0 || channel > 3) return false;
  Wire.beginTransmission(static_cast<uint8_t>(muxAddress));
  Wire.write(1U << channel);
  return Wire.endTransmission() == 0;
}

bool as5600Read(uint8_t reg, uint8_t *buffer, size_t length) {
  Wire.beginTransmission(AS5600_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  const size_t received = Wire.requestFrom(AS5600_ADDRESS, length);
  if (received != length) return false;
  for (size_t i = 0; i < length; ++i) buffer[i] = Wire.read();
  return true;
}

void discoverMux() {
  muxAddress = -1;
  for (uint8_t address = 0x70; address <= 0x77; ++address) {
    if (i2cProbe(address)) {
      muxAddress = address;
      break;
    }
  }
}

bool startI2cAndDiscover(int sdaPin, int sclPin, bool alreadyStarted) {
  if (alreadyStarted) {
    Wire.end();
    delay(5);
  }
  activeSdaPin = sdaPin;
  activeSclPin = sclPin;
  Wire.begin(activeSdaPin, activeSclPin, I2C_CLOCK_HZ);
  Wire.setTimeOut(20);
  delay(10);
  discoverMux();
  Serial.printf(
    "I2C test SDA=GPIO%d SCL=GPIO%d -> mux=%s\n",
    activeSdaPin,
    activeSclPin,
    muxAddress >= 0 ? "presente" : "assente"
  );
  return muxAddress >= 0;
}

void autoDetectI2cPins() {
  if (startI2cAndDiscover(I2C_SDA, I2C_SCL, false)) return;
  if (startI2cAndDiscover(I2C_SCL, I2C_SDA, true)) return;

  // Keep the user-declared wiring active if neither order answers.
  startI2cAndDiscover(I2C_SDA, I2C_SCL, true);
}

void readSensors() {
  const unsigned long scanStartedUs = micros();
  const unsigned long now = millis();
  const bool healthDue = lastHealthReadMs == 0 || now - lastHealthReadMs >= SENSOR_HEALTH_INTERVAL_MS;

  if (muxAddress < 0) discoverMux();
  if (muxAddress >= 0) {
    stuckChannel = -1;
    stuckSdaLevel = -1;
    stuckSclLevel = -1;
  }
  for (uint8_t channel = 0; channel < 4; ++channel) {
    const bool selected = muxSelect(channel);
    delayMicroseconds(500);

    const int sdaLevel = digitalRead(activeSdaPin);
    const int sclLevel = digitalRead(activeSclPin);
    if (!selected || sdaLevel == LOW || sclLevel == LOW) {
      stuckChannel = channel;
      stuckSdaLevel = sdaLevel;
      stuckSclLevel = sclLevel;
      if (!stuckReported) {
        Serial.printf(
          "BUS BLOCCATO CH%d SDA=%s SCL=%s\n",
          stuckChannel,
          stuckSdaLevel == HIGH ? "HIGH" : "LOW",
          stuckSclLevel == HIGH ? "HIGH" : "LOW"
        );
        stuckReported = true;
      }
      return;
    }

    uint8_t angle[2] = {0, 0};
    if (!as5600Read(0x0C, angle, 2)) {
      channels[channel].present = false;
      continue;
    }

    channels[channel].present = true;
    channels[channel].rawAngle = ((static_cast<uint16_t>(angle[0]) << 8) | angle[1]) & 0x0FFF;

    if (healthDue) {
      uint8_t status = channels[channel].status;
      uint8_t agc = channels[channel].agc;
      uint8_t magnitude[2] = {
        static_cast<uint8_t>(channels[channel].magnitude >> 8),
        static_cast<uint8_t>(channels[channel].magnitude & 0xFF)
      };
      if (as5600Read(0x0B, &status, 1)) channels[channel].status = status;
      if (as5600Read(0x1A, &agc, 1)) channels[channel].agc = agc;
      if (as5600Read(0x1B, magnitude, 2)) {
        channels[channel].magnitude =
          ((static_cast<uint16_t>(magnitude[0]) << 8) | magnitude[1]) & 0x0FFF;
      }
    }
  }

  if (healthDue) lastHealthReadMs = now;

  if (muxAddress >= 0) {
    Wire.beginTransmission(static_cast<uint8_t>(muxAddress));
    Wire.write(0x00);
    Wire.endTransmission();
  }

  ++scanCounter;
  lastScanDurationUs = micros() - scanStartedUs;
}

bool channelDisplayChanged(uint8_t channel) {
  return channels[channel].present != displayedChannels[channel].present ||
         channels[channel].rawAngle != displayedChannels[channel].rawAngle ||
         (channels[channel].status & 0x38) != (displayedChannels[channel].status & 0x38);
}

void drawMuxStatus(bool force) {
  if (!force && muxAddress == displayedMuxAddress) return;

  char line[18];
  char status[17];
  if (muxAddress >= 0) {
    snprintf(status, sizeof(status), "M%02X D%d C%d OK", muxAddress, activeSdaPin, activeSclPin);
  } else {
    snprintf(status, sizeof(status), "M-- D%d C%d NO", activeSdaPin, activeSclPin);
  }
  snprintf(line, sizeof(line), "%-16s", status);
  lcdDrawTextOpaque(2, 36, line, 2, muxAddress >= 0 ? GREEN : RED, NAVY);
  lcdFillRect(0, 302, LCD_WIDTH, 18, muxAddress >= 0 ? GREEN : RED);
  displayedMuxAddress = muxAddress;
}

void drawChannel(uint8_t channel, bool force) {
  if (!force && !channelDisplayChanged(channel)) return;

  char line[14];
  uint16_t color = RED;
  if (channels[channel].present) {
    const bool magnetDetected = channels[channel].status & 0x20;
    const bool magnetWeak = channels[channel].status & 0x10;
    const bool magnetStrong = channels[channel].status & 0x08;
    color = magnetDetected && !magnetWeak && !magnetStrong ? GREEN : ORANGE;
    const char *magnet = color == GREEN ? "OK" : "MAG";
    snprintf(line, sizeof(line), "S%u %04u %-3s", channel, channels[channel].rawAngle, magnet);
  } else {
    snprintf(line, sizeof(line), "S%u ---- -- ", channel);
  }

  const int y = 78 + channel * 43;
  lcdDrawTextOpaque(10, y, line, 2, color, DARK_GREY);
  displayedChannels[channel] = channels[channel];
}

void drawDashboardFrame() {
  lcdFillRect(0, 0, LCD_WIDTH, LCD_HEIGHT, NAVY);
  lcdFillRect(0, 0, LCD_WIDTH, 24, BLUE);
  lcdDrawText(2, 5, "LAVEGGIO TEST", 2, WHITE);

  for (uint8_t channel = 0; channel < 4; ++channel) {
    const int y = 78 + channel * 43;
    lcdFillRect(4, y - 5, LCD_WIDTH - 8, 35, DARK_GREY);
  }

  drawMuxStatus(true);
  for (uint8_t channel = 0; channel < 4; ++channel) drawChannel(channel, true);
  lcdDrawText(22, 270, "LIVE 100K", 2, CYAN);
}

void updateDashboard() {
  drawMuxStatus(false);
  for (uint8_t channel = 0; channel < 4; ++channel) drawChannel(channel, false);
}

void printDiagnostics() {
  const unsigned long scansPerSecond = scanCounter - lastPrintedScanCounter;
  lastPrintedScanCounter = scanCounter;
  Serial.printf(
    "{\"i2c\":{\"sda\":%d,\"scl\":%d,\"hz\":%lu,\"last_scan_us\":%lu,\"stuck_channel\":%d,\"sda_level\":%d,\"scl_level\":%d},\"mux\":%d,\"channels\":[",
    activeSdaPin,
    activeSclPin,
    scansPerSecond,
    lastScanDurationUs,
    stuckChannel,
    stuckSdaLevel,
    stuckSclLevel,
    muxAddress
  );
  for (uint8_t channel = 0; channel < 4; ++channel) {
    const bool md = channels[channel].status & 0x20;
    const bool ml = channels[channel].status & 0x10;
    const bool mh = channels[channel].status & 0x08;
    Serial.printf(
      "%s{\"ch\":%u,\"present\":%s,\"raw\":%u,\"md\":%s,\"ml\":%s,\"mh\":%s,\"agc\":%u,\"magnitude\":%u}",
      channel == 0 ? "" : ",",
      channel,
      channels[channel].present ? "true" : "false",
      channels[channel].rawAngle,
      md ? "true" : "false",
      ml ? "true" : "false",
      mh ? "true" : "false",
      channels[channel].agc,
      channels[channel].magnitude
    );
  }
  Serial.println("]}");
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("Laveggio Printomatic diagnostic starting");

  lcdInit();

  pinMode(MUX_RST, OUTPUT);
  digitalWrite(MUX_RST, HIGH);
  autoDetectI2cPins();

  readSensors();
  drawDashboardFrame();
  printDiagnostics();
}

void loop() {
  const unsigned long now = millis();
  if (now - lastReadMs >= SENSOR_READ_INTERVAL_MS) {
    lastReadMs = now;
    readSensors();
    updateDashboard();
  }
  if (now - lastSerialMs >= 1000) {
    lastSerialMs = now;
    printDiagnostics();
  }
  delay(1);
}
