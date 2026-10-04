// Slant fit test: synthetic pictures with a 3 pixel wide line of known slope, with noise and missing rows.
#include "slantfit.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <vector>

static int fails=0;

static void check(const char *name,bool ok)
{
  printf("%-50s %s\n",name,ok? "ok":"FAIL");
  if(!ok) fails++;
}

static std::vector<unsigned char> make(int w,int h,double x0,double slope,int noise,int missingEvery,int firstRow=0,int lastRow=-1)
{
  std::vector<unsigned char> img((size_t)w*h,10);
  if(lastRow<0) lastRow=h-1;
  srand(1);
  for(int y=firstRow;y<=lastRow;y++)
    {
      if(missingEvery && (y%missingEvery)==0) continue;
      double c=x0+slope*y;
      for(int x=0;x<w;x++)
        {
          // line 3 pixels wide, box filtered at sub-pixel positions
          double l=std::max(c-1.5,(double)x-0.5),r=std::min(c+1.5,(double)x+0.5);
          double cover=std::max(0.0,r-l);
          int v=10+(int)(cover*235.0);
          if(noise) v+=(rand()%(2*noise+1))-noise;
          img[(size_t)y*w+x]=(unsigned char)std::max(0,std::min(255,v));
        }
    }
  return img;
}

int main()
{
  const int W=640,H=496;
  for(double s : {0.0,0.0123,-0.2,0.5})
    {
      auto img=make(W,H,300.3,s,8,0);
      slantFitResult r=fitSlant(img.data(),W,H,W);
      char nm[80];
      snprintf(nm,sizeof nm,"slope %+.4f px/row (got %+.5f, rms %.3f)",s,r.slope,r.rms);
      check(nm,r.valid && std::fabs(r.slope-s)<2e-4 && r.used>H*0.9);
    }
  {
    auto img=make(W,H,100.0,0.37,8,7);
    slantFitResult r=fitSlant(img.data(),W,H,W);
    check("every 7th row missing",r.valid && std::fabs(r.slope-0.37)<3e-4);
  }
  {
    // only the first 60% of the picture received
    auto img=make(W,H,100.0,0.2,8,0,0,297);
    slantFitResult r=fitSlant(img.data(),W,H,W);
    check("partial picture",r.valid && std::fabs(r.slope-0.2)<5e-4 && r.found<=298);
  }
  {
    // slants out of the picture: only rows where it is inside count
    auto img=make(W,H,20.0,1.2,8,0);
    slantFitResult r=fitSlant(img.data(),W,H,W);
    check("slants out of the picture",r.valid && std::fabs(r.slope-1.2)<2e-3);
  }
  {
    std::vector<unsigned char> blank((size_t)W*H,40);
    slantFitResult r=fitSlant(blank.data(),W,H,W);
    check("blank picture is not valid",!r.valid);
  }
  printf(fails? "%d FAILED\n":"all passed\n",fails);
  return fails? 1:0;
}
