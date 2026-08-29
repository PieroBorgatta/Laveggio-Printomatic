#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "ScaleCore.h"

struct DeviceConfig {
  String deviceId;
  String hostname;
  String wifiSsid;
  String wifiPassword;
  bool useDhcp = true;
  String staticIp = "192.168.1.90";
  String gateway = "192.168.1.1";
  String subnet = "255.255.255.0";
  String dns = "192.168.1.1";
  String backendUrl;
  String backendToken;
  String tlsCaCertificate;
  String tlsClientCertificate;
  String tlsClientPrivateKey;
  String notificationUrl;
  String ntpServer = "pool.ntp.org";
  String timezone = "CET-1CEST,M3.5.0,M10.5.0/3";
  String adminUser = "info@casklogic.com";
  String adminPassword;
  bool displayDefaultOn = false;
  bool powerSenseEnabled = false;
  bool powerSenseActiveHigh = true;
  uint32_t stableWindowMs = 600;
  uint32_t heartbeatSeconds = 30;
  bool heartbeatWatchdogEnabled = false;
  uint8_t heartbeatFailureThreshold = 5;
  bool heartbeatRestartSuppressed = false;
  bool historyEnabled = true;
  bool historyKeepForever = true;
  uint16_t historyRetentionDays = 730;
  uint16_t historyFileMaxMb = 32;
  uint16_t systemLogFileMaxMb = 8;
  bool batterySenseEnabled = false;
  uint16_t batteryDividerMilli = 2000;
  uint16_t batteryMinMv = 3200;
  uint16_t batteryMaxMv = 4200;
  uint16_t batteryCapacityMah = 1200;
  laveggio::ChannelCalibration calibrations[laveggio::kChannelCount];
};

class ConfigStore {
 public:
  bool begin(const String &deviceSuffix);
  const DeviceConfig &get() const { return config_; }
  DeviceConfig &mutableConfig() { return config_; }
  bool saveSettings();
  bool saveHeartbeatRestartSuppressed();
  bool saveCalibration(uint8_t channel);
  bool clearCalibration(uint8_t channel);
  bool isProvisioned() const { return !config_.wifiSsid.isEmpty(); }

 private:
  Preferences preferences_;
  DeviceConfig config_;
  void loadCalibration();
};
