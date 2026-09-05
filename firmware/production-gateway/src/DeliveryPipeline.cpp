#include "DeliveryPipeline.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <mbedtls/md.h>
#include <esp_heap_caps.h>
#include <algorithm>

namespace {
struct Transmission { DeliveryRecord record; DeliveryConfig config; };
QueueHandle_t largeQueue(size_t count,size_t item) {
  auto *control=new StaticQueue_t;
  auto *buffer=static_cast<uint8_t*>(heap_caps_malloc(count*item,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!buffer) { delete control; return nullptr; }
  return xQueueCreateStatic(count,item,buffer,control);
}
String iso(time_t epoch) {
  if(epoch<1700000000) return "";
  tm t{}; gmtime_r(&epoch,&t); char value[32]; strftime(value,sizeof(value),"%Y-%m-%dT%H:%M:%S+0000",&t); return value;
}
String signature(const char *key,const String &payload) {
  if(!key[0]) return "";
  unsigned char digest[32]; char hex[65];
  if(mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),reinterpret_cast<const uint8_t*>(key),strlen(key),reinterpret_cast<const uint8_t*>(payload.c_str()),payload.length(),digest)!=0) return "";
  for(unsigned i=0;i<32;++i) snprintf(hex+2*i,3,"%02x",digest[i]); return hex;
}
}
bool DeliveryPipeline::begin(Acquisition *source,const DeviceConfig &c,const String &boot) {
  source_=source;
  configQueue_=largeQueue(1,sizeof(DeliveryConfig)); httpQueue_=largeQueue(32,sizeof(Transmission));
  mqttQueue_=largeQueue(32,sizeof(Transmission)); storageQueue_=largeQueue(64,sizeof(DeliveryRecord));
  if(!configQueue_||!httpQueue_||!mqttQueue_||!storageQueue_) return false;
  configure(c,boot);
  return xTaskCreatePinnedToCore(dispatchEntry,"weight_dispatch",8192,this,4,nullptr,0)==pdPASS &&
    xTaskCreatePinnedToCore(httpEntry,"weight_https",12288,this,3,nullptr,0)==pdPASS &&
    xTaskCreatePinnedToCore(mqttEntry,"weight_mqtt",10240,this,3,nullptr,0)==pdPASS;
}
void DeliveryPipeline::configure(const DeviceConfig &c,const String &boot) {
  if(!configQueue_) return;
  auto *v=new DeliveryConfig{};
  #define COPY(field,value) strlcpy(v->field,(value).c_str(),sizeof(v->field))
  COPY(deviceId,c.deviceId); COPY(hmac,c.eventHmacSecret); COPY(bootId,boot);
  COPY(url,c.backendUrl); COPY(token,c.backendToken); COPY(ca,c.tlsCaCertificate); COPY(certificate,c.tlsClientCertificate); COPY(key,c.tlsClientPrivateKey);
  COPY(mqttHost,c.mqttHost); COPY(mqttUser,c.mqttUsername); COPY(mqttPassword,c.mqttPassword); COPY(mqttTopic,c.mqttBaseTopic);
  while(strlen(v->mqttTopic)&&v->mqttTopic[strlen(v->mqttTopic)-1]=='/') v->mqttTopic[strlen(v->mqttTopic)-1]=0;
  #undef COPY
  v->mqtt=c.mqttEnabled; v->mqttPort=c.mqttPort; v->history=c.historyEnabled;
  xQueueOverwrite(configQueue_,v); delete v;
}
DeliveryState DeliveryPipeline::status() { portENTER_CRITICAL(&mux_); const auto value=state_; portEXIT_CRITICAL(&mux_); return value; }
bool DeliveryPipeline::takeStorage(DeliveryRecord &r) { return storageQueue_&&xQueueReceive(storageQueue_,&r,0)==pdTRUE; }
void DeliveryPipeline::storageResult(const char *id,bool ok) {
  portENTER_CRITICAL(&mux_); if(!ok) ++state_.storageFailures;
  if(strcmp(id,state_.eventId)==0) { state_.storageAttempted=true; state_.saved=ok; } portEXIT_CRITICAL(&mux_);
}
void DeliveryPipeline::dispatchEntry(void *p) { static_cast<DeliveryPipeline*>(p)->dispatch(); }
void DeliveryPipeline::httpEntry(void *p) { static_cast<DeliveryPipeline*>(p)->http(); }
void DeliveryPipeline::mqttEntry(void *p) { static_cast<DeliveryPipeline*>(p)->mqtt(); }
void DeliveryPipeline::dispatch() {
  auto *job=new Transmission{};
  CapturePacket packet{}; uint32_t sequence=0;
  for(;;) {
    if(!source_->take(packet,pdMS_TO_TICKS(100))) continue;
    xQueueReceive(configQueue_,&job->config,0);
    const auto &c=job->config;
    auto &r=job->record; r=DeliveryRecord{}; r.epoch=packet.epoch;
    snprintf(r.id,sizeof(r.id),"%s:%s:w%lu",c.deviceId,c.bootId,static_cast<unsigned long>(++sequence));
    const String captured=iso(packet.epoch);
    String digits; for(unsigned i=0;i<4;++i) { if(i) digits+='.'; digits+=packet.state.weight.digits[i]; }
    const bool closureOnly=packet.closure&&!packet.completed;
    JsonDocument doc;
    doc["type"]=closureOnly?"scale.closure_candidate":"scale.snapshot";
    doc["schema_version"]=1; doc["event_id"]=r.id; doc["device_id"]=c.deviceId; doc["boot_id"]=c.bootId;
    doc["sequence"]=sequence; doc["captured_at"]=captured; doc["captured_ms"]=packet.state.capturedMs;
    doc["time_synchronized"]=packet.timeSource==2; doc["time_valid"]=packet.epoch>=1700000000; doc["time_source"]=packet.timeSource==2?"ntp":packet.timeSource==1?"rtc":"unavailable";
    doc["weight_kg"]=packet.state.weight.weightKg; doc["stable"]=packet.state.weight.stable; doc["valid"]=packet.state.weight.valid;
    doc["calibration_revision"]=packet.revision;
    doc["completion_experimental"]=packet.completed; doc["weight_completed"]=packet.completed;
    doc["closure_detected"]=packet.closure; doc["closure_peak_g"]=packet.state.closurePeakG;
    for(unsigned i=0;i<4;++i) {
      doc["digits"][i]=packet.state.weight.digits[i]; doc["multipliers_kg"][i]=packet.multipliers[i]; doc["sensor_order"][i]=packet.order[i];

    }
    auto sensors=doc["sensors"].to<JsonArray>();
    for(unsigned i=0;i<4;++i) { const auto &s=packet.state.sensors[packet.order[i]]; auto item=sensors.add<JsonObject>();
      item["channel"]=packet.order[i]; item["raw"]=s.raw; item["healthy"]=s.healthy(); item["status"]=s.status; item["agc"]=s.agc; item["magnitude"]=s.magnitude; }
    const String sig=signature(c.hmac,String(r.id)+"\n"+captured+"\n"+packet.state.weight.weightKg+"\n"+digits);
    if(sig.length()) { doc["signature_alg"]="HMAC-SHA256"; doc["signature"]=sig; }
    else { doc["signature_alg"]=nullptr; doc["signature"]=nullptr; }
    const String completionPayload=String(r.id)+"\n"+captured+"\n"+packet.state.weight.weightKg+"\n"+digits+"\n"+packet.revision+"\n"+(packet.completed?"true":"false");
    if(c.hmac[0]) doc["completion_signature"]=signature(c.hmac,completionPayload);
    doc["delivery"]=(c.url[0]||c.mqtt)?"requested":"local";
    const size_t length=measureJson(doc);
    if(length>=sizeof(r.body)) { portENTER_CRITICAL(&mux_); ++state_.queueDrops; portEXIT_CRITICAL(&mux_); continue; }
    serializeJson(doc,r.body,sizeof(r.body)); r.weight=!closureOnly;
    portENTER_CRITICAL(&mux_);
    if(!closureOnly) {
    strlcpy(state_.eventId,r.id,sizeof(state_.eventId)); state_.capturedMs=packet.state.capturedMs;
    state_.saved=false; state_.storageAttempted=false; state_.historyEnabled=c.history;
    state_.httpConfigured=c.url[0]; state_.httpAck=false; state_.mqttConfigured=c.mqtt; state_.mqttPublished=false;
    }
    ++state_.dispatched; state_.maxDispatchMs=std::max(state_.maxDispatchMs,millis()-packet.state.capturedMs);
    portEXIT_CRITICAL(&mux_);
    // Network queues are populated before any optional storage processing.
    uint32_t drops=0;
    if(!closureOnly && c.url[0] && xQueueSend(httpQueue_,job,0)!=pdTRUE) ++drops;
    if(!closureOnly && c.mqtt && xQueueSend(mqttQueue_,job,0)!=pdTRUE) ++drops;
    if(c.history && xQueueSend(storageQueue_,&r,0)!=pdTRUE) { ++drops; storageResult(r.id,false); }
    if(drops) { portENTER_CRITICAL(&mux_); state_.queueDrops+=drops; portEXIT_CRITICAL(&mux_); }
  }
}
void DeliveryPipeline::report(const DeliveryRecord &r,bool http,bool ok,int code) {
  portENTER_CRITICAL(&mux_);
  if(!ok) { if(http) ++state_.httpFailures; else ++state_.mqttFailures; }
  if(strcmp(r.id,state_.eventId)==0) { if(http) state_.httpAck=ok; else state_.mqttPublished=ok; }
  portEXIT_CRITICAL(&mux_);
  DeliveryRecord audit{}; audit.weight=false; audit.epoch=time(nullptr);
  snprintf(audit.body,sizeof(audit.body),"{\"type\":\"delivery.result\",\"event_id\":\"%s\",\"transport\":\"%s\",\"ok\":%s,\"code\":%d}",r.id,http?"https":"mqtt",ok?"true":"false",code);
  if(xQueueSend(storageQueue_,&audit,0)!=pdTRUE) { portENTER_CRITICAL(&mux_); ++state_.queueDrops; portEXIT_CRITICAL(&mux_); }
}
void DeliveryPipeline::http() {
  auto *job=new Transmission{};
  for(;;) {
    if(xQueueReceive(httpQueue_,job,portMAX_DELAY)!=pdTRUE) continue;
    int code=-1; auto &c=job->config;
    if(WiFi.status()==WL_CONNECTED && c.ca[0] && String(c.url).startsWith("https://")) {
      WiFiClientSecure client; client.setCACert(c.ca); client.setHandshakeTimeout(3);
      if(c.certificate[0]&&c.key[0]) { client.setCertificate(c.certificate); client.setPrivateKey(c.key); }
      HTTPClient http; http.setConnectTimeout(1500); http.setTimeout(1500);
      if(http.begin(client,c.url)) { http.addHeader("Content-Type","application/json");
        if(c.token[0]) http.addHeader("Authorization",String("Bearer ")+c.token);
        code=http.POST(reinterpret_cast<uint8_t*>(job->record.body),strlen(job->record.body)); http.end(); }
    }
    report(job->record,true,code>=200&&code<300,code);
  }
}
void DeliveryPipeline::mqtt() {
  auto *job=new Transmission{};
  WiFiClientSecure tls; PubSubClient mqtt(tls); mqtt.setBufferSize(4096); mqtt.setSocketTimeout(2);
  String identity;
  for(;;) {
    if(xQueueReceive(mqttQueue_,job,pdMS_TO_TICKS(100))!=pdTRUE) { if(mqtt.connected()) mqtt.loop(); continue; }
    auto &c=job->config; bool ok=false;
    const String next=String(c.mqttHost)+":"+c.mqttPort+":"+c.deviceId+":"+c.mqttUser+":"+c.mqttPassword+":"+c.ca+":"+c.certificate+":"+c.key;
    if(next!=identity) { mqtt.disconnect(); tls.stop(); identity=next; }
    if(WiFi.status()==WL_CONNECTED && c.ca[0]) {
      tls.setCACert(c.ca); tls.setHandshakeTimeout(3);
      if(c.certificate[0]&&c.key[0]) { tls.setCertificate(c.certificate); tls.setPrivateKey(c.key); }
      mqtt.setServer(c.mqttHost,c.mqttPort);
      if(mqtt.connected() || mqtt.connect((String(c.deviceId)+"-weights").c_str(),c.mqttUser,c.mqttPassword))
        ok=mqtt.publish((String(c.mqttTopic)+"/"+c.deviceId+"/weights").c_str(),job->record.body,false);
    }
    // QoS0 publication is not an application acknowledgement.
    report(job->record,false,ok,mqtt.state());
  }
}
