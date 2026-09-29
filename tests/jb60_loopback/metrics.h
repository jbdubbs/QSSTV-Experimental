// Image-quality metrics and test-target generators for the mode loopback test.
// Header only, no Qt widgets: works on QImage (Format_RGB32) and plain vectors.
#pragma once
#include <QImage>
#include <QRgb>
#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace metrics
{
  struct Rect { int x0,y0,x1,y1; };   // [x0,x1) x [y0,y1)

  inline std::vector<double> luma(const QImage &im)
  {
    std::vector<double> y((size_t)im.width()*im.height());
    for(int r=0;r<im.height();r++)
      {
        const QRgb *p=(const QRgb*)im.constScanLine(r);
        for(int c=0;c<im.width();c++) y[(size_t)r*im.width()+c]=0.299*qRed(p[c])+0.587*qGreen(p[c])+0.114*qBlue(p[c]);
      }
    return y;
  }

  inline double psnrFromMse(double mse) { return mse<=0 ? 99. : 10*log10(255.*255./mse); }

  // channel: 0..2 = R,G,B, 3 = luma, 4 = all channels
  inline double psnr(const QImage &a,const QImage &b,int channel)
  {
    double se=0; long n=0;
    if(channel==3)
      {
        std::vector<double> ya=luma(a),yb=luma(b);
        for(size_t i=0;i<ya.size();i++) { double d=ya[i]-yb[i]; se+=d*d; }
        return psnrFromMse(se/ya.size());
      }
    for(int r=0;r<a.height();r++)
      {
        const QRgb *pa=(const QRgb*)a.constScanLine(r),*pb=(const QRgb*)b.constScanLine(r);
        for(int c=0;c<a.width();c++)
          {
            int ca[3]={qRed(pa[c]),qGreen(pa[c]),qBlue(pa[c])},cb[3]={qRed(pb[c]),qGreen(pb[c]),qBlue(pb[c])};
            for(int k=0;k<3;k++)
              {
                if(channel<3 && k!=channel) continue;
                double d=ca[k]-cb[k]; se+=d*d; n++;
              }
          }
      }
    return psnrFromMse(se/n);
  }

  // Mean SSIM of the luma over a rectangle: 7x7 uniform window, windows fully inside the rectangle.
  inline double ssim(const QImage &a,const QImage &b,Rect rc)
  {
    const int W=a.width(),H=a.height(),win=7;
    std::vector<double> ya=luma(a),yb=luma(b);
    // summed area tables of a, b, a*a, b*b, a*b
    const int SW=W+1;
    std::vector<double> sa((size_t)SW*(H+1),0),sb=sa,saa=sa,sbb=sa,sab=sa;
    for(int r=0;r<H;r++)
      for(int c=0;c<W;c++)
        {
          double va=ya[(size_t)r*W+c],vb=yb[(size_t)r*W+c];
          size_t i=(size_t)(r+1)*SW+(c+1),u=(size_t)r*SW+(c+1),l=(size_t)(r+1)*SW+c,ul=(size_t)r*SW+c;
          sa[i]=va+sa[u]+sa[l]-sa[ul];
          sb[i]=vb+sb[u]+sb[l]-sb[ul];
          saa[i]=va*va+saa[u]+saa[l]-saa[ul];
          sbb[i]=vb*vb+sbb[u]+sbb[l]-sbb[ul];
          sab[i]=va*vb+sab[u]+sab[l]-sab[ul];
        }
    auto box=[&](const std::vector<double> &t,int x,int y)
    {
      return t[(size_t)(y+win)*SW+(x+win)]-t[(size_t)y*SW+(x+win)]-t[(size_t)(y+win)*SW+x]+t[(size_t)y*SW+x];
    };
    const double C1=(0.01*255)*(0.01*255),C2=(0.03*255)*(0.03*255),N=win*win;
    double sum=0; long cnt=0;
    for(int y=rc.y0;y+win<=rc.y1;y++)
      for(int x=rc.x0;x+win<=rc.x1;x++)
        {
          double ma=box(sa,x,y)/N,mb=box(sb,x,y)/N;
          double va=box(saa,x,y)/N-ma*ma,vb=box(sbb,x,y)/N-mb*mb,cab=box(sab,x,y)/N-ma*mb;
          sum+=((2*ma*mb+C1)*(2*cab+C2))/((ma*ma+mb*mb+C1)*(va+vb+C2));
          cnt++;
        }
    return cnt ? sum/cnt : 0;
  }

  inline double gradEnergy(const QImage &im,Rect rc)
  {
    std::vector<double> y=luma(im);
    const int W=im.width();
    double s=0; long n=0;
    for(int r=rc.y0;r+1<rc.y1;r++)
      for(int c=rc.x0;c+1<rc.x1;c++)
        {
          double gx=y[(size_t)r*W+c+1]-y[(size_t)r*W+c],gy=y[(size_t)(r+1)*W+c]-y[(size_t)r*W+c];
          s+=hypot(gx,gy); n++;
        }
    return n ? s/n : 0;
  }

  // 10-90% rise width (pixels) of a rising edge profile
  inline double rise1090(const std::vector<double> &p)
  {
    double lo=0,hi=0;
    for(int i=0;i<5;i++) { lo+=p[i]; hi+=p[p.size()-1-i]; }
    lo/=5; hi/=5;
    if(hi-lo<1) return -1;
    auto cross=[&](double t)
    {
      for(size_t i=0;i<p.size();i++)
        {
          double q=(p[i]-lo)/(hi-lo);
          if(q>=t)
            {
              if(i==0) return 0.;
              double qp=(p[i-1]-lo)/(hi-lo);
              return (double)(i-1)+(t-qp)/(q-qp);
            }
        }
      return (double)p.size();
    };
    return cross(0.9)-cross(0.1);
  }

  // ---------------------------------------------------------------- targets
  const int LO=30,HI=225;
  const int kPeriods[10]={64,32,16,12,8,6,5,4,3,2};

  inline QImage gray(int w,int h) { QImage im(w,h,QImage::Format_RGB32); im.fill(qRgb(128,128,128)); return im; }
  inline QRgb g(double v) { int i=(int)lround(v); i=i<0?0:(i>255?255:i); return qRgb(i,i,i); }

  inline QImage vEdge(int w,int h,int x0)   // transition along x, straight vertical edge
  {
    QImage im=gray(w,h);
    for(int y=0;y<h;y++) { QRgb *p=(QRgb*)im.scanLine(y); for(int x=0;x<w;x++) p[x]=g(x<x0 ? LO : HI); }
    return im;
  }
  inline QImage hEdge(int w,int h,int y0)   // transition along y, straight horizontal edge
  {
    QImage im=gray(w,h);
    for(int y=0;y<h;y++) { QRgb *p=(QRgb*)im.scanLine(y); for(int x=0;x<w;x++) p[x]=g(y<y0 ? LO : HI); }
    return im;
  }
  // Colour step along x: the two sides are matched in luma (~0.3 apart) but R and B are swapped between them,
  // so the step is invisible to the luma path and isolates the chroma (Cr/Cb) response the way vEdge isolates luma.
  // Measures the channel's raw chroma bandwidth, independent of any luma-guided TX/RX processing (which has
  // nothing to guide on here, since there's no coincident luma edge).
  inline QImage cEdge(int w,int h,int x0)
  {
    QImage im(w,h,QImage::Format_RGB32);
    const QRgb cA=qRgb(200,60,40),cB=qRgb(40,110,200);   // luma 0.299R+0.587G+0.114B: 99.58 vs 99.33
    for(int y=0;y<h;y++) { QRgb *p=(QRgb*)im.scanLine(y); for(int x=0;x<w;x++) p[x]=(x<x0) ? cA : cB; }
    return im;
  }
  // Mixed step along x: a real luma edge (dark grey -> bright orange, like vEdge's LO/HI) that ALSO carries a
  // colour change, the way a coloured object or text against a plain background usually does. Unlike cEdge,
  // luma-guided processing (TX downsample, RX upsample) has something to snap to here.
  inline QImage mEdge(int w,int h,int x0)
  {
    QImage im(w,h,QImage::Format_RGB32);
    const QRgb cA=qRgb(30,30,30),cB=qRgb(255,120,60);   // luma 30 -> 153.5, plus a strong colour change
    for(int y=0;y<h;y++) { QRgb *p=(QRgb*)im.scanLine(y); for(int x=0;x<w;x++) p[x]=(x<x0) ? cA : cB; }
    return im;
  }
  // kind 'h': vertical stripes (vary along x, 10 row bands), 'v': vary along y (10 column bands), 'd': diagonal
  inline QImage grating(int w,int h,char kind)
  {
    QImage im=gray(w,h);
    const double amp=60.;
    for(int y=0;y<h;y++)
      {
        QRgb *p=(QRgb*)im.scanLine(y);
        for(int x=0;x<w;x++)
          {
            int band=(kind=='v') ? std::min(x/(w/10),9) : std::min(y/(h/10),9);
            double per=kPeriods[band],ph;
            if(kind=='h') ph=x/per; else if(kind=='v') ph=y/per; else ph=(x+y)/(per*M_SQRT2);
            p[x]=g(128+amp*sin(2*M_PI*ph));
          }
      }
    return im;
  }

  // ---------------------------------------------------------------- measurements on the target images
  inline double edgeRiseV(const QImage &rx,int x0)
  {
    std::vector<double> y=luma(rx),prof(28,0.);
    const int W=rx.width(),H=rx.height();
    for(int r=20;r<H-20;r++) for(int i=0;i<28;i++) prof[i]+=y[(size_t)r*W+x0-14+i];
    for(double &v:prof) v/=(H-40);
    return rise1090(prof);
  }
  inline double edgeRiseH(const QImage &rx,int y0)
  {
    std::vector<double> y=luma(rx),prof(28,0.);
    const int W=rx.width(),H=rx.height();
    for(int c=20;c<W-20;c++) for(int i=0;i<28;i++) prof[i]+=y[(size_t)(y0-14+i)*W+c];
    for(double &v:prof) v/=(W-40);
    (void)H;
    return rise1090(prof);
  }
  // Same as edgeRiseV, but the profile is one RGB channel (0=R,1=G,2=B) instead of luma: for reading the
  // rise of a cEdge() step, whose two sides are luma-matched so the luma-only edgeRiseV can't see it.
  inline double edgeRiseChannel(const QImage &rx,int x0,int channel)
  {
    std::vector<double> prof(28,0.);
    const int H=rx.height();
    for(int r=20;r<H-20;r++)
      {
        const QRgb *p=(const QRgb*)rx.constScanLine(r);
        for(int i=0;i<28;i++)
          {
            QRgb v=p[x0-14+i];
            prof[i]+=(channel==0) ? qRed(v) : (channel==1) ? qGreen(v) : qBlue(v);
          }
      }
    for(double &v:prof) v/=(H-40);
    return rise1090(prof);
  }
  // Absolute x position (sub-pixel) where a cEdge()/vEdge()-style step's channel value crosses `frac` of
  // the way from its low plateau to its high plateau -- same windowed profile as edgeRiseChannel(), but a
  // position, not a width, so two runs (e.g. a TX-side change on vs off) can be compared for a group-delay
  // shift on a real transmitted edge. See the loopback README's chroma pre-emphasis section (TX idea 6) --
  // calibrateDelay() alone can't see a TX-side change since its synthetic step bypasses the mode's TX code.
  inline double edgeCrossChannel(const QImage &rx,int x0,int channel,double frac=0.5)
  {
    std::vector<double> prof(28,0.);
    const int H=rx.height();
    for(int r=20;r<H-20;r++)
      {
        const QRgb *p=(const QRgb*)rx.constScanLine(r);
        for(int i=0;i<28;i++)
          {
            QRgb v=p[x0-14+i];
            prof[i]+=(channel==0) ? qRed(v) : (channel==1) ? qGreen(v) : qBlue(v);
          }
      }
    for(double &v:prof) v/=(H-40);
    double lo=0,hi=0;
    for(int i=0;i<5;i++) { lo+=prof[i]; hi+=prof[prof.size()-1-i]; }
    lo/=5; hi/=5;
    if(hi-lo<1) return -1;
    for(size_t i=0;i<prof.size();i++)
      {
        double q=(prof[i]-lo)/(hi-lo);
        if(q>=frac)
          {
            if(i==0) return (double)(x0-14);
            double qp=(prof[i-1]-lo)/(hi-lo);
            return (double)(x0-14)+(double)(i-1)+(frac-qp)/(q-qp);
          }
      }
    return (double)(x0-14+(int)prof.size());
  }

  // amplitude ratio (output/input) of the fundamental in each of the 10 bands
  inline std::vector<double> mtf(const QImage &rx,char kind)
  {
    std::vector<double> y=luma(rx),out;
    const int W=rx.width(),H=rx.height();
    for(int b=0;b<10;b++)
      {
        const double per=kPeriods[b];
        std::complex<double> acc(0,0); long n=0;
        int x0=40,x1=W-40,y0=40,y1=H-40;
        if(kind=='v') { x0=b*(W/10)+8; x1=(b+1)*(W/10)-8; }
        else { y0=b*(H/10)+8; y1=(b+1)*(H/10)-8; }
        if(kind=='h') x1=x0+(int)((x1-x0)/kPeriods[b])*kPeriods[b];
        if(kind=='v') y1=y0+(int)((y1-y0)/kPeriods[b])*kPeriods[b];
        for(int r=y0;r<y1;r++)
          for(int c=x0;c<x1;c++)
            {
              double ph;
              if(kind=='h') ph=c/per; else if(kind=='v') ph=r/per; else ph=(c+r)/(per*M_SQRT2);
              acc+=y[(size_t)r*W+c]*std::polar(1.,-2*M_PI*ph); n++;
            }
        out.push_back(std::abs(acc)/n*2/60.);
      }
    return out;
  }
}
