// Line match test: the rule that decides whether two sync pulses are one or more lines apart (issue #128).
//
// Two pulses less than half a line apart used to round to 0 lines with a tiny fraction and were accepted as
// consecutive lines. The chain entry then had a line count of 0 and syncProcessor::calcSyncQuality divided by it
// (SIGFPE on the RX thread). Only the modes with a line time of more than 100 times the sync width can get there:
// the second pulse must be at least 0.8 times the sync width and still end within 0.8% of a line.
#include "linematch.h"
#include <cstdio>
#include <cmath>

static int fails=0;

static void check(const char *name,bool ok)
{
  printf("%-62s %s\n",name,ok? "ok":"FAIL");
  if(!ok) fails++;
}

struct sMode
{
  const char *name;
  double imageTime;     // s
  int lines;            // data lines
  double sync;          // s
};

// from SSTVTable (src/sstv/sstvparam.cpp)
static const sMode modes[]=
{
  {"SC2-180",182.03850,256,0.005},
  {"MP175",175.3730,128,0.009},
  {"MP140",139.5280,128,0.009},
  {"Scottie DX",268.89380,256,0.009},
  {"Martin 1",114.29700,256,0.005},     // too short a line to be affected, must keep working too
};

static const double tolerance=0.008;    // LINETOLERANCEMODEDETECT and LINETOLERANCEINSYNC
static const double clockHz=12000.;     // the sync detector runs at 12 kHz

static bool match(double delta,double spl,uint16_t &n,double &fract)
{
  return lineMatch(delta,spl,tolerance,n,fract);
}

int main()
{
  uint16_t n;
  double fract;

  // the pair in the core dump: SC2-180, second pulse ends 66 samples after the first (limit 68.3)
  {
    double spl=modes[0].imageTime/modes[0].lines*clockHz;
    bool m=match(66,spl,n,fract);
    check("SC2-180: pulses 66 samples apart are not a line",!m);
    check("SC2-180: ... they round to 0 lines with a tiny fraction",n==0 && fract<tolerance);
  }

  for(const sMode &md:modes)
    {
      char name[96];
      double spl=md.imageTime/md.lines*clockHz;

      // anything under half a line apart is one pulse seen twice
      bool zero=false;
      for(double d=0;d<spl/2;d+=1.) if(match(d,spl,n,fract)) zero=true;
      snprintf(name,sizeof(name),"%s: no match for pulses less than half a line apart",md.name);
      check(name,!zero);

      // a match never has 0 lines, whatever the distance
      bool anyZero=false;
      for(double d=0;d<8*spl;d+=0.5) if(match(d,spl,n,fract) && n==0) anyZero=true;
      snprintf(name,sizeof(name),"%s: a match always has at least 1 line",md.name);
      check(name,!anyZero);

      // real lines are still found, up to the tolerance
      bool lines=true,tooFar=true;
      for(int k=1;k<=8;k++)
        {
          if(!match(k*spl,spl,n,fract) || n!=k) lines=false;
          if(!match(k*spl+0.5*tolerance*spl,spl,n,fract) || n!=k) lines=false;
          if(!match(k*spl-0.5*tolerance*spl,spl,n,fract) || n!=k) lines=false;
          if(match(k*spl+1.5*tolerance*spl,spl,n,fract)) tooFar=false;
          if(match(k*spl-1.5*tolerance*spl,spl,n,fract)) tooFar=false;
        }
      snprintf(name,sizeof(name),"%s: 1 to 8 lines apart match",md.name);
      check(name,lines);
      snprintf(name,sizeof(name),"%s: more than 0.8%% off a line does not match",md.name);
      check(name,tooFar);

      // the fraction alone (the old rule) is under the tolerance for a short distance, so it cannot tell it from a line
      match(0.4*tolerance*spl,spl,n,fract);
      snprintf(name,sizeof(name),"%s: a 0-line pair has a fraction under the tolerance",md.name);
      check(name,n==0 && fract<tolerance);
    }

  printf("%s\n",fails? "FAILED":"all ok");
  return fails? 1:0;
}
