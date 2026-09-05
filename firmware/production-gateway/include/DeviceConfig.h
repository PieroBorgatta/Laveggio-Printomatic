#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "ScaleCore.h"
#include "ReliabilityCore.h"

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
  String eventHmacSecret;
  String metricsToken;
  String tlsCaCertificate;
  String tlsClientCertificate;
  String tlsClientPrivateKey;
  String notificationUrl;
  bool configSyncEnabled = false;
  String configSyncUrl;
  uint32_t configSyncSeconds = 900;
  uint32_t remoteConfigVersion = 0;
  bool mqttEnabled = false;
  String mqttHost;
  uint16_t mqttPort = 8883;
  String mqttUsername;
  String mqttPassword;
  String mqttBaseTopic = "casklogic/laveggio";
  bool mqttCommandsEnabled = false;
  String ntpServer = "pool.ntp.org";
  String timezone = "CET-1CEST,M3.5.0,M10.5.0/3";
  String adminUser = "admin";
  String adminPassword;
  bool displayDefaultOn = true;
  bool speakerDefaultOn = true;
  uint8_t speakerVolumePercent = 100;
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
  bool batterySenseEnabled = true;
  uint16_t batteryDividerMilli = 3000;
  uint16_t batteryMinMv = 3200;
  uint16_t batteryMaxMv = 4200;
  uint16_t batteryCapacityMah = 1000;
  uint8_t sensorOrder[4] = {0,1,2,3};
  uint32_t calibrationRevision = 0;
  bool calibrationChecksumValid = true;
  uint8_t displayBrightness = 65;
  uint16_t displayDimSeconds = 120;
  uint8_t batteryLowPercent = 15;
  bool shutdownButtonEnabled = true;
  laveggio::ClosureConfig closure;
  laveggio::ChannelCalibration calibrations[laveggio::kChannelCount];
};

class ConfigStore {
 public:
  bool begin(const String &deviceSuffix);
  const DeviceConfig &get() const { return config_; }
  DeviceConfig &mutableConfig() { return config_; }
  bool saveSettings();
  bool saveReliabilitySettings();
  bool saveDisplayDefaultOn();
  bool saveSpeakerDefaultOn();
  bool saveSpeakerVolume();
  bool saveHeartbeatRestartSuppressed();
  bool saveRemoteConfigVersion();
  bool saveCalibration(uint8_t channel);
  bool clearCalibration(uint8_t channel);
  bool factoryReset();
  bool isProvisioned() const { return !config_.wifiSsid.isEmpty(); }

 private:
  Preferences preferences_;
  DeviceConfig config_;
  void loadCalibration();
};
