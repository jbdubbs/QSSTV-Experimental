#include "synctrain.h"
#include <cmath>

#define STMS(x)        ((uint)((SAMPLERATE*(x))/1000))
#define STSEPARATION   STMS(50)       // an event less than this after the previous one is not counted
#define STMININTERVAL  STMS(63)       // the shortest interval that is compared with a line time
#define STTOLERANCE    STMS(3)        // how far an interval may be from the line time (or a whole number of them)
#define STHOLD         (10*SAMPLERATE) // after a train no other is reported for this long
#define STMAXMISSED    3              // an interval may hold up to this many line times (lines that were missed)
#define STOLDDEFAULT   2              // intervals before the newest one that have to fit (the train is this + 1 intervals)

syncTrainDetector::syncTrainDetector()
{
  pre[0].highpass(700,0.70710678);
  pre[1].highpass(700,0.70710678);
  pre[2].lowpass(2300,0.70710678);
  pre[3].lowpass(2300,0.70710678);
  static const double offsets[STBANKS]={-STBANKSPACING,0,STBANKSPACING};
  for(int k=0;k<STBANKS;k++)
    {
      banks[k].t12.init(1200+offsets[k],100);
      banks[k].t19.init(1900+offsets[k],100);
    }
  for(int m=0;m<NUMSSTVMODES;m++)
    {
      period[m]=0;
      need[m]=0;
    }
  setClock(SAMPLERATE);
  reset();
}

void syncTrainDetector::reset()
{
  for(int i=0;i<4;i++) pre[i].clear();
  for(int k=0;k<STBANKS;k++)
    {
      banks[k].t12.clear();
      banks[k].t19.clear();
    }
  localPos=0;
  inRun=false;
  runBest=0;
  runBestPos=0;
  nAcc=nIvs=0;
  busy=false;
  busyUntil=0;
  hit=false;
  nFresh=0;
}

/**
 * The modes that can be found by their rhythm: all the wide modes with sync pulses except the two Scottie 2 modes of 60 and 120 s, whose
 * lines are so short that an interval of several of them says nothing. Two intervals besides the newest one have to fit; the modes
 * whose line time is near that of the lines of several others (Robot 24 and 36, PD50, PD240) need three, and the black and white modes,
 * whose line is 67 and 100 ms, need seven.
 */

void syncTrainDetector::setClock(double clock)
{
  for(int m=STARTWIDE;m<=ENDWIDE;m++)
    {
      period[m]=0;
      need[m]=0;
      if(m==SC2_60 || m==SC2_120) continue;
      period[m]=getLineLength((esstvMode)m,clock);
      need[m]=STOLDDEFAULT;
      if(m==R24 || m==R36 || m==PD50 || m==PD240) need[m]=STOLDDEFAULT+1;
      if(m==BW8 || m==BW12) need[m]=7;
    }
}

/** @return the number of line times (1..STMAXMISSED) that make up the interval w, 0 if none does */

int syncTrainDetector::fits(double w,double T) const
{
  for(int k=1;k<=STMAXMISSED;k++)
    {
      if((w/k)>STMININTERVAL && fabs(w/k-T)<STTOLERANCE) return k;
    }
  return 0;
}

bool syncTrainDetector::process(const float *audio,unsigned int syncSampleCtr,esstvMode first,esstvMode last,double level,bool armed)
{
  hit=false;
  nFresh=0;
  for(int i=0;i<RXSTRIPE;i++,localPos++)
    {
      double x=audio[i];
      for(int k=0;k<4;k++) x=pre[k].run(x);
      // hard limiter: noise, a signal and a crash all count the same, so what the tones say is their share of the energy
      double y=(x>1.)?1.:((x<-1.)?-1.:x);
      double u=-1e9;
      for(int k=0;k<STBANKS;k++)
        {
          double d=banks[k].t12.run(y)-banks[k].t19.run(y);
          if(d>u) u=d;
        }
      if(level<=0)
        {
          inRun=false;
          continue;
        }
      if(u>=level)
        {
          if(!inRun)
            {
              inRun=true;
              runBest=u;
              runBestPos=localPos;
            }
          else if(u>runBest)
            {
              runBest=u;
              runBestPos=localPos;
            }
        }
      else if(inRun)
        {
          // the event is over: its position is where the difference was largest
          inRun=false;
          event(runBestPos,(int)(syncSampleCtr+i),localPos,first,last,armed);
        }
    }
  return hit;
}

/**
 * One sync event, at localPos pos. The train is looked for when the event completes it.
 */

void syncTrainDetector::event(uint pos,int counterNow,uint localNow,esstvMode first,esstvMode last,bool armed)
{
  if(nAcc>0 && (pos-acc[nAcc-1])<=STSEPARATION) return;
  if(nAcc>0)
    {
      if(nIvs==STMAXINTERVALS)
        {
          for(uint k=1;k<nIvs;k++) ivs[k-1]=ivs[k];
          nIvs--;
        }
      ivs[nIvs++]=pos-acc[nAcc-1];
    }
  if(nAcc==STMAXPEAKS)
    {
      for(uint k=1;k<nAcc;k++) acc[k-1]=acc[k];
      nAcc--;
    }
  acc[nAcc++]=pos;
  if(nFresh<4) freshPos[nFresh++]=counterNow-(int)(localNow-pos);
  if(!armed) return;
  if(busy && (int)(pos-busyUntil)<0) return;
  if(nIvs==0 || ivs[nIvs-1]<=STMININTERVAL) return;
  if(match(first,last,counterNow,localNow))
    {
      busy=true;
      busyUntil=pos+STHOLD;
      hit=true;
    }
}

/**
 * Do the intervals between the last events fit the line time of a mode? The newest interval and the ones before it (STOLDDEFAULT
 * of them, more for some modes) have to be a line time within STTOLERANCE, or two or three line times when lines were missed.
 * The mode with the smallest total deviation wins; the train is then followed back through the older events while they fit.
 */

bool syncTrainDetector::match(esstvMode first,esstvMode last,int counterNow,uint localNow)
{
  double bestKey=1e18;
  int bestMode=-1;
  uint nOther=0;
  esstvMode other[4];
  for(int m=first;m<=last;m++)
    {
      if(period[m]<=0) continue;
      double T=period[m];
      if((int)nIvs<need[m]+1) continue;
      int k=fits(ivs[nIvs-1],T);
      if(!k) continue;
      double dev=fabs(ivs[nIvs-1]/(double)k-T);
      int missed=k-1;
      int single=(k==1)?1:0;
      bool good=true;
      for(int j=2;j<=need[m]+1;j++)
        {
          int kk=fits(ivs[nIvs-j],T);
          if(!kk)
            {
              good=false;
              break;
            }
          dev+=fabs(ivs[nIvs-j]/(double)kk-T);
          missed+=kk-1;
          if(kk==1) single++;
        }
      // (intervals that are all two or three line times are the train of a slower mode, not of this one with lines missed)
      if(!good || !single) continue;
      // A mode that needs lines to have been missed to fit is less likely than one that fits without (the intervals of Robot 72 are
      // those of Robot 36 with every other line missed): the fewest missed lines first, then the smallest deviation.
      double key=missed*1e6+dev;
      if(key<bestKey)
        {
          if(bestMode>=0 && nOther<4) other[nOther++]=(esstvMode)bestMode;
          bestKey=key;
          bestMode=m;
        }
      else if(nOther<4)
        {
          other[nOther++]=(esstvMode)m;
        }
    }
  if(bestMode<0) return false;
  // back-fill: the older events are part of the train as long as their intervals fit
  uint n=1;
  while(n<nIvs && fits(ivs[nIvs-1-n],period[bestMode])) n++;
  found.mode=(esstvMode)bestMode;
  found.count=n+1;
  for(uint k=0;k<=n;k++)
    {
      uint p=acc[nAcc-1-n+k];
      found.peak[k]=counterNow-(int)(localNow-p);
    }
  found.ambiguous=nOther;
  for(uint k=0;k<nOther;k++) found.other[k]=other[k];
  return true;
}

/** end - event, (start+end)/2 + sync width/2 - event, in ms, per mode; the events are the peaks of the strength at 12 kHz (see synctrain.h) */

void syncTrainDetector::pulseDelayMs(esstvMode mode,double &end,double &corrected)
{
  static const struct {esstvMode mode; float end; float corrected;} table[]=
  {
    {M1,3.29,2.90},{M2,3.34,2.95},{S1,4.69,4.14},{S2,4.54,4.09},{SDX,4.59,4.21},{SC2_180,3.11,2.80},
    {R24,2.71,2.54},{R36,3.99,3.67},{R72,4.13,3.81},{P3,3.06,2.67},{P5,4.05,3.69},{P7,4.67,4.37},
    {BW8,2.94,2.75},{BW12,2.88,2.72},
    {PD50,6.92,6.71},{PD90,6.89,6.67},{PD120,6.92,6.70},{PD120W,6.90,6.68},{PD160,6.91,6.69},{PD180,6.93,6.70},{PD240,6.93,6.70},{PD290,6.91,6.68},
    {MP73,4.60,4.13},{MP115,4.62,4.14},{MP140,4.63,4.15},{MP175,4.63,4.15},
    {MR73,4.39,3.94},{MR90,4.39,3.94},{MR115,4.40,3.94},{MR140,4.42,3.95},{MR175,4.42,3.96},
    {ML180,4.40,3.95},{ML240,4.41,3.95},{ML280,4.42,3.96},{ML320,4.42,3.97},
    {JB60,7.00,6.75},{FAX480,3.21,2.92}
  };
  end=4.0;
  corrected=3.7;
  for(unsigned int i=0;i<sizeof(table)/sizeof(table[0]);i++)
    {
      if(table[i].mode==mode)
        {
          end=table[i].end;
          corrected=table[i].corrected;
          return;
        }
    }
}
