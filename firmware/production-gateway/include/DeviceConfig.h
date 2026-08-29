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
  String notificationUrl;
  String ntpServer = "pool.ntp.org";
  String timezone = "CET-1CEST,M3.5.0,M10.5.0/3";
  String adminUser = "admin";
  String adminPassword;
  bool displayDefaultOn = false;
  bool powerSenseEnabled = false;
  bool powerSenseActiveHigh = true;
  uint32_t stableWindowMs = 600;
  uint32_t heartbeatSeconds = 30;
  laveggio::ChannelCalibration calibrations[laveggio::kChannelCount];
};

class ConfigStore {
 public:
  bool begin(const String &deviceSuffix);
  const DeviceConfig &get() const { return config_; }
  DeviceConfig &mutableConfig() { return config_; }
  bool saveSettings();
  bool saveCalibration(uint8_t channel);
  bool clearCalibration(uint8_t channel);
  bool isProvisioned() const { return !config_.wifiSsid.isEmpty(); }

 private:
  Preferences preferences_;
  DeviceConfig config_;
  void loadCalibration();
};
