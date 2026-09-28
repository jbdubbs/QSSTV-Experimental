#include "modes/modes.h"
#include "appglobal.h"
#include "synthes.h"
#include "rxwidget.h"
#include <QGuiApplication>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <random>

double rxClock=12000, txClock=12000;
synthesizer *synthesPtr=nullptr;
dispatcher *dispatcherPtr=nullptr;
rxWidget *rxWidgetPtr=nullptr;
logFile *logFilePtr=nullptr;
void logFile::addToAux(QString){}
void logFile::add(const char*,const char*,int,QString,short unsigned int){}

static QImage makeImage(int w,int h,int kind)
{
  QImage im(w,h,QImage::Format_RGB32);
  for(int y=0;y<h;y++) for(int x=0;x<w;x++)
  {
    double u=(double)x/w, v=(double)y/h;
    int r,g,b;
    if(kind==0){ // smooth colour ramps with soft blobs
      r=(int)(255*u); g=(int)(255*v); b=(int)(127+127*sin(6.28*(u+v)));
      double d=hypot(x-320,y-248); if(d<120){ r=200; g=60; b=40+d; }
    } else if(kind==1){ // hard-edged: colour bars, diagonals, circles, fine stripes
      static const int bars[8][3]={{255,255,255},{255,255,0},{0,255,255},{0,255,0},{255,0,255},{255,0,0},{0,0,255},{0,0,0}};
      if(y<120){int i=x*8/w; r=bars[i][0];g=bars[i][1];b=bars[i][2];}
      else if(y<240){ int t=((x+y)/8)&1; r=g=b=t?230:25; }
      else if(y<360){ int t=(x/2)&1; r=g=b=t?200:50; if(x>320){ r=t?255:0; g=0; b=t?0:255; } }
      else { double d=hypot(x-320,y-430); bool in=d<50; r=in?250:30; g=in?200:60; b=in?30:220; if(((x/20)+(y/20))&1){r/=2;} }
    } else { // noisy-looking texture
      std::mt19937 rng(x*7919+y*104729);
      int n=rng()%64; r=(int)(128+100*sin(x*0.05)*cos(y*0.04))+n-32; g=(int)(128+100*cos(x*0.03+y*0.02))+n-32; b=(int)(128+100*sin(y*0.06))+n-32;
      r=std::max(0,std::min(255,r)); g=std::max(0,std::min(255,g)); b=std::max(0,std::min(255,b));
    }
    im.setPixel(x,y,qRgb(r,g,b));
  }
  return im;
}

int main(int argc,char**argv)
{
  esstvMode m=(argc>1 && !strcmp(argv[1],"pd"))?PD120:JB60;
  int kind=argc>2?atoi(argv[2]):0;
  double noiseHz=argc>3?atof(argv[3]):0;   // gaussian noise sigma on demodulated frequency
  const char *outPath=argc>4?argv[4]:nullptr;
  int decim=argc>5?atoi(argv[5]):1;        // TX at 12000*decim, RX picks every decim-th sample
  double clkErr=argc>6?atof(argv[6]):0;    // RX clock error (fraction)

  QGuiApplication app(argc,argv);
  initializeSSTVParametersIndex(m,true);
  initializeSSTVParametersIndex(m,false);
  unsigned W=txSSTVParam.numberOfPixels,H=txSSTVParam.numberOfDisplayLines;

  imageViewer txIv; txIv.img=makeImage(W,H,kind);
  imageViewer rxIv; rxIv.img=QImage(W,H,QImage::Format_RGB32); rxIv.img.fill(qRgb(128,128,128));
  rxWidget rw; rw.ivp=&rxIv; rxWidgetPtr=&rw;
  synthesizer syn; synthesPtr=&syn;

  double txc=12000.*decim;
  modeBase *tx=(m==JB60)?(modeBase*)new modeJB60(m,1024,true,false):(modeBase*)new modePD(m,1024,true,false);
  tx->init(txc);
  tx->transmitImage(&txIv);
  size_t n=syn.out.size();
  printf("mode %s: %zu tx samples @%.0f = %.2f s (table imageTime %.2f)\n",txSSTVParam.name.toLatin1().data(),n,txc,n/txc,txSSTVParam.imageTime);

  // channel: decimate, add noise
  std::vector<quint16> demod;
  std::mt19937 rng(1); std::normal_distribution<double> nd(0,1);
  for(size_t i=0;i<n;i+=decim){
    double f=syn.out[i];
    if(decim>1){ double s=0; int c=0; for(int k=0;k<decim && i+k<n;k++){s+=syn.out[i+k];c++;} f=s/c; }
    f+=noiseHz*nd(rng);
    demod.push_back((quint16)std::max(0.,std::min(65535.,f)));
  }
  for(int i=0;i<24000;i++) demod.push_back(1500);   // tail

  modeBase *rx=(m==JB60)?(modeBase*)new modeJB60(m,demod.size(),false,false):(modeBase*)new modePD(m,demod.size(),false,false);
  rx->init(12000.*(1+clkErr));
  rx->setRxSampleCounter(0);
  modeBase::eModeBase r=rx->process(demod.data(),0,false,0);
  printf("rx result %d (1=end of image), lines received %d / %d\n",(int)r,rx->receivedLines(),rx->imageLines());

  // metrics
  double se[3]={0,0,0},sa[3]={0,0,0}; double sey=0; long cnt=0; double maxe=0;
  for(unsigned y=0;y<H;y++){
    const QRgb *a=(const QRgb*)txIv.img.constScanLine(y),*b=(const QRgb*)rxIv.img.constScanLine(y);
    for(unsigned x=0;x<W;x++){
      int ca[3]={qRed(a[x]),qGreen(a[x]),qBlue(a[x])},cb[3]={qRed(b[x]),qGreen(b[x]),qBlue(b[x])};
      for(int c=0;c<3;c++){double d=ca[c]-cb[c]; se[c]+=d*d; sa[c]+=fabs(d); maxe=std::max(maxe,fabs(d));}
      double ya=.299*ca[0]+.587*ca[1]+.114*ca[2], yb=.299*cb[0]+.587*cb[1]+.114*cb[2]; sey+=(ya-yb)*(ya-yb);
      cnt++;
    }
  }
  const char*nm[3]={"R","G","B"};
  for(int c=0;c<3;c++) printf("  %s: PSNR %.2f dB  MAE %.2f\n",nm[c],10*log10(255.*255./(se[c]/cnt)),sa[c]/cnt);
  printf("  luma PSNR %.2f dB  overall PSNR %.2f dB  max err %.0f\n",10*log10(255.*255./(sey/cnt)),10*log10(255.*255./((se[0]+se[1]+se[2])/(3*cnt))),maxe);
  if(outPath){ txIv.img.save(QString(outPath)+"_src.png"); rxIv.img.save(QString(outPath)+"_rx.png"); }
  return 0;
}
