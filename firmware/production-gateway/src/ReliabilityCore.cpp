#include "ReliabilityCore.h"
#include <cmath>
#include <algorithm>
namespace laveggio {
int64_t rtcUtcEpoch(const uint8_t raw[7],uint8_t control) {
  if((raw[0]&0x80)||(control&0x20)) return 0;
  const uint8_t mask[]={0x7f,0x7f,0x3f,0x3f,0x07,0x1f,0xff}; int v[7];
  for(unsigned i=0;i<7;++i) { const unsigned b=raw[i]&mask[i]; if((b&15)>9 || (b>>4)>9) return 0; v[i]=(b>>4)*10+(b&15); }
  const int year=2000+v[6],month=v[5],day=v[3];
  if(year<2024||month<1||month>12||day<1||v[2]>23||v[1]>59||v[0]>59||v[4]>6) return 0;
  const int lengths[]={31,28+(year%4==0),31,30,31,30,31,31,30,31,30,31};
  if(day>lengths[month-1]) return 0;
  int days=0; for(int y=1970;y<year;++y) days+=365+(y%4==0&&(y%100!=0||y%400==0));
  for(int m=1;m<month;++m) days+=lengths[m-1];
  return int64_t(days+day-1)*86400+v[2]*3600+v[1]*60+v[0];
}
void ClosureDetector::reset() { pending = false; initialized_ = false; quiet_ = false; cooling_ = false; vibrationG = 0; }
bool ClosureDetector::update(float x, float y, float z, bool valid, bool stable, uint32_t now, const ClosureConfig &c) {
  if (!c.enabled || !valid || !std::isfinite(x+y+z)) { reset(); return false; }
  if (!initialized_ || now-previousMs_ > 100) {
    bx_=x; by_=y; bz_=z; initialized_=true; previousMs_=now; pending=false; quiet_=false; return false;
  }
  previousMs_=now;
  const float dx=x-bx_, dy=y-by_, dz=z-bz_;
  vibrationG=std::sqrt(dx*dx+dy*dy+dz*dz);
  bx_+=0.1f*dx; by_+=0.1f*dy; bz_+=0.1f*dz;
  if (cooling_) { if (now-lastDetectedMs < c.cooldownMs) return false; cooling_=false; }
  if (!pending && vibrationG >= c.thresholdG) { pending=true; impulseMs_=now; peakG=vibrationG; quiet_=false; }
  if (!pending) return false;
  peakG=std::max(peakG,vibrationG);
  if (now-impulseMs_ > c.timeoutMs) { pending=false; quiet_=false; return false; }
  if (vibrationG > c.quietG) { quiet_=false; return false; }
  if (!quiet_) { quiet_=true; quietSince_=now; }
  if (!stable || now-quietSince_ < c.quietMs) return false;
  pending=false; cooling_=true; lastDetectedMs=now; ++count; return true;
}
bool validSensorOrder(const uint8_t order[kChannelCount]) {
  uint8_t mask=0; for(uint8_t i=0;i<kChannelCount;++i) { if(order[i]>=kChannelCount || (mask&(1<<order[i]))) return false; mask|=1<<order[i]; } return true;
}
bool calibrationSeparated(const ChannelCalibration &c) {
  for(uint8_t i=0;i<kPositionCount;++i) for(uint8_t j=i+1;j<kPositionCount;++j)
    if(c.points[i].enabled && c.points[j].enabled && circularDistance(c.points[i].raw,c.points[j].raw)<=2*c.tolerance+c.hysteresis) return false;
  return true;
}
uint8_t estimateBatteryPercent(uint16_t mv,uint16_t minimum,uint16_t maximum) {
  if(maximum<=minimum || mv<=minimum) return 0;
  if(mv>=maximum) return 100;
  // Approximate resting LiPo curve. Still a voltage estimate, not a fuel gauge.
  const uint16_t voltage[]={3200,3500,3650,3700,3750,3800,3900,4000,4100,4200};
  const uint8_t percent[]={0,5,10,20,35,50,65,80,90,100};
  const uint32_t normalized=3200+uint32_t(mv-minimum)*1000/(maximum-minimum);
  for(unsigned i=1;i<10;++i) if(normalized<=voltage[i]) return percent[i-1]+(normalized-voltage[i-1])*(percent[i]-percent[i-1])/(voltage[i]-voltage[i-1]);
  return 100;
}
}
