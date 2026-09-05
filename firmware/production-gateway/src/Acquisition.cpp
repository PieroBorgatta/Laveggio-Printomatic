#include "Acquisition.h"
#include <Wire.h>
#include <algorithm>

bool Acquisition::begin(BoardHardware *board, const DeviceConfig &config) {
  board_=board;
  configQueue_=xQueueCreate(1,sizeof(AcquisitionConfig));
  rtcQueue_=xQueueCreate(1,sizeof(time_t));
  eventQueue_=xQueueCreate(32,sizeof(CapturePacket));
  if(!configQueue_ || !eventQueue_ || !rtcQueue_) return false;
  configure(config);
  return xTaskCreatePinnedToCore(entry,"acquisition",6144,this,5,nullptr,1)==pdPASS;
}
void Acquisition::configure(const DeviceConfig &config) {
  if(!configQueue_) return;
  AcquisitionConfig c{};
  memcpy(c.calibration,config.calibrations,sizeof(c.calibration));
  memcpy(c.order,config.sensorOrder,4);
  c.revision=config.calibrationRevision; c.stableMs=config.stableWindowMs; c.closure=config.closure;
  xQueueOverwrite(configQueue_,&c);
}
void Acquisition::syncRtc(time_t epoch) { if(rtcQueue_) xQueueOverwrite(rtcQueue_,&epoch); }
AcquisitionState Acquisition::snapshot() { portENTER_CRITICAL(&mux_); const auto result=published_; portEXIT_CRITICAL(&mux_); return result; }
bool Acquisition::take(CapturePacket &p,TickType_t wait) { return eventQueue_ && xQueueReceive(eventQueue_,&p,wait)==pdTRUE; }
void Acquisition::entry(void *self) { static_cast<Acquisition*>(self)->run(); }
bool Acquisition::read(uint8_t reg,uint8_t *data,size_t n) {
  Wire.beginTransmission(0x36); Wire.write(reg);
  if(Wire.endTransmission(false)!=0 || Wire.requestFrom(uint8_t(0x36),n)!=n) return false;
  for(size_t i=0;i<n;++i) data[i]=Wire.read(); return true;
}
void Acquisition::run() {
  AcquisitionConfig config{};
  AcquisitionState state{};
  laveggio::StabilityTracker tracker;
  laveggio::ClosureDetector detector;
  uint16_t rawWindow[4][25]={}; uint8_t cursor=0;
  uint32_t lastStart=0,lastDiscovery=0,lastHealth=0;
  bool previousHealthy[4]={}, healthInitialized[4]={};
  TickType_t wake=xTaskGetTickCount();
  Wire.setTimeOut(2);
  for(;;) {
    const uint32_t start=millis();
    AcquisitionConfig next;
    if(xQueueReceive(configQueue_,&next,0)==pdTRUE) {
      if(next.revision!=config.revision || next.stableMs!=config.stableMs || memcmp(next.order,config.order,4)!=0) {
        tracker=laveggio::StabilityTracker(next.stableMs); memset(state.samples,0,sizeof(state.samples));
      }
      detector.reset(); config=next;
    }
    if(lastStart) { const uint32_t gap=start-lastStart; state.maxGapMs=std::max(state.maxGapMs,gap); if(gap>30) ++state.overruns; }
    lastStart=start;
    if(state.muxAddress<0 && (lastDiscovery==0 || start-lastDiscovery>=2000)) {
      lastDiscovery=start;
      for(uint8_t address=0x70;address<=0x77;++address) {
        Wire.beginTransmission(address);
        if(Wire.endTransmission()==0) { state.muxAddress=address; break; }
      }
    }
    const bool health=start-lastHealth>=250;
    for(uint8_t physical=0;physical<4;++physical) {
      auto &sensor=state.sensors[physical];
      uint8_t raw[3]={};
      bool selected=false;
      if(state.muxAddress>=0) {
        Wire.beginTransmission(uint8_t(state.muxAddress)); Wire.write(1<<physical); selected=Wire.endTransmission()==0;
      }
      // Status and raw angle are adjacent; never reuse stale magnet health.
      sensor.present=selected && read(0x0B,raw,3);
      if(sensor.present) {
        sensor.status=raw[0]; sensor.raw=((uint16_t(raw[1])<<8)|raw[2])&4095;
        if(health) { uint8_t detail[3]; if(read(0x1A,detail,3)) { sensor.agc=detail[0]; sensor.magnitude=((uint16_t(detail[1])<<8)|detail[2])&4095; } }
      }
      if(!sensor.present) ++state.readFailures[physical];
      if(health) {
        if(!sensor.present) ++state.missingSamples[physical];
        if(sensor.present&&sensor.magnetWeak()) ++state.weakSamples[physical];
        if(sensor.present&&sensor.magnetStrong()) ++state.strongSamples[physical];
        if(healthInitialized[physical]&&previousHealthy[physical]&&!sensor.healthy()) ++state.unhealthyTransitions[physical];
        healthInitialized[physical]=true; previousHealthy[physical]=sensor.healthy();
      }
      if(!sensor.healthy()) { state.samples[physical]=0; state.noise[physical]=0; continue; }
      rawWindow[physical][cursor]=sensor.raw;
      state.samples[physical]=std::min<uint16_t>(25,state.samples[physical]+1);
      uint16_t spread=0;
      for(uint16_t i=0;i<state.samples[physical];++i) spread=std::max(spread,laveggio::circularDistance(sensor.raw,rawWindow[physical][(cursor+25-i)%25]));
      state.noise[physical]=spread;
    }
    cursor=(cursor+1)%25;
    if(health) lastHealth=start;
    if(state.muxAddress>=0) { Wire.beginTransmission(uint8_t(state.muxAddress)); Wire.write(0); if(Wire.endTransmission()!=0) state.muxAddress=-1; }
    laveggio::SensorReading logical[4]; laveggio::ChannelCalibration calibration[4];
    for(uint8_t slot=0;slot<4;++slot) { logical[slot]=state.sensors[config.order[slot]]; calibration[slot]=config.calibration[config.order[slot]]; calibration[slot].multiplierKg=config.calibration[slot].multiplierKg; }
    const uint32_t acquired=millis();
    state.weight=tracker.update(logical,calibration,acquired); state.capturedMs=acquired; ++state.scans;
    auto emit=[&](bool closure) {
      CapturePacket packet{}; packet.state=state; packet.revision=config.revision; packet.epoch=time(nullptr);
      packet.timeSource=timeSource_.load(); packet.closure=closure; packet.completed=closure&&config.closure.completeWeight;
      memcpy(packet.order,config.order,4); for(uint8_t i=0;i<4;++i) packet.multipliers[i]=config.calibration[i].multiplierKg;
      if(xQueueSend(eventQueue_,&packet,0)!=pdTRUE) ++state.droppedEvents;
    };
    if(state.weight.changed) emit(false);
    // Optional board work uses the remaining time AFTER all four critical readings.
    // Never perform SD, networking, String allocation or NVS writes on this task.
    bool imuFresh=false;
    if(millis()-start<10) {
      imuFresh=board_->pollMotion(acquired,config.closure.enabled);
      if(millis()-start<10) { time_t epoch; if(xQueueReceive(rtcQueue_,&epoch,0)==pdTRUE) board_->synchronizeRtc(epoch); else board_->poll(acquired); }
    }
    state.board=board_->status();
    bool closure=false;
    if(imuFresh) closure=detector.update(state.board.accelerationX,state.board.accelerationY,state.board.accelerationZ,state.board.imuAvailable,state.weight.valid&&state.weight.stable,acquired,config.closure);
    else if(!config.closure.enabled || !state.board.imuAvailable) detector.reset();
    state.vibrationG=detector.vibrationG; state.closurePeakG=detector.peakG; state.closureCount=detector.count;
    state.closureAtMs=detector.lastDetectedMs; state.closurePending=detector.pending;
    if(closure) emit(true);
    portENTER_CRITICAL(&mux_); published_=state; portEXIT_CRITICAL(&mux_);
    if(millis()-start>=20) wake=xTaskGetTickCount();
    vTaskDelayUntil(&wake,pdMS_TO_TICKS(20));
  }
}
