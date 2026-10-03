// The calibrated transmit clock (Options > Calibrate, txclock) must reach the MMSSTV-engine transmit bridge.
// Links the real mmsstv_sstv_tx.cpp and mmsstv-core; the sound side is stubbed (see stubs/) and just counts
// audio frames. A picture sent with txClock = 48000*(1+e) must contain (1+e) times as many frames as one sent
// at 48000 -- the card plays txClock frames per second, so the same signal timing needs that many -- and the
// bridge must leave the shared mmsstv-core clock globals as it found them (the receive bridge uses them too).
#include "mmsstv_sstv_tx.h"
#include "imageviewer.h"
#include "synthes.h"
#include "sstv.h"
#include <cstdio>
#include <cmath>

double rxClock=48000.0;
double txClock=48000.0;
synthesizer *synthesPtr=nullptr;

static long long send(esstvMode mode,double clock)
{
  int w,h;
  getModeDimensions(mode,w,h);
  imageViewer iv;
  iv.img=QImage(w,h,QImage::Format_RGB32);
  iv.img.fill(qRgb(120,90,200));
  synthesizer synth;
  synthesPtr=&synth;
  txClock=clock;
  if(!sendImageViaMmsstv(&iv,mode)) return -1;
  return synth.frames;
}

int main()
{
  struct {esstvMode mode;const char *name;} modes[]={{M2,"Martin 2"},{R36,"Robot 36"},{PD50,"PD50"}};
  const double errors[]={0.005,-0.005};   // large enough that per-line rounding (tens of ppm) doesn't matter
  int fails=0;
  for(auto &m:modes)
    {
      long long base=send(m.mode,48000.0);
      if(base<=0) {printf("FAIL %s: nothing sent\n",m.name);fails++;continue;}
      for(double e:errors)
        {
          long long n=send(m.mode,48000.0*(1.0+e));
          double ratio=(double)n/(double)base;
          double errPpm=(ratio/(1.0+e)-1.0)*1e6;
          bool ok=n>0 && std::fabs(errPpm)<150.0;
          printf("%s %-8s txclock %+.1f%%: %lld -> %lld frames, ratio %.6f (off by %+.0f ppm from expected)\n",ok?"ok  ":"FAIL",m.name,e*100,base,n,ratio,errPpm);
          if(!ok) fails++;
        }
    }
  // the clock globals are shared with the receive bridge: sending must put them back
  SampFreq=12345.0; sys.m_SampFreq=23456.0; sys.m_TxSampOff=7.0;
  send(M2,48000.0*1.001);
  bool restored=(SampFreq==12345.0 && sys.m_SampFreq==23456.0 && sys.m_TxSampOff==7.0);
  printf("%s globals restored after a send\n",restored?"ok  ":"FAIL");
  if(!restored) fails++;
  printf(fails? "%d FAILED\n":"all passed\n",fails);
  return fails? 1:0;
}
