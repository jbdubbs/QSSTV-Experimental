#include "slantfit.h"
#include <algorithm>
#include <cmath>

#define LINEHALFWINDOW 8      // pixels either side of the brightest pixel that take part in the centroid
#define MINFITROWS 12

static double median(std::vector<double> v)
{
  if(v.empty()) return 0;
  std::nth_element(v.begin(),v.begin()+v.size()/2,v.end());
  return v[v.size()/2];
}

static void leastSquares(const std::vector<double> &r,const std::vector<double> &x,const std::vector<char> &in,
                         double &slope,double &icpt,int &n)
{
  double sr=0,sx=0,srr=0,srx=0;
  n=0;
  for(size_t i=0;i<r.size();i++)
    {
      if(!in[i]) continue;
      sr+=r[i];sx+=x[i];srr+=r[i]*r[i];srx+=r[i]*x[i];n++;
    }
  double den=n*srr-sr*sr;
  if(n<2 || std::fabs(den)<1e-9) {slope=0;icpt=(n>0)? sx/n:0;return;}
  slope=(n*srx-sr*sx)/den;
  icpt=(sx-slope*sr)/n;
}

slantFitResult fitSlantPoints(const std::vector<double> &rows,const std::vector<double> &x,int totalRows)
{
  slantFitResult res;
  res.total=totalRows;
  res.found=(int)rows.size();
  if((int)rows.size()<MINFITROWS) return res;
  const size_t n=rows.size();

  // robust start: median of the slopes between rows a quarter of the list apart, then the median offset
  std::vector<double> sl;
  size_t d=std::max<size_t>(1,n/4);
  for(size_t i=0;i+d<n;i++)
    if(rows[i+d]!=rows[i]) sl.push_back((x[i+d]-x[i])/(rows[i+d]-rows[i]));
  double slope=median(sl);
  std::vector<double> off(n);
  for(size_t i=0;i<n;i++) off[i]=x[i]-slope*rows[i];
  double icpt=median(off);

  std::vector<char> in(n,1);
  double thr=3.0;
  double rms=0;
  for(int pass=0;pass<8;pass++)
    {
      for(size_t i=0;i<n;i++) in[i]=(std::fabs(x[i]-(icpt+slope*rows[i]))<=thr);
      int used;
      leastSquares(rows,x,in,slope,icpt,used);
      if(used<MINFITROWS) return res;
      double s2=0;
      for(size_t i=0;i<n;i++) if(in[i]) {double e=x[i]-(icpt+slope*rows[i]);s2+=e*e;}
      rms=std::sqrt(s2/used);
      thr=std::max(0.75,3.0*rms);
    }
  int used=0;
  for(size_t i=0;i<n;i++)
    {
      in[i]=(std::fabs(x[i]-(icpt+slope*rows[i]))<=thr);
      if(in[i]) used++;
    }
  if(used<MINFITROWS) return res;
  leastSquares(rows,x,in,slope,icpt,used);
  double s2=0;
  for(size_t i=0;i<n;i++) if(in[i]) {double e=x[i]-(icpt+slope*rows[i]);s2+=e*e;}
  res.valid=true;
  res.slope=slope;
  res.intercept=icpt;
  res.rms=std::sqrt(s2/used);
  res.used=used;
  return res;
}

slantFitResult fitSlant(const unsigned char *gray,int width,int height,int stride,int minContrast)
{
  std::vector<double> rows,xs;
  std::vector<unsigned char> sorted;
  const int edge=width/20;
  for(int y=0;y<height;y++)
    {
      const unsigned char *p=gray+(size_t)y*stride;
      int hi=0,peak=0;
      // neither outer edge is searched: the first and last columns often carry the end of the neighbouring line or
      // sync pulse (slightly off audio or sync timing), often brighter than the line, and the line is never there
      for(int i=edge;i<width-edge;i++) if(p[i]>hi) {hi=p[i];peak=i;}
      // dark level: a low percentile of the row, so a few noisy pixels do not set it
      sorted.assign(p,p+width);
      std::nth_element(sorted.begin(),sorted.begin()+width/10,sorted.end());
      int lo=sorted[width/10];
      if(hi-lo<minContrast) continue;
      double thr=lo+0.5*(hi-lo);
      double sw=0,swx=0;
      int a=std::max(0,peak-LINEHALFWINDOW),b=std::min(width-1,peak+LINEHALFWINDOW);
      for(int i=a;i<=b;i++)
        {
          double w=p[i]-thr;
          if(w>0) {sw+=w;swx+=w*i;}
        }
      if(sw<=0) continue;
      rows.push_back(y);
      xs.push_back(swx/sw);
    }
  return fitSlantPoints(rows,xs,height);
}
