#include "wwvtickdetector.h"
#include <cmath>
#include <algorithm>

#define TICKSECONDS 0.005     // duration of the WWV/WWVH tick burst
#define PEAKTHRESHOLD 5.0     // a tick must be this many times above the average matched filter output
#define SHAPEFRACTION 0.35    // ...and the output 10 ms before and after must be below this fraction of the peak
#define MINABSOLUTE 3.0       // per window sample (sample units); stops digital silence from triggering

wwvTickDetector::wwvTickDetector(double sampleRate,double toneHz) : fs(sampleRate),tone(toneHz)
{
  window=(int)std::lround(TICKSECONDS*fs);
  if(window<4) window=4;
  init();
}

void wwvTickDetector::setTone(double toneHz)
{
  tone=toneHz;
  reset();
}

void wwvTickDetector::init()
{
  const double pi=std::acos(-1.0);
  phaseStep=2.0*pi*tone/fs;
  ringRe.assign(window,0.0);
  ringIm.assign(window,0.0);
  magRing.assign(4*window+8,0.0);
  phase=0;
  sampleCount=0;
  sumRe=sumIm=0;
  noise=0;
  prevMag=0;
  inPeak=false;
  bestMag=0;
  bestIndex=0;
}

void wwvTickDetector::reset()
{
  init();
}

void wwvTickDetector::process(const double *samples,int n,std::vector<wwvCandidate> &out)
{
  const double twoPi=2.0*std::acos(-1.0);
  const long long R=(long long)magRing.size();
  for(int i=0;i<n;i++)
    {
      long long idx=sampleCount;
      // mix down with the tick tone; the phase is tied to the absolute sample number so the magnitude of the
      // window sum does not depend on where in its cycle the tick starts
      double c=std::cos(phase);
      double s=-std::sin(phase);
      phase+=phaseStep;
      if(phase>=twoPi) phase-=twoPi;
      int w=(int)(idx%window);
      sumRe+=samples[i]*c-ringRe[w];
      sumIm+=samples[i]*s-ringIm[w];
      ringRe[w]=samples[i]*c;
      ringIm[w]=samples[i]*s;
      sampleCount++;
      if(w==0 && (idx/window)%((long long)fs/window+1)==0)
        {
          // running sums drift by rounding error; rebuild them from the ring now and then
          sumRe=sumIm=0;
          for(int k=0;k<window;k++) {sumRe+=ringRe[k];sumIm+=ringIm[k];}
        }
      bool valid=(idx>=window-1);
      double mag=valid? std::hypot(sumRe,sumIm):0.0;
      magRing[idx%R]=mag;
      if(valid)
        {
          double alpha=1.0/std::min<double>((double)(idx-window+2),2.0*fs);
          noise+=(mag-noise)*alpha;
        }
      double thr=std::max(PEAKTHRESHOLD*noise,MINABSOLUTE*window);
      if(!inPeak)
        {
          if(valid && mag>thr && mag>=prevMag)
            {
              inPeak=true;
              bestMag=mag;
              bestIndex=idx;
            }
        }
      else
        {
          if(mag>bestMag)
            {
              bestMag=mag;
              bestIndex=idx;
            }
          else if(idx-bestIndex>=2*window)
            {
              inPeak=false;
              // a 5 ms burst is quiet again 10 ms later and was quiet 10 ms before
              long long back=bestIndex-2*window;
              double lookBack=(back>=window-1)? magRing[back%R]:bestMag;
              if(mag<SHAPEFRACTION*bestMag && lookBack<SHAPEFRACTION*bestMag && bestMag>thr)
                {
                  double yPrev=magRing[(bestIndex-1)%R];
                  double yNext=magRing[(bestIndex+1)%R];
                  double denom=2.0*(bestMag-std::min(yPrev,yNext));
                  double delta=(denom>0)? (yNext-yPrev)/denom:0.0;
                  delta=std::max(-0.5,std::min(0.5,delta));
                  wwvCandidate cand;
                  cand.position=(double)(bestIndex-window+1)+delta;
                  cand.strength=(noise>0)? bestMag/noise:0.0;
                  out.push_back(cand);
                }
            }
        }
      prevMag=mag;
    }
}
