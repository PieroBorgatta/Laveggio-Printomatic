#include "DeviceConfig.h"

namespace {

String calibrationKey(uint8_t channel, uint8_t position) {
  return String("c") + channel + "p" + position;
}

String enabledKey(uint8_t channel, uint8_t position) {
  return String("c") + channel + "e" + position;
}

}  // namespace

bool ConfigStore::begin(const String &deviceSuffix) {
  if (!preferences_.begin("laveggio", false)) return false;

  config_.deviceId = preferences_.getString("device_id", "laveggio-printomatic-" + deviceSuffix);
  config_.hostname = preferences_.getString("hostname", "laveggio-" + deviceSuffix);
  config_.wifiSsid = preferences_.getString("wifi_ssid", "");
  config_.wifiPassword = preferences_.getString("wifi_pass", "");
  config_.useDhcp = preferences_.getBool("dhcp", true);
  config_.staticIp = preferences_.getString("static_ip", "192.168.1.90");
  config_.gateway = preferences_.getString("gateway", "192.168.1.1");
  config_.subnet = preferences_.getString("subnet", "255.255.255.0");
  config_.dns = preferences_.getString("dns", "192.168.1.1");
  config_.backendUrl = preferences_.getString("backend_url", "");
  config_.backendToken = preferences_.getString("backend_tok", "");
  config_.tlsCaCertificate = preferences_.getString("tls_ca", "");
  config_.notificationUrl = preferences_.getString("notify_url", "");
  config_.ntpServer = preferences_.getString("ntp", "pool.ntp.org");
  config_.timezone = preferences_.getString("timezone", "CET-1CEST,M3.5.0,M10.5.0/3");
  config_.adminUser = preferences_.getString("admin_user", "admin");
  config_.adminPassword = preferences_.getString("admin_pass", "Cask-" + deviceSuffix + "!");
  config_.displayDefaultOn = preferences_.getBool("display_on", false);
  config_.powerSenseEnabled = preferences_.getBool("pwr_sense", false);
  config_.powerSenseActiveHigh = preferences_.getBool("pwr_high", true);
  config_.stableWindowMs = preferences_.getUInt("stable_ms", 600);
  config_.heartbeatSeconds = preferences_.getUInt("heartbeat_s", 30);

  const uint32_t defaultMultipliers[laveggio::kChannelCount] = {10000, 1000, 100, 10};
  for (uint8_t channel = 0; channel < laveggio::kChannelCount; ++channel) {
    config_.calibrations[channel].multiplierKg = defaultMultipliers[channel];
  }
  loadCalibration();
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
  preferences_.putString("tls_ca", config_.tlsCaCertificate);
  preferences_.putString("notify_url", config_.notificationUrl);
  preferences_.putString("ntp", config_.ntpServer);
  preferences_.putString("timezone", config_.timezone);
  preferences_.putString("admin_user", config_.adminUser);
  preferences_.putString("admin_pass", config_.adminPassword);
  preferences_.putBool("display_on", config_.displayDefaultOn);
  preferences_.putBool("pwr_sense", config_.powerSenseEnabled);
  preferences_.putBool("pwr_high", config_.powerSenseActiveHigh);
  preferences_.putUInt("stable_ms", config_.stableWindowMs);
  preferences_.putUInt("heartbeat_s", config_.heartbeatSeconds);
  return ok;
}

bool ConfigStore::saveCalibration(uint8_t channel) {
  if (channel >= laveggio::kChannelCount) return false;
  const laveggio::ChannelCalibration &calibration = config_.calibrations[channel];
  const String prefix = String("c") + channel;
  preferences_.putUInt((prefix + "mul").c_str(), calibration.multiplierKg);
  preferences_.putUShort((prefix + "tol").c_str(), calibration.tolerance);
  preferences_.putUShort((prefix + "hys").c_str(), calibration.hysteresis);
  for (uint8_t position = 0; position < laveggio::kPositionCount; ++position) {
    preferences_.putBool(
      enabledKey(channel, position).c_str(),
      calibration.points[position].enabled
    );
    preferences_.putUShort(
      calibrationKey(channel, position).c_str(),
      calibration.points[position].raw
    );
  }
  return true;
}

bool ConfigStore::clearCalibration(uint8_t channel) {
  if (channel >= laveggio::kChannelCount) return false;
  for (uint8_t position = 0; position < laveggio::kPositionCount; ++position) {
    config_.calibrations[channel].points[position] = {};
  }
  return saveCalibration(channel);
}
