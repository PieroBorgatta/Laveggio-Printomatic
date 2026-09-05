#include "DeviceConfig.h"

namespace {

constexpr char kDefaultAdminUser[] = "admin";
constexpr char kDefaultAdminPassword[] = "casklogic";
constexpr char kLegacyDefaultAdminUser[] = "info@casklogic.com";
constexpr char kLegacyDefaultAdminPassword[] = "Presario41740+";
constexpr uint8_t kAdminCredentialRevision = 2;

String calibrationKey(uint8_t channel, uint8_t position) {
  return String("c") + channel + "p" + position;
}

String enabledKey(uint8_t channel, uint8_t position) {
  return String("c") + channel + "e" + position;
}

struct ReliabilityEnvelope {
  uint32_t schema;
  uint8_t brightness;
  uint16_t dimSeconds;
  uint8_t batteryLow;
  bool shutdownButton;
  laveggio::ClosureConfig closure;
};

struct CalibrationEnvelope {
  uint32_t schema;
  uint32_t revision;
  uint8_t order[4];
  laveggio::ChannelCalibration channels[4];
  uint32_t checksum;
};
uint32_t calibrationChecksum(const CalibrationEnvelope &value) {
  uint32_t crc=0xFFFFFFFF;
  const auto *bytes=reinterpret_cast<const uint8_t*>(&value);
  for(size_t i=0;i<offsetof(CalibrationEnvelope,checksum);++i) {
    crc^=bytes[i]; for(unsigned bit=0;bit<8;++bit) crc=(crc>>1)^(0xEDB88320U & (0U-(crc&1U)));
  }
  return ~crc;
}

}  // namespace

bool ConfigStore::begin(const String &deviceSuffix) {
  // Namespace storico mantenuto per non perdere la configurazione dei dispositivi esistenti.
  if (!preferences_.begin("laveggio", false)) return false;

  const String legacyDeviceId = "laveggio-printomatic-" + deviceSuffix;
  const String legacyHostname = "laveggio-" + deviceSuffix;
  config_.deviceId = preferences_.getString("device_id", "pesalink-" + deviceSuffix);
  config_.hostname = preferences_.getString("hostname", "pesalink-" + deviceSuffix);
  config_.wifiSsid = preferences_.getString("wifi_ssid", "");
  config_.wifiPassword = preferences_.getString("wifi_pass", "");
  config_.useDhcp = preferences_.getBool("dhcp", true);
  config_.staticIp = preferences_.getString("static_ip", "192.168.1.90");
  config_.gateway = preferences_.getString("gateway", "192.168.1.1");
  config_.subnet = preferences_.getString("subnet", "255.255.255.0");
  config_.dns = preferences_.getString("dns", "192.168.1.1");
  config_.backendUrl = preferences_.getString("backend_url", "");
  config_.backendToken = preferences_.getString("backend_tok", "");
  config_.eventHmacSecret = preferences_.getString("event_hmac", "");
  config_.metricsToken = preferences_.getString("metrics_tok", "");
  config_.tlsCaCertificate = preferences_.getString("tls_ca", "");
  config_.tlsClientCertificate = preferences_.getString("tls_cli_crt", "");
  config_.tlsClientPrivateKey = preferences_.getString("tls_cli_key", "");
  config_.notificationUrl = preferences_.getString("notify_url", "");
  config_.configSyncEnabled = preferences_.getBool("cfg_sync", false);
  config_.configSyncUrl = preferences_.getString("cfg_sync_url", "");
  config_.configSyncSeconds = preferences_.getUInt("cfg_sync_s", 900);
  config_.remoteConfigVersion = preferences_.getUInt("cfg_version", 0);
  config_.mqttEnabled = preferences_.getBool("mqtt_enabled", false);
  config_.mqttHost = preferences_.getString("mqtt_host", "");
  config_.mqttPort = preferences_.getUShort("mqtt_port", 8883);
  config_.mqttUsername = preferences_.getString("mqtt_user", "");
  config_.mqttPassword = preferences_.getString("mqtt_pass", "");
  config_.mqttBaseTopic = preferences_.getString("mqtt_topic", "casklogic/pesalink");
  config_.mqttCommandsEnabled = preferences_.getBool("mqtt_cmd", false);
  config_.ntpServer = preferences_.getString("ntp", "pool.ntp.org");
  config_.timezone = preferences_.getString("timezone", "CET-1CEST,M3.5.0,M10.5.0/3");
  config_.adminUser = preferences_.getString("admin_user", kDefaultAdminUser);
  config_.adminPassword = preferences_.getString("admin_pass", kDefaultAdminPassword);
  const uint8_t credentialRevision = preferences_.getUChar("auth_rev", 0);
  if (credentialRevision < kAdminCredentialRevision ||
      (config_.adminUser == kLegacyDefaultAdminUser &&
       config_.adminPassword == kLegacyDefaultAdminPassword)) {
    config_.adminUser = kDefaultAdminUser;
    config_.adminPassword = kDefaultAdminPassword;
    preferences_.putString("admin_user", config_.adminUser);
    preferences_.putString("admin_pass", config_.adminPassword);
    preferences_.putUChar("auth_rev", kAdminCredentialRevision);
  }
  config_.displayDefaultOn = preferences_.getBool("display_on", true);
  config_.speakerDefaultOn = preferences_.getBool("speaker_on", true);
  config_.speakerVolumePercent = min<uint8_t>(preferences_.getUChar("speaker_vol", 100), 100);
  config_.powerSenseEnabled = preferences_.getBool("pwr_sense", false);
  config_.powerSenseActiveHigh = preferences_.getBool("pwr_high", true);
  config_.stableWindowMs = preferences_.getUInt("stable_ms", 600);
  config_.heartbeatSeconds = preferences_.getUInt("heartbeat_s", 30);
  config_.heartbeatWatchdogEnabled = preferences_.getBool("hb_watchdog", false);
  config_.heartbeatFailureThreshold = preferences_.getUChar("hb_fail_max", 5);
  config_.heartbeatRestartSuppressed = preferences_.getBool("hb_suppress", false);
  config_.historyEnabled = preferences_.getBool("hist_enabled", true);
  config_.historyKeepForever = preferences_.getBool("hist_forever", true);
  config_.historyRetentionDays = preferences_.getUShort("hist_days", 730);
  config_.historyFileMaxMb = preferences_.getUShort("hist_max_mb", 32);
  config_.systemLogFileMaxMb = preferences_.getUShort("log_max_mb", 8);
  config_.batterySenseEnabled = preferences_.getBool("bat_sense", true);
  config_.batteryDividerMilli = preferences_.getUShort("bat_div", 3000);
  config_.batteryMinMv = preferences_.getUShort("bat_min_mv", 3200);
  config_.batteryMaxMv = preferences_.getUShort("bat_max_mv", 4200);
  config_.batteryCapacityMah = preferences_.getUShort("bat_cap_mah", 1000);

  // Aggiorna solo i vecchi valori automatici; le personalizzazioni dell'operatore restano intatte.
  if (config_.deviceId == legacyDeviceId) {
    config_.deviceId = "pesalink-" + deviceSuffix;
    preferences_.putString("device_id", config_.deviceId);
  }
  if (config_.hostname == legacyHostname) {
    config_.hostname = "pesalink-" + deviceSuffix;
    preferences_.putString("hostname", config_.hostname);
  }
  if (config_.mqttBaseTopic == "casklogic/laveggio") {
    config_.mqttBaseTopic = "casklogic/pesalink";
    preferences_.putString("mqtt_topic", config_.mqttBaseTopic);
  }

  const uint32_t defaultMultipliers[laveggio::kChannelCount] = {10000, 1000, 100, 10};
  for (uint8_t channel = 0; channel < laveggio::kChannelCount; ++channel) {
    config_.calibrations[channel].multiplierKg = defaultMultipliers[channel];
  }
  preferences_.getBytes("sensor_order", config_.sensorOrder, sizeof(config_.sensorOrder));
  if (!laveggio::validSensorOrder(config_.sensorOrder)) for (uint8_t i=0;i<4;++i) config_.sensorOrder[i]=i;
  config_.calibrationRevision=preferences_.getUInt("cal_rev",0);
  config_.displayBrightness=preferences_.getUChar("brightness",65);
  config_.displayDimSeconds=preferences_.getUShort("dim_seconds",120);
  config_.batteryLowPercent=preferences_.getUChar("battery_low",15);
  config_.shutdownButtonEnabled=preferences_.getBool("power_button",true);
  config_.closure.enabled=preferences_.getBool("closure_on",false);
  config_.closure.completeWeight=preferences_.getBool("closure_weight",false);
  config_.closure.thresholdG=preferences_.getFloat("closure_g",0.35f);
  config_.closure.quietG=preferences_.getFloat("closure_quiet",0.08f);
  config_.closure.quietMs=preferences_.getUInt("closure_ms",400);
  config_.closure.timeoutMs=preferences_.getUInt("closure_timeout",3000);
  config_.closure.cooldownMs=preferences_.getUInt("closure_cool",2000);
  if (preferences_.isKey("reliability_v1")) {
    ReliabilityEnvelope blob{};
    if (preferences_.getBytes("reliability_v1", &blob, sizeof(blob)) == sizeof(blob) && blob.schema == 1) {
      config_.displayBrightness = blob.brightness;
      config_.displayDimSeconds = blob.dimSeconds;
      config_.batteryLowPercent = blob.batteryLow;
      config_.shutdownButtonEnabled = blob.shutdownButton;
      config_.closure = blob.closure;
    } else {
      config_.closure.enabled = false;
      config_.closure.completeWeight = false;
    }
  }
  loadCalibration();
  if(preferences_.isKey("calibration_v1")) {
    CalibrationEnvelope blob{};
    if(preferences_.getBytes("calibration_v1",&blob,sizeof(blob))==sizeof(blob) && blob.schema==1 && blob.checksum==calibrationChecksum(blob) && laveggio::validSensorOrder(blob.order)) {
      memcpy(config_.calibrations,blob.channels,sizeof(blob.channels)); memcpy(config_.sensorOrder,blob.order,4); config_.calibrationRevision=blob.revision;
    } else {
      config_.calibrationChecksumValid=false;
      for(auto &channel:config_.calibrations) for(auto &point:channel.points) point.enabled=false;
    }
  }
  return true;
}

void ConfigStore::loadCalibration() {
  for (uint8_t channel = 0; channel < laveggio::kChannelCount; ++channel) {
    laveggio::ChannelCalibration &calibration = config_.calibrations[channel];
    const String prefix = String("c") + channel;
    calibration.multiplierKg = preferences_.getUInt(
      (prefix + "mul").c_str(),
      calibration.multiplierKg
    );
    calibration.tolerance = preferences_.getUShort((prefix + "tol").c_str(), 90);
    calibration.hysteresis = preferences_.getUShort((prefix + "hys").c_str(), 28);
    for (uint8_t position = 0; position < laveggio::kPositionCount; ++position) {
      calibration.points[position].enabled = preferences_.getBool(
        enabledKey(channel, position).c_str(),
        false
      );
      calibration.points[position].noise=preferences_.getUShort((String("n")+channel+"p"+position).c_str(),0);
      calibration.points[position].magnitude=preferences_.getUShort((String("m")+channel+"p"+position).c_str(),0);
      calibration.points[position].agc=preferences_.getUChar((String("a")+channel+"p"+position).c_str(),0);
      calibration.points[position].raw = preferences_.getUShort(
        calibrationKey(channel, position).c_str(),
        0
      );
    }
  }
}

bool ConfigStore::saveSettings() {
  bool ok = true;
  ok &= preferences_.putString("device_id", config_.deviceId) > 0;
  ok &= preferences_.putString("hostname", config_.hostname) > 0;
  preferences_.putString("wifi_ssid", config_.wifiSsid);
  preferences_.putString("wifi_pass", config_.wifiPassword);
  preferences_.putBool("dhcp", config_.useDhcp);
  preferences_.putString("static_ip", config_.staticIp);
  preferences_.putString("gateway", config_.gateway);
  preferences_.putString("subnet", config_.subnet);
  preferences_.putString("dns", config_.dns);
  preferences_.putString("backend_url", config_.backendUrl);
  preferences_.putString("backend_tok", config_.backendToken);
  preferences_.putString("event_hmac", config_.eventHmacSecret);
  preferences_.putString("metrics_tok", config_.metricsToken);
  preferences_.putString("tls_ca", config_.tlsCaCertificate);
  preferences_.putString("tls_cli_crt", config_.tlsClientCertificate);
  preferences_.putString("tls_cli_key", config_.tlsClientPrivateKey);
  preferences_.putString("notify_url", config_.notificationUrl);
  preferences_.putBool("cfg_sync", config_.configSyncEnabled);
  preferences_.putString("cfg_sync_url", config_.configSyncUrl);
  preferences_.putUInt("cfg_sync_s", config_.configSyncSeconds);
  preferences_.putUInt("cfg_version", config_.remoteConfigVersion);
  preferences_.putBool("mqtt_enabled", config_.mqttEnabled);
  preferences_.putString("mqtt_host", config_.mqttHost);
  preferences_.putUShort("mqtt_port", config_.mqttPort);
  preferences_.putString("mqtt_user", config_.mqttUsername);
  preferences_.putString("mqtt_pass", config_.mqttPassword);
  preferences_.putString("mqtt_topic", config_.mqttBaseTopic);
  preferences_.putBool("mqtt_cmd", config_.mqttCommandsEnabled);
  preferences_.putString("ntp", config_.ntpServer);
  preferences_.putString("timezone", config_.timezone);
  preferences_.putString("admin_user", config_.adminUser);
  preferences_.putString("admin_pass", config_.adminPassword);
  preferences_.putBool("display_on", config_.displayDefaultOn);
  preferences_.putBool("speaker_on", config_.speakerDefaultOn);
  preferences_.putUChar("speaker_vol", config_.speakerVolumePercent);
  preferences_.putBool("pwr_sense", config_.powerSenseEnabled);
  preferences_.putBool("pwr_high", config_.powerSenseActiveHigh);
  preferences_.putUInt("stable_ms", config_.stableWindowMs);
  preferences_.putUInt("heartbeat_s", config_.heartbeatSeconds);
  preferences_.putBool("hb_watchdog", config_.heartbeatWatchdogEnabled);
  preferences_.putUChar("hb_fail_max", config_.heartbeatFailureThreshold);
  preferences_.putBool("hb_suppress", config_.heartbeatRestartSuppressed);
  preferences_.putBool("hist_enabled", config_.historyEnabled);
  preferences_.putBool("hist_forever", config_.historyKeepForever);
  preferences_.putUShort("hist_days", config_.historyRetentionDays);
  preferences_.putUShort("hist_max_mb", config_.historyFileMaxMb);
  preferences_.putUShort("log_max_mb", config_.systemLogFileMaxMb);
  preferences_.putBool("bat_sense", config_.batterySenseEnabled);
  preferences_.putUShort("bat_div", config_.batteryDividerMilli);
  preferences_.putUShort("bat_min_mv", config_.batteryMinMv);
  preferences_.putUShort("bat_max_mv", config_.batteryMaxMv);
  preferences_.putUShort("bat_cap_mah", config_.batteryCapacityMah);
  preferences_.putBytes("sensor_order",config_.sensorOrder,sizeof(config_.sensorOrder));
  preferences_.putUChar("brightness",config_.displayBrightness);
  preferences_.putUShort("dim_seconds",config_.displayDimSeconds);
  preferences_.putUChar("battery_low",config_.batteryLowPercent);
  preferences_.putBool("power_button",config_.shutdownButtonEnabled);
  preferences_.putBool("closure_on",config_.closure.enabled);
  preferences_.putBool("closure_weight",config_.closure.completeWeight);
  preferences_.putFloat("closure_g",config_.closure.thresholdG);
  preferences_.putFloat("closure_quiet",config_.closure.quietG);
  preferences_.putUInt("closure_ms",config_.closure.quietMs);
  preferences_.putUInt("closure_timeout",config_.closure.timeoutMs);
  preferences_.putUInt("closure_cool",config_.closure.cooldownMs);
  return ok;
}

bool ConfigStore::saveReliabilitySettings() {
  ReliabilityEnvelope blob{};
  blob.schema = 1;
  blob.brightness = config_.displayBrightness;
  blob.dimSeconds = config_.displayDimSeconds;
  blob.batteryLow = config_.batteryLowPercent;
  blob.shutdownButton = config_.shutdownButtonEnabled;
  blob.closure = config_.closure;
  return preferences_.putBytes("reliability_v1", &blob, sizeof(blob)) == sizeof(blob);
}

bool ConfigStore::saveDisplayDefaultOn() {
  return preferences_.putBool("display_on", config_.displayDefaultOn) > 0;
}

bool ConfigStore::saveSpeakerDefaultOn() {
  return preferences_.putBool("speaker_on", config_.speakerDefaultOn) > 0;
}

bool ConfigStore::saveSpeakerVolume() {
  preferences_.putUChar("speaker_vol", config_.speakerVolumePercent);
  return true;
}

bool ConfigStore::saveHeartbeatRestartSuppressed() {
  preferences_.putBool("hb_suppress", config_.heartbeatRestartSuppressed);
  return true;
}

bool ConfigStore::saveRemoteConfigVersion() {
  preferences_.putUInt("cfg_version", config_.remoteConfigVersion);
  return true;
}

bool ConfigStore::saveCalibration(uint8_t channel) {
  if(channel>=4) return false;
  CalibrationEnvelope blob{}; blob.schema=1; blob.revision=config_.calibrationRevision+1;
  memcpy(blob.channels,config_.calibrations,sizeof(blob.channels)); memcpy(blob.order,config_.sensorOrder,4);
  blob.checksum=calibrationChecksum(blob);
  if(preferences_.putBytes("calibration_v1",&blob,sizeof(blob))!=sizeof(blob)) return false;
  config_.calibrationRevision=blob.revision; config_.calibrationChecksumValid=true;
  return true;
}

bool ConfigStore::clearCalibration(uint8_t channel) {
  if (channel >= laveggio::kChannelCount) return false;
  const auto previous = config_.calibrations[channel];
  for (uint8_t position = 0; position < laveggio::kPositionCount; ++position) {
    config_.calibrations[channel].points[position] = {};
  }
  if (saveCalibration(channel)) return true;
  config_.calibrations[channel] = previous;
  return false;
}

bool ConfigStore::factoryReset() {
  return preferences_.clear();
}
