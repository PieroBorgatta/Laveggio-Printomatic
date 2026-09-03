#include "BoardHardware.h"

#include <Wire.h>
#include <algorithm>
#include <cmath>
#include <time.h>

namespace {

constexpr uint8_t kBatteryAdcPin = 8;
constexpr uint8_t kQmi8658Address = 0x6B;
constexpr uint8_t kPcf85063Address = 0x51;
constexpr uint8_t kQmiCtrl1 = 0x02;
constexpr uint8_t kQmiCtrl2 = 0x03;
constexpr uint8_t kQmiCtrl3 = 0x04;
constexpr uint8_t kQmiCtrl5 = 0x06;
constexpr uint8_t kQmiCtrl7 = 0x08;
constexpr uint8_t kQmiTemperatureLow = 0x33;
constexpr uint8_t kQmiAccelerationLow = 0x35;
constexpr uint8_t kRtcSeconds = 0x04;
constexpr uint32_t kPollIntervalMs = 1000;
constexpr float kBatteryDivider = 3.0f;
constexpr float kBatteryMeasurementOffset = 0.990476f;

uint8_t decimalToBcd(int value) {
  return static_cast<uint8_t>((value / 10) * 16 + value % 10);
}

int bcdToDecimal(uint8_t value) {
  return (value >> 4) * 10 + (value & 0x0F);
}

int16_t signedWord(const uint8_t *data) {
  return static_cast<int16_t>(static_cast<uint16_t>(data[0]) |
    (static_cast<uint16_t>(data[1]) << 8));
}

}  // namespace

bool BoardHardware::readRegister(
  uint8_t address,
  uint8_t reg,
  uint8_t *data,
  size_t length
) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  const size_t received = Wire.requestFrom(address, length, true);
  if (received != length) {
    while (Wire.available()) Wire.read();
    return false;
  }
  for (size_t index = 0; index < length; ++index) data[index] = Wire.read();
  return true;
}

bool BoardHardware::writeRegister(uint8_t address, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission(true) == 0;
}

void BoardHardware::begin() {
  pinMode(kBatteryAdcPin, INPUT);
  analogSetPinAttenuation(kBatteryAdcPin, ADC_11db);
  initializeImu();
  readRtc();
  readBattery();
}

void BoardHardware::initializeImu() {
  uint8_t revision = 0;
  status_.imuAvailable = readRegister(kQmi8658Address, 0x01, &revision, 1);
  if (!status_.imuAvailable) return;

  // Auto-increment, accelerometro +/-4 g a 125 Hz, giroscopio +/-64 dps.
  writeRegister(kQmi8658Address, kQmiCtrl1, 0x40);
  writeRegister(kQmi8658Address, kQmiCtrl2, 0x16);
  writeRegister(kQmi8658Address, kQmiCtrl3, 0x26);
  writeRegister(kQmi8658Address, kQmiCtrl5, 0x11);
  writeRegister(kQmi8658Address, kQmiCtrl7, 0x43);
}

void BoardHardware::readBattery() {
  uint32_t samples[9];
  for (uint8_t index = 0; index < 9; ++index) {
    samples[index] = analogReadMilliVolts(kBatteryAdcPin);
    delayMicroseconds(180);
  }
  std::sort(std::begin(samples), std::end(samples));
  const uint32_t adcMv = (samples[3] + samples[4] + samples[5]) / 3;
  const uint32_t measured = static_cast<uint32_t>(
    (adcMv * kBatteryDivider) / kBatteryMeasurementOffset
  );
  status_.batteryAvailable = measured >= 2500 && measured <= 5000;
  status_.batteryVoltageMv = status_.batteryAvailable
    ? static_cast<uint16_t>(measured)
    : 0;
}

void BoardHardware::readImu() {
  if (!status_.imuAvailable) {
    initializeImu();
    if (!status_.imuAvailable) return;
  }
  uint8_t raw[8] = {0};
  if (!readRegister(kQmi8658Address, kQmiTemperatureLow, raw, sizeof(raw))) {
    status_.imuAvailable = false;
    return;
  }
  const int16_t temperatureRaw = signedWord(raw);
  const float scale = 4.0f / 32768.0f;
  status_.boardTemperatureC = temperatureRaw / 256.0f;
  status_.accelerationX = signedWord(raw + 2) * scale;
  status_.accelerationY = signedWord(raw + 4) * scale;
  status_.accelerationZ = signedWord(raw + 6) * scale;
}

void BoardHardware::readRtc() {
  uint8_t raw[7] = {0};
  status_.rtcAvailable = readRegister(kPcf85063Address, kRtcSeconds, raw, sizeof(raw));
  if (!status_.rtcAvailable) {
    status_.rtcClockValid = false;
    status_.rtcDateTime[0] = '\0';
    return;
  }
  const int year = 2000 + bcdToDecimal(raw[6]);
  const int month = bcdToDecimal(raw[5] & 0x1F);
  const int day = bcdToDecimal(raw[3] & 0x3F);
  const int hour = bcdToDecimal(raw[2] & 0x3F);
  const int minute = bcdToDecimal(raw[1] & 0x7F);
  const int second = bcdToDecimal(raw[0] & 0x7F);
  status_.rtcClockValid = year >= 2024 && month >= 1 && month <= 12 &&
    day >= 1 && day <= 31 && hour <= 23 && minute <= 59 && second <= 59;
  snprintf(
    status_.rtcDateTime,
    sizeof(status_.rtcDateTime),
    "%02d/%02d/%04d %02d:%02d",
    day,
    month,
    year,
    hour,
    minute
  );
}

void BoardHardware::synchronizeRtc(time_t epoch) {
  if (epoch < 1700000000) return;
  struct tm local{};
  localtime_r(&epoch, &local);
  uint8_t raw[7] = {
    decimalToBcd(local.tm_sec), decimalToBcd(local.tm_min), decimalToBcd(local.tm_hour),
    decimalToBcd(local.tm_mday), decimalToBcd(local.tm_wday),
    decimalToBcd(local.tm_mon + 1), decimalToBcd(local.tm_year + 1900 - 2000)
  };
  Wire.beginTransmission(kPcf85063Address);
  Wire.write(kRtcSeconds);
  Wire.write(raw, sizeof(raw));
  status_.rtcAvailable = Wire.endTransmission(true) == 0;
  readRtc();
}

void BoardHardware::poll(uint32_t nowMs) {
  if (nowMs - lastPollMs_ < kPollIntervalMs) return;
  lastPollMs_ = nowMs;
  readBattery();
  readImu();
  readRtc();
}
