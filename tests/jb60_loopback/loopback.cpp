// TX -> channel -> RX loopback for the QSSTV-engine modes (JB60, with PD120 as a control).
//
// The real modeJB60 / modePD classes transmit and receive. Between them, by default, sits the real receive
// front end: 16 bit audio -> dsp/downsamplefilter (48k -> 12k) -> dsp/filters videoFilter (the 181 tap complex
// FIR, atan2 discriminator). That filter is what limits horizontal sharpness, so a test that skips it
// (--ideal, the old behaviour) cannot show how a mode really looks.
//
// The app finds line timing with its sync detector, which is not run here. Instead the fixed delay of the video
// path is measured once by sending a step through the same chain and the demod track is shifted back by it.
#include "modes/modes.h"
#include "appglobal.h"
#include "synthes.h"
#include "rxwidget.h"
#include "downsamplefilter.h"
#include "filters.h"
#include "metrics.h"
#include <QGuiApplication>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <random>
#include <string>
#include <vector>

double rxClock=12000, txClock=12000;
synthesizer *synthesPtr=nullptr;
dispatcher *dispatcherPtr=nullptr;
rxWidget *rxWidgetPtr=nullptr;
logFile *logFilePtr=nullptr;
void logFile::addToAux(QString){}
void logFile::add(const char*,const char*,int,QString,short unsigned int){}

#ifndef IMAGE_DIR
#define IMAGE_DIR "images"
#endif

namespace
{
  struct Options
  {
    esstvMode mode=JB60;
    std::string image="0";
    bool ideal=false;      // old baseband path: no audio, no downsampler, no video filter
    bool ssb=false;        // 300-2700 Hz band limit
    bool hasSnr=false;
    double snr=0;          // dB in 2.7 kHz, white noise at the receiver input
    double noiseHz=0;      // --ideal only: gaussian noise on the demodulated frequency
    double clockErr=0;     // RX clock error (fraction)
    double tshift=0;       // extra timing shift (12 kHz samples)
    std::string out,wav;
    bool suite=false;
  };

  const int W=640,H=496;

  QImage makeImage(int kind)
  {
    QImage im(W,H,QImage::Format_RGB32);
    for(int y=0;y<H;y++) for(int x=0;x<W;x++)
    {
      double u=(double)x/W, v=(double)y/H;
      int r,g,b;
      if(kind==0){ // smooth colour ramps with soft blobs
        r=(int)(255*u); g=(int)(255*v); b=(int)(127+127*sin(6.28*(u+v)));
        double d=hypot(x-320,y-248); if(d<120){ r=200; g=60; b=40+d; }
      } else if(kind==1){ // hard-edged: colour bars, diagonals, circles, fine stripes
        static const int bars[8][3]={{255,255,255},{255,255,0},{0,255,255},{0,255,0},{255,0,255},{255,0,0},{0,0,255},{0,0,0}};
        if(y<120){int i=x*8/W; r=bars[i][0];g=bars[i][1];b=bars[i][2];}
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

  QImage loadImage(const std::string &name)
  {
    if(name=="0"||name=="1"||name=="2") return makeImage(atoi(name.c_str()));
    if(name=="vedge320") return metrics::vEdge(W,H,320);
    if(name=="vedge321") return metrics::vEdge(W,H,321);
    if(name=="hedge248") return metrics::hEdge(W,H,248);
    if(name=="hedge249") return metrics::hEdge(W,H,249);
    if(name=="grath") return metrics::grating(W,H,'h');
    if(name=="gratv") return metrics::grating(W,H,'v');
    if(name=="gratd") return metrics::grating(W,H,'d');
    QString path=(name=="card") ? QString(IMAGE_DIR)+"/card.png" : QString::fromStdString(name);
    QImage im(path);
    if(im.isNull()) { fprintf(stderr,"cannot load image %s\n",path.toLatin1().data()); exit(2); }
    im=im.convertToFormat(QImage::Format_RGB32);
    if(im.width()!=W||im.height()!=H) im=im.scaled(W,H,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    return im;
  }

  // ------------------------------------------------------------------ channel
  const double kAudioAmp=8000.;      // what synthesizer::nextSample produces

  std::vector<double> makeAudio(const std::vector<float> &f48)
  {
    std::vector<double> a(f48.size());
    double ph=0;
    for(size_t i=0;i<f48.size();i++)
      {
        ph+=2*M_PI*f48[i]/48000.;
        if(ph>2*M_PI) ph-=2*M_PI;
        a[i]=kAudioAmp*sin(ph);
      }
    return a;
  }

  void addNoise(std::vector<double> &x,double snrDb)
  {
    std::mt19937 rng(1); std::normal_distribution<double> nd(0,1);
    double ps=kAudioAmp*kAudioAmp/2;
    double pn=ps/pow(10,snrDb/10);                 // noise power in 2.7 kHz
    double sigma=sqrt(pn*(24000./2700.));          // white over 0..24 kHz
    for(double &v:x) v+=sigma*nd(rng);
  }

  struct Biquad { double b0,b1,b2,a1,a2; };
  Biquad rbj(bool highpass,double f,double q)
  {
    double w0=2*M_PI*f/48000.,c=cos(w0),al=sin(w0)/(2*q),a0=1+al;
    Biquad s;
    if(highpass) { s.b0=(1+c)/2/a0; s.b1=-(1+c)/a0; s.b2=(1+c)/2/a0; }
    else         { s.b0=(1-c)/2/a0; s.b1=(1-c)/a0;  s.b2=(1-c)/2/a0; }
    s.a1=-2*c/a0; s.a2=(1-al)/a0;
    return s;
  }
  void runBiquad(const Biquad &s,std::vector<double> &x)
  {
    double x1=0,x2=0,y1=0,y2=0;
    for(double &v:x) { double y=s.b0*v+s.b1*x1+s.b2*x2-s.a1*y1-s.a2*y2; x2=x1; x1=v; y2=y1; y1=y; v=y; }
  }
  // 6th order Butterworth high pass at 300 Hz and low pass at 2700 Hz, run forward and backward (zero phase)
  void bandLimit(std::vector<double> &x)
  {
    static const double q[3]={0.5176380902,0.7071067812,1.9318516526};
    for(int pass=0;pass<2;pass++)
      {
        for(int i=0;i<3;i++) { runBiquad(rbj(true,300.,q[i]),x); runBiquad(rbj(false,2700.,q[i]),x); }
        std::reverse(x.begin(),x.end());
      }
  }

  // audio (48 kHz) -> real downsampler -> real video filter -> 12 kHz demod track (Hz)
  std::vector<quint16> demodChain(const std::vector<double> &audio)
  {
    const unsigned block=DOWNSAMPLESIZE;
    downsampleFilter ds(block,true);
    videoFilter vf(RXSTRIPE);
    std::vector<quint16> out;
    std::vector<short> buf(block);
    for(size_t pos=0;pos<audio.size();pos+=block)
      {
        for(unsigned i=0;i<block;i++)
          {
            double v=(pos+i<audio.size()) ? audio[pos+i] : 0.;
            buf[i]=(short)std::max(-32768.,std::min(32767.,std::round(v)));
          }
        ds.downSample4(buf.data());
        vf.process(ds.filteredDataPtr());
        out.insert(out.end(),vf.demodPtr,vf.demodPtr+RXSTRIPE);
      }
    return out;
  }

  std::vector<float> withTail(std::vector<float> f48)
  {
    f48.insert(f48.end(),48000/2,1500.f);
    while(f48.size()%DOWNSAMPLESIZE) f48.push_back(1500.f);
    return f48;
  }

  // delay (12 kHz samples) between a frequency step entering the TX track and its 50% point in the demod track
  double calibrateDelay()
  {
    static double cached=-1;
    if(cached>=0) return cached;
    std::vector<float> f(14400,1500.f);
    f.insert(f.end(),14400,2300.f);
    f.insert(f.end(),9600,1500.f);
    std::vector<quint16> y=demodChain(makeAudio(withTail(f)));
    for(size_t n=1500;n<y.size();n++)
      if(y[n]>=1900 && y[n-1]<1900)
        {
          double frac=(1900.-y[n-1])/((double)y[n]-y[n-1]);
          cached=(n-1+frac)-3600.;
          return cached;
        }
    fprintf(stderr,"delay calibration failed\n"); exit(3);
  }

  // shift a demod track earlier by `delay` samples (linear interpolation)
  std::vector<quint16> alignTrack(const std::vector<quint16> &y,double delay)
  {
    std::vector<quint16> a(y.size());
    for(size_t k=0;k<y.size();k++)
      {
        double t=k+delay;
        size_t i=(size_t)floor(t);
        double fr=t-i;
        double v=(i+1<y.size()) ? y[i]*(1-fr)+y[i+1]*fr : y.back();
        a[k]=(quint16)std::max(0.,std::min(65535.,std::round(v)));
      }
    return a;
  }

  void writeWav(const std::string &path,const std::vector<double> &x)
  {
    FILE *f=fopen(path.c_str(),"wb"); if(!f) return;
    unsigned n=x.size(),rate=48000,bytes=n*2,brate=rate*2,riff=36+bytes;
    unsigned short fmt=1,ch=1,align=2,bits=16;
    unsigned fmtLen=16;
    fwrite("RIFF",1,4,f); fwrite(&riff,4,1,f); fwrite("WAVEfmt ",1,8,f); fwrite(&fmtLen,4,1,f);
    fwrite(&fmt,2,1,f); fwrite(&ch,2,1,f); fwrite(&rate,4,1,f); fwrite(&brate,4,1,f); fwrite(&align,2,1,f); fwrite(&bits,2,1,f);
    fwrite("data",1,4,f); fwrite(&bytes,4,1,f);
    for(double v:x) { short s=(short)std::max(-32768.,std::min(32767.,std::round(v))); fwrite(&s,2,1,f); }
    fclose(f);
  }

  // ------------------------------------------------------------------ one TX -> channel -> RX pass
  struct Result { QImage rx; double seconds; int rxResult; int lines,imageLines; double delay; };

  Result runChain(const Options &o,const QImage &src)
  {
    initializeSSTVParametersIndex(o.mode,true);
    initializeSSTVParametersIndex(o.mode,false);
    synthesizer syn; synthesPtr=&syn;
    imageViewer txIv; txIv.img=src;
    imageViewer rxIv; rxIv.img=QImage(W,H,QImage::Format_RGB32); rxIv.img.fill(qRgb(128,128,128));
    rxWidget rw; rw.ivp=&rxIv; rxWidgetPtr=&rw;

    const double txc=48000.;
    modeBase *tx=(o.mode==JB60)?(modeBase*)new modeJB60(o.mode,1024,true,false):(modeBase*)new modePD(o.mode,1024,true,false);
    tx->init(txc);
    tx->transmitImage(&txIv);
    std::vector<float> f48=syn.out;
    Result res; res.seconds=f48.size()/txc; res.delay=0;

    std::vector<quint16> demod;
    if(o.ideal)
      {
        std::mt19937 rng(1); std::normal_distribution<double> nd(0,1);
        for(size_t i=0;i+4<=f48.size();i+=4)
          {
            double f=(f48[i]+f48[i+1]+f48[i+2]+f48[i+3])/4+o.noiseHz*nd(rng);
            demod.push_back((quint16)std::max(0.,std::min(65535.,f)));
          }
      }
    else
      {
        std::vector<double> audio=makeAudio(withTail(f48));
        if(o.hasSnr) addNoise(audio,o.snr);
        if(o.ssb) bandLimit(audio);
        if(!o.wav.empty()) writeWav(o.wav,audio);
        res.delay=calibrateDelay()+o.tshift;
        demod=alignTrack(demodChain(audio),res.delay);
      }
    for(int i=0;i<24000;i++) demod.push_back(1500);   // tail

    modeBase *rx=(o.mode==JB60)?(modeBase*)new modeJB60(o.mode,demod.size(),false,false):(modeBase*)new modePD(o.mode,demod.size(),false,false);
    rx->init(12000.*(1+o.clockErr));
    rx->setRxSampleCounter(0);
    res.rxResult=(int)rx->process(demod.data(),0,false,0);
    res.lines=rx->receivedLines(); res.imageLines=rx->imageLines();
    res.rx=rxIv.img;
    delete tx; delete rx;
    return res;
  }

  metrics::Rect textRegion(const std::string &image)
  {
    if(image=="card") return {0,120,W,300};
    return {0,0,W,H};
  }

  void printMtf(const char *label,const std::vector<double> &m)
  {
    printf("  MTF %s (period px:",label);
    for(int p:metrics::kPeriods) printf(" %d",p);
    printf(")\n      ");
    for(double v:m) printf(" %.2f",v);
    printf("\n");
  }

  void report(const Options &o,const Result &r,const QImage &src)
  {
    const metrics::Rect full={0,0,W,H},txt=textRegion(o.image);
    printf("mode %s  image %s  channel %s%s%s\n",txSSTVParam.name.toLatin1().data(),o.image.c_str(),
           o.ideal?"ideal (baseband)":"real (downsampler+video FIR)",o.hasSnr?QString("  snr %1 dB").arg(o.snr).toLatin1().data():"",o.ssb?"  ssb":"");
    printf("  tx %.2f s (table %.2f)  video-path delay %.2f samples  rx result %d, lines %d/%d\n",r.seconds,txSSTVParam.imageTime,r.delay,r.rxResult,r.lines,r.imageLines);
    printf("  PSNR R %.2f G %.2f B %.2f  luma %.2f  all %.2f dB\n",metrics::psnr(src,r.rx,0),metrics::psnr(src,r.rx,1),metrics::psnr(src,r.rx,2),metrics::psnr(src,r.rx,3),metrics::psnr(src,r.rx,4));
    printf("  SSIM full %.3f  text-region %.3f   gradient kept: full %.2f text-region %.2f\n",metrics::ssim(src,r.rx,full),metrics::ssim(src,r.rx,txt),
           metrics::gradEnergy(r.rx,full)/metrics::gradEnergy(src,full),metrics::gradEnergy(r.rx,txt)/metrics::gradEnergy(src,txt));
    if(o.image=="vedge320") printf("  edge 10-90%% rise (x): %.2f px\n",metrics::edgeRiseV(r.rx,320));
    if(o.image=="vedge321") printf("  edge 10-90%% rise (x): %.2f px\n",metrics::edgeRiseV(r.rx,321));
    if(o.image=="hedge248") printf("  edge 10-90%% rise (y): %.2f px\n",metrics::edgeRiseH(r.rx,248));
    if(o.image=="hedge249") printf("  edge 10-90%% rise (y): %.2f px\n",metrics::edgeRiseH(r.rx,249));
    if(o.image=="grath") printMtf("horizontal (vertical stripes)",metrics::mtf(r.rx,'h'));
    if(o.image=="gratv") printMtf("vertical (horizontal stripes)",metrics::mtf(r.rx,'v'));
    if(o.image=="gratd") printMtf("diagonal",metrics::mtf(r.rx,'d'));
  }

  // fixed set of targets: the table in README.md
  void runSuite(Options o)
  {
    struct Row { const char *name; };
    double riseX=0,riseY=0;
    std::vector<double> mh,mv,md;
    double cardYpsnr=0,cardSsimText=0,cardSsimFull=0,cardGrad=0;
    {
      o.image="card"; QImage s=loadImage(o.image); Result r=runChain(o,s);
      cardYpsnr=metrics::psnr(s,r.rx,3); cardSsimText=metrics::ssim(s,r.rx,textRegion("card"));
      cardSsimFull=metrics::ssim(s,r.rx,{0,0,W,H});
      cardGrad=metrics::gradEnergy(r.rx,textRegion("card"))/metrics::gradEnergy(s,textRegion("card"));
    }
    for(const char *n:{"vedge320","vedge321"}) { o.image=n; QImage s=loadImage(o.image); riseX+=metrics::edgeRiseV(runChain(o,s).rx,atoi(n+5))/2; }
    for(const char *n:{"hedge248","hedge249"}) { o.image=n; QImage s=loadImage(o.image); riseY+=metrics::edgeRiseH(runChain(o,s).rx,atoi(n+5))/2; }
    { o.image="grath"; QImage s=loadImage(o.image); mh=metrics::mtf(runChain(o,s).rx,'h'); }
    { o.image="gratv"; QImage s=loadImage(o.image); mv=metrics::mtf(runChain(o,s).rx,'v'); }
    { o.image="gratd"; QImage s=loadImage(o.image); md=metrics::mtf(runChain(o,s).rx,'d'); }
    printf("suite  mode %s  channel %s%s%s\n",txSSTVParam.name.toLatin1().data(),o.ideal?"ideal":"real",o.hasSnr?QString("  snr %1 dB").arg(o.snr).toLatin1().data():"",o.ssb?"  ssb":"");
    printf("  card: luma PSNR %.2f dB  SSIM text %.3f full %.3f  text gradient kept %.2f\n",cardYpsnr,cardSsimText,cardSsimFull,cardGrad);
    printf("  edge 10-90%% rise: x %.2f px   y %.2f px\n",riseX,riseY);
    printMtf("horizontal",mh); printMtf("vertical",mv); printMtf("diagonal",md);
  }
}

int main(int argc,char**argv)
{
  Options o;
  if(argc<2) { fprintf(stderr,
      "usage: %s <jb|pd> [--image 0|1|2|card|vedge320|vedge321|hedge248|hedge249|grath|gratv|gratd|file.png]\n"
      "          [--suite] [--ideal] [--snr dB] [--ssb] [--noise-hz Hz (with --ideal)] [--clock-err frac]\n"
      "          [--tshift samples] [--out prefix] [--wav file.wav]\n",argv[0]); return 1; }
  o.mode=(!strcmp(argv[1],"pd"))?PD120:JB60;
  for(int i=2;i<argc;i++)
    {
      std::string a=argv[i];
      auto val=[&]()->const char*{ if(i+1>=argc) { fprintf(stderr,"%s needs a value\n",a.c_str()); exit(1); } return argv[++i]; };
      if(a=="--image") o.image=val();
      else if(a=="--ideal") o.ideal=true;
      else if(a=="--ssb") o.ssb=true;
      else if(a=="--snr") { o.hasSnr=true; o.snr=atof(val()); }
      else if(a=="--noise-hz") o.noiseHz=atof(val());
      else if(a=="--clock-err") o.clockErr=atof(val());
      else if(a=="--tshift") o.tshift=atof(val());
      else if(a=="--out") o.out=val();
      else if(a=="--wav") o.wav=val();
      else if(a=="--suite") o.suite=true;
      else { fprintf(stderr,"unknown option %s\n",a.c_str()); return 1; }
    }
  QGuiApplication app(argc,argv);
  if(o.suite) { runSuite(o); return 0; }
  QImage src=loadImage(o.image);
  Result r=runChain(o,src);
  report(o,r,src);
  if(!o.out.empty()) { src.save(QString::fromStdString(o.out)+"_src.png"); r.rx.save(QString::fromStdString(o.out)+"_rx.png"); }
  return 0;
}
