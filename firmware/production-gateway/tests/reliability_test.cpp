#include "ReliabilityCore.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace laveggio;
int main() {
  ChannelCalibration channels[4]; SensorReading readings[4];
  const uint32_t multipliers[]={10000,1000,100,10};
  for(unsigned i=0;i<4;++i) {
    channels[i].multiplierKg=multipliers[i]; channels[i].points[i+1]={true,uint16_t(400*(i+1))};
    readings[i].present=true; readings[i].status=0x20; readings[i].raw=400*(i+1);
  }
  StabilityTracker tracker(600);
  for(unsigned t=0;t<=600;t+=20) tracker.update(readings,channels,t);
  assert(tracker.snapshot().stable && tracker.snapshot().weightKg==12340);
  assert(!tracker.update(readings,channels,2000).stable); // Unobserved interval never counts.
  for(unsigned t=2020;t<=2600;t+=20) tracker.update(readings,channels,t);
  assert(tracker.snapshot().stable);
  readings[1].status=0x10; assert(!tracker.update(readings,channels,2620).valid);
  readings[1].status=0x20; assert(!tracker.update(readings,channels,2640).stable);
  StabilityTracker wrap(600);
  for(unsigned t=0;t<=600;t+=20) wrap.update(readings,channels,UINT32_MAX-400+t);
  assert(wrap.snapshot().stable);
  std::cout<<"PASS fresh stability, invalid magnet, timer wrap\n";

  uint8_t order[]={3,1,0,2}; assert(validSensorOrder(order)); order[1]=3; assert(!validSensorOrder(order)); order[1]=4; assert(!validSensorOrder(order));
  ChannelCalibration c; c.points[0]={true,4090}; c.points[1]={true,10}; assert(!calibrationSeparated(c));
  c.points[1].raw=400; assert(calibrationSeparated(c)); c.tolerance=250; assert(!calibrationSeparated(c));
  std::cout<<"PASS sensor permutation and circular calibration overlap\n";

  uint8_t rtc[]={0,0,0,0x29,4,0x02,0x24};
  assert(rtcUtcEpoch(rtc,0)==1709164800LL);
  rtc[6]=0x25; assert(rtcUtcEpoch(rtc,0)==0); rtc[6]=0x24;
  rtc[0]=0x80; assert(rtcUtcEpoch(rtc,0)==0); rtc[0]=0;
  assert(rtcUtcEpoch(rtc,0x20)==0);
  rtc[1]=0x1a; assert(rtcUtcEpoch(rtc,0)==0);
  std::cout<<"PASS RTC leap year, oscillator stop, invalid BCD\n";

  ClosureConfig config; ClosureDetector detector;
  for(unsigned t=0;t<1000;t+=20) assert(!detector.update(t%40?2:0,0,1,true,true,t,config));
  assert(detector.count==0); config.enabled=true;
  detector.update(0,0,1,true,true,1000,config);
  assert(!detector.update(1,0,1,true,true,1020,config));
  bool detected=false; for(unsigned t=1040;t<=2020;t+=20) detected|=detector.update(0,0,1,true,true,t,config);
  assert(detected && detector.count==1);
  for(unsigned t=2040;t<=3000;t+=20) assert(!detector.update(t%40?1:0,0,1,true,true,t,config));
  assert(detector.count==1);
  detector.reset(); detector.update(0,0,1,true,true,4000,config); detector.update(1,0,1,true,true,4020,config);
  for(unsigned t=4040;t<8000;t+=20) assert(!detector.update(0,0,1,true,false,t,config));
  assert(!detector.pending && detector.count==1);
  detector.reset(); detector.update(0,0,1,true,true,8000,config); detector.update(1,0,1,true,true,8020,config);
  assert(!detector.update(0,0,1,true,true,9000,config)); assert(!detector.pending);
  assert(!detector.update(NAN,0,1,true,true,9020,config));
  std::cout<<"PASS experimental off, impulse/quiet, cooldown, unstable timeout, missed IMU samples\n";
  assert(estimateBatteryPercent(3200,3200,4200)==0);
  assert(estimateBatteryPercent(4200,3200,4200)==100);
  assert(estimateBatteryPercent(3800,3200,4200)==50);
  for(unsigned v=3201;v<4200;++v) assert(estimateBatteryPercent(v,3200,4200)>=estimateBatteryPercent(v-1,3200,4200));
  std::cout<<"PASS LiPo estimate endpoints and monotonicity\n";
}
