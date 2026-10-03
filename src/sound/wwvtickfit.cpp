#include "wwvtickfit.h"
#include <cmath>
#include <algorithm>

#define MAXPOINTS 4000         // oldest candidates are dropped beyond this
#define ACQUIRESECONDS 40      // acquisition looks at the most recent candidates only
#define ACQUIREMINTICKS 8      // ticks on a line before it is believed
#define ACQUIREMAXPPM 1000     // search range for the sample rate error
#define ACQUIREPPMSTEP 25
#define ACQUIRETOLERANCE 0.0025 // seconds
#define MINTICKS 5             // fewer ticks on the line than this and the lock is dropped
#define MINTOLERANCE 0.0005    // seconds, lower limit of the inlier window while refining
#define MAXTOLERANCE 0.005     // seconds, upper limit
#define VALIDTICKS 8

wwvTickFit::wwvTickFit(double nominalRate) : nominal(nominalRate)
{
  reset();
}

void wwvTickFit::reset()
{
  pts.clear();
  haveLine=false;
  lineA=0;
  lineB=nominal;
  res.valid=false;
  res.rate=nominal;
  res.ppm=0;
  res.ppmError=0;
  res.rmsMs=0;
  res.ticks=0;
  res.spanSeconds=0;
}

void wwvTickFit::add(const wwvCandidate &c)
{
  wwvFitPoint p;
  p.position=c.position;
  p.strength=c.strength;
  p.k=0;
  p.inlier=false;
  pts.push_back(p);
  if(pts.size()>MAXPOINTS) pts.erase(pts.begin(),pts.begin()+(pts.size()-MAXPOINTS));
}

bool wwvTickFit::solve()
{
  if(!haveLine)
    {
      if(!acquire()) return false;
      haveLine=true;
      if(!refine(ACQUIRETOLERANCE*nominal)) {haveLine=false; return false;}
      return res.valid;
    }
  if(refine(MAXTOLERANCE*nominal)) return res.valid;
  // lost the line (a different station, long fade): look for a new one
  haveLine=false;
  res.valid=false;
  return false;
}

/*
  Find a line without any prior knowledge: try every candidate of the last seconds as a tick and every sample
  rate error in a grid, and take the combination on which most seconds have a tick.
*/
bool wwvTickFit::acquire()
{
  if(pts.size()<ACQUIREMINTICKS) return false;
  double newest=pts.back().position;
  size_t first=0;
  while(first<pts.size() && pts[first].position<newest-ACQUIRESECONDS*nominal) first++;
  if(pts.size()-first<ACQUIREMINTICKS) return false;

  int bestScore=0;
  double bestStrength=0;
  double bestA=0;
  double bestB=nominal;
  const int span=2*ACQUIRESECONDS+3;
  std::vector<char> used(span);
  for(size_t i=first;i<pts.size();i++)
    {
      for(int ppm=-ACQUIREMAXPPM;ppm<=ACQUIREMAXPPM;ppm+=ACQUIREPPMSTEP)
        {
          double b=nominal*(1.0+ppm*1e-6);
          std::fill(used.begin(),used.end(),0);
          int score=0;
          for(size_t j=first;j<pts.size();j++)
            {
              double d=pts[j].position-pts[i].position;
              long long k=std::llround(d/b);
              if(k<-ACQUIRESECONDS-1 || k>ACQUIRESECONDS+1) continue;
              if(std::fabs(d-k*b)>ACQUIRETOLERANCE*nominal) continue;
              char &u=used[k+ACQUIRESECONDS+1];
              if(!u) {u=1;score++;}
            }
          if(score>bestScore || (score==bestScore && pts[i].strength>bestStrength))
            {
              bestScore=score;
              bestStrength=pts[i].strength;
              bestA=pts[i].position;
              bestB=b;
            }
        }
    }
  if(bestScore<ACQUIREMINTICKS) return false;
  lineA=bestA;
  lineB=bestB;
  return true;
}

/*
  Least squares fit of the candidates that are within `tolerance` of the current line. The window shrinks to
  a few times the scatter of the ticks found, so speech and other junk close to the line is thrown out.
*/
bool wwvTickFit::refine(double tolerance)
{
  const double minTol=MINTOLERANCE*nominal;
  const double maxTol=MAXTOLERANCE*nominal;
  double rms=0;
  double stdErr=0;
  int n=0;
  double firstPos=0,lastPos=0;
  for(int iter=0;iter<6;iter++)
    {
      // number the candidates and mark inliers (one per tick number, the strongest)
      for(size_t i=0;i<pts.size();i++)
        {
          wwvFitPoint &p=pts[i];
          p.k=std::llround((p.position-lineA)/lineB);
          p.inlier=std::fabs(p.position-(lineA+lineB*p.k))<=tolerance;
        }
      for(size_t i=0;i<pts.size();i++)
        {
          if(!pts[i].inlier) continue;
          // candidates are in time order, so equal tick numbers are neighbours among the inliers
          for(size_t j=i+1;j<pts.size() && pts[j].k<=pts[i].k;j++)
            {
              if(!pts[j].inlier || pts[j].k!=pts[i].k) continue;
              if(pts[j].strength>pts[i].strength) {pts[i].inlier=false;break;}
              pts[j].inlier=false;
            }
        }
      double sumK=0,sumY=0;
      n=0;
      for(size_t i=0;i<pts.size();i++)
        {
          if(!pts[i].inlier) continue;
          sumK+=pts[i].k;
          sumY+=pts[i].position;
          n++;
        }
      if(n<MINTICKS) return false;
      double meanK=sumK/n;
      double meanY=sumY/n;
      double sxx=0,sxy=0;
      for(size_t i=0;i<pts.size();i++)
        {
          if(!pts[i].inlier) continue;
          double dk=pts[i].k-meanK;
          sxx+=dk*dk;
          sxy+=dk*(pts[i].position-meanY);
        }
      if(sxx<=0) return false;
      lineB=sxy/sxx;
      lineA=meanY-lineB*meanK;
      double sse=0;
      firstPos=1e300;
      lastPos=-1e300;
      for(size_t i=0;i<pts.size();i++)
        {
          if(!pts[i].inlier) continue;
          double r=pts[i].position-(lineA+lineB*pts[i].k);
          sse+=r*r;
          firstPos=std::min(firstPos,pts[i].position);
          lastPos=std::max(lastPos,pts[i].position);
        }
      rms=std::sqrt(sse/n);
      stdErr=(n>2)? std::sqrt(sse/(n-2)/sxx):0.0;
      tolerance=std::max(minTol,std::min(maxTol,4.0*rms));
    }
  res.valid=(n>=VALIDTICKS);
  res.ticks=n;
  res.rate=lineB;
  res.ppm=(lineB/nominal-1.0)*1e6;
  res.ppmError=stdErr/nominal*1e6;
  res.rmsMs=1000.0*rms/nominal;
  res.spanSeconds=(lastPos-firstPos)/nominal;
  return true;
}
