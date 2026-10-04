#include "linefit.h"
#include <cmath>
#include <map>
#include <algorithm>

#define MAXITERATIONS 6
#define MINPOINTS 3

namespace
{
struct groupStat
{
  double n=0,mx=0,my=0;
};

// slope with per group intercepts: centre x and y on each group's mean, then ordinary regression
bool solve(const std::vector<linePoint> &pts,const std::vector<bool> &use,double &slope,double &sxx,
           std::map<int,groupStat> &g)
{
  g.clear();
  for(size_t i=0;i<pts.size();i++)
    if(use[i]) {groupStat &s=g[pts[i].group];s.n++;s.mx+=pts[i].x;s.my+=pts[i].y;}
  for(auto &kv:g) {kv.second.mx/=kv.second.n;kv.second.my/=kv.second.n;}
  double sxy=0;
  sxx=0;
  for(size_t i=0;i<pts.size();i++)
    if(use[i])
      {
        const groupStat &s=g[pts[i].group];
        double dx=pts[i].x-s.mx;
        sxx+=dx*dx;
        sxy+=dx*(pts[i].y-s.my);
      }
  if(sxx<=0) return false;
  slope=sxy/sxx;
  return true;
}
}

lineFitResult fitLine(const std::vector<linePoint> &pts,double clipSigma,double minClip,
                      std::vector<double> *residuals,std::vector<bool> *inlierOut)
{
  lineFitResult r;
  const size_t n=pts.size();
  std::vector<bool> use(n,true);
  std::map<int,groupStat> g;
  double slope=0,sxx=0;
  int used=int(n);
  for(int it=0;it<MAXITERATIONS;it++)
    {
      if(used<MINPOINTS || !solve(pts,use,slope,sxx,g)) return r;
      std::vector<double> res(n);
      double ss=0;
      for(size_t i=0;i<n;i++)
        {
          const groupStat &s=g.count(pts[i].group)? g[pts[i].group]:groupStat();
          res[i]=pts[i].y-(s.my+slope*(pts[i].x-s.mx));
          if(use[i]) ss+=res[i]*res[i];
        }
      double dof=std::max(1.0,double(used)-double(g.size())-1.0);
      double rms=std::sqrt(ss/dof);
      // the clip limit comes from the median absolute residual: a few wild points must not inflate it (as they
      // would the rms) and so protect themselves
      std::vector<double> absRes;
      for(size_t i=0;i<n;i++) if(use[i]) absRes.push_back(std::fabs(res[i]));
      std::nth_element(absRes.begin(),absRes.begin()+absRes.size()/2,absRes.end());
      double scale=1.4826*absRes[absRes.size()/2];
      double limit=std::max(clipSigma*scale,minClip);
      bool changed=false;
      int count=0;
      std::vector<bool> next(n);
      for(size_t i=0;i<n;i++)
        {
          next[i]=std::fabs(res[i])<=limit;
          if(next[i]!=use[i]) changed=true;
          if(next[i]) count++;
        }
      r.slope=slope;
      r.rms=rms;
      r.slopeError=rms/std::sqrt(sxx);
      r.inliers=used;
      if(residuals) *residuals=res;
      if(!changed || count<MINPOINTS) break;
      use=next;
      used=count;
    }
  if(!solve(pts,use,slope,sxx,g)) return r;
  double mn=1e300,mx=-1e300;
  double ss=0;
  used=0;
  for(size_t i=0;i<n;i++)
    if(use[i])
      {
        const groupStat &s=g[pts[i].group];
        double e=pts[i].y-(s.my+slope*(pts[i].x-s.mx));
        ss+=e*e;
        used++;
        mn=std::min(mn,pts[i].x);
        mx=std::max(mx,pts[i].x);
      }
  if(used<MINPOINTS) return r;
  double dof=std::max(1.0,double(used)-double(g.size())-1.0);
  r.valid=true;
  r.slope=slope;
  r.rms=std::sqrt(ss/dof);
  r.slopeError=r.rms/std::sqrt(sxx);
  r.inliers=used;
  r.span=mx-mn;
  if(residuals)
    {
      residuals->resize(n);
      for(size_t i=0;i<n;i++)
        {
          auto f=g.find(pts[i].group);
          double my=(f!=g.end())? f->second.my:0,mxg=(f!=g.end())? f->second.mx:0;
          (*residuals)[i]=pts[i].y-(my+slope*(pts[i].x-mxg));
        }
    }
  if(inlierOut) *inlierOut=use;
  return r;
}
