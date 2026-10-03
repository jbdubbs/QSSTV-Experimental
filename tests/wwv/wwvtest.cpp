// Offline test of the WWV tick detector and sample-rate fit with synthetic audio (see build.sh).
//   ./wwvtest            run the built-in cases, exit status 0 if all pass
#include "wwvtickdetector.h"
#include "wwvtickfit.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <random>
#include <vector>

struct testCase
{
  const char *name;
  double nominal;     // sample rate the application believes
  double ppm;         // actual sample rate error of the card
  double tone;        // tick tone
  double snrDb;       // tick amplitude over white noise rms (in the full band)
  double missRate;    // fraction of ticks that are lost
  double jitterMs;    // propagation jitter (rms)
  bool tone500;       // continuous 500 Hz tone (WWV minute 1/3/...) between the ticks
  bool speech;        // random 1 kHz bursts
  double seconds;
  double maxErrPpm;   // pass limit on |measured-true|
};

static bool run(const testCase &t)
{
  std::mt19937 rng(1234);
  std::normal_distribution<double> gauss(0.0,1.0);
  std::uniform_real_distribution<double> uni(0.0,1.0);
  const double fsTrue=t.nominal*(1.0+t.ppm*1e-6);
  const long long total=(long long)(t.seconds*fsTrue);
  const double amp=8000.0;
  const double noiseRms=amp/std::pow(10.0,t.snrDb/20.0);
  std::vector<double> x(total);
  for(long long n=0;n<total;n++) x[n]=noiseRms*gauss(rng);
  const double pi=std::acos(-1.0);
  if(t.tone500)
    for(long long n=0;n<total;n++)
      {
        double sec=std::fmod(n/fsTrue,1.0);
        if(sec>0.04 && sec<0.99) x[n]+=0.5*amp*std::sin(2*pi*500.0*n/fsTrue);   // 10 ms before / 25+ ms after each tick is silent
      }
  for(int k=0;k<(int)t.seconds;k++)
    {
      if(uni(rng)<t.missRate) continue;
      double start=(k+0.2)+t.jitterMs*1e-3*gauss(rng);      // arbitrary offset inside the second
      long long n0=(long long)std::ceil(start*fsTrue);
      for(long long n=n0;n<n0+(long long)(0.005*fsTrue) && n<total;n++)
        x[n]+=amp*std::sin(2*pi*t.tone*n/fsTrue);
    }
  if(t.speech)
    for(int b=0;b<(int)(t.seconds*3);b++)   // 30-200 ms bursts of 900-1100 Hz at random places
      {
        long long n0=(long long)(uni(rng)*(total-20000));
        long long len=(long long)((0.03+0.17*uni(rng))*fsTrue);
        double f=900+200*uni(rng);
        for(long long n=n0;n<n0+len && n<total;n++) x[n]+=0.6*amp*std::sin(2*pi*f*n/fsTrue);
      }
  wwvTickDetector det(t.nominal,t.tone);
  wwvTickFit fit(t.nominal);
  std::vector<wwvCandidate> cands;
  for(long long n=0;n<total;n+=4096)
    {
      int len=(int)std::min<long long>(4096,total-n);
      cands.clear();
      det.process(&x[n],len,cands);
      for(size_t i=0;i<cands.size();i++) fit.add(cands[i]);
      if(!cands.empty()) fit.solve();
    }
  const wwvFitResult &r=fit.result();
  double err=r.ppm-t.ppm;
  bool ok=r.valid && std::fabs(err)<=t.maxErrPpm;
  printf("%-34s true %+8.2f ppm  measured %+8.2f ppm (err %+6.2f, se %.2f)  ticks %3d rms %.2f ms  %s\n",
         t.name,t.ppm,r.ppm,err,r.ppmError,r.ticks,r.rmsMs,ok?"ok":"FAIL");
  return ok;
}

int main()
{
  testCase cases[]=
  {
    //name                         nominal ppm     tone   snr  miss jit  500   speech secs  maxErr
    {"clean, +20 ppm",             48000,  20.0,  1000.0, 30,  0.0, 0.0, false,false, 120, 0.5},
    {"clean, -35 ppm",             48000, -35.0,  1000.0, 30,  0.0, 0.0, false,false, 120, 0.5},
    {"zero error",                 48000,   0.0,  1000.0, 30,  0.0, 0.0, false,false, 120, 0.5},
    {"large error +400 ppm",       48000, 400.0,  1000.0, 30,  0.0, 0.0, false,false, 120, 1.0},
    {"noisy (10 dB), 5 min",       48000,  12.0,  1000.0, 10,  0.1, 0.3, false,false, 300, 1.0},
    {"misses+jitter+500 Hz tone",  48000,  -8.0,  1000.0, 15,  0.2, 0.5, true, false, 300, 1.0},
    {"speech bursts",              48000,  25.0,  1000.0, 15,  0.1, 0.3, true, true,  300, 1.5},
    {"WWVH 1200 Hz",               48000,  17.0,  1200.0, 20,  0.1, 0.3, false,false, 180, 1.0},
    {"44.1 kHz stream",            44100, -22.0,  1000.0, 20,  0.1, 0.3, false,false, 180, 1.0},
  };
  int fails=0;
  for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++) if(!run(cases[i])) fails++;
  printf(fails? "%d case(s) FAILED\n":"all passed\n",fails);
  return fails? 1:0;
}
