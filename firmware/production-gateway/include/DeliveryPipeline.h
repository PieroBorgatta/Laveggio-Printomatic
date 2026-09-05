#pragma once
#include <Arduino.h>
#include "Acquisition.h"

struct DeliveryConfig {
  char deviceId[97], hmac[161], bootId[40];
  char url[256], token[160], ca[3072], certificate[3072], key[3072];
  char mqttHost[129], mqttUser[129], mqttPassword[161], mqttTopic[129];
  uint16_t mqttPort;
  bool mqtt, history;
};
struct DeliveryRecord {
  char id[180], body[3072];
  time_t epoch;
  bool weight = true;
};
struct DeliveryState {
  char eventId[180] = "";
  uint32_t capturedMs=0, dispatched=0, queueDrops=0, httpFailures=0, mqttFailures=0;
  uint32_t maxDispatchMs=0, storageFailures=0;
  bool saved=false, storageAttempted=false, historyEnabled=true;
  bool httpConfigured=false, httpAck=false, mqttConfigured=false, mqttPublished=false;
};
class DeliveryPipeline {
 public:
  bool begin(Acquisition *source,const DeviceConfig &config,const String &bootId);
  void configure(const DeviceConfig &config,const String &bootId);
  bool takeStorage(DeliveryRecord &record);
  void storageResult(const char *id,bool ok);
  DeliveryState status();
 private:
  static void dispatchEntry(void *self), httpEntry(void *self), mqttEntry(void *self);
  void dispatch(), http(), mqtt();
  void report(const DeliveryRecord &record,bool http,bool ok,int code);
  Acquisition *source_=nullptr;
  QueueHandle_t configQueue_=nullptr, httpQueue_=nullptr,mqttQueue_=nullptr,storageQueue_=nullptr;
  portMUX_TYPE mux_=portMUX_INITIALIZER_UNLOCKED;
  DeliveryState state_;
};
