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
#include "videofilterselection.h"
#include "chromadeconvolution.h"
#include "chromaedgeboost.h"
#include "chromagridphase.h"
#include "chromacompanding.h"
#include "chromapseudoluma.h"
#include "chromatriangledecimation.h"
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
    bool wide=false;       // --fir wide: the wide video filter (dsp/filters videoFilter(.., true))
    bool hasSnr=false;
    double snr=0;          // dB in 2.7 kHz, white noise at the receiver input
    double noiseHz=0;      // --ideal only: gaussian noise on the demodulated frequency
    double clockErr=0;     // RX clock error (fraction)
    double tshift=0;       // extra timing shift (12 kHz samples)
    std::string out,wav;
    bool suite=false;
    bool vis=false;        // --vis: --wav gets the leader/VIS header the real transmitter sends, so the app can detect the mode
    int count=1;           // --count N: N pictures in the --wav file
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
    if(name=="cedge320") return metrics::cEdge(W,H,320);
    if(name=="cedge321") return metrics::cEdge(W,H,321);
    if(name=="medge320") return metrics::mEdge(W,H,320);
    if(name=="medge321") return metrics::mEdge(W,H,321);
    if(name=="dedge320") return metrics::dEdge(W,H,320);
    if(name=="dedge321") return metrics::dEdge(W,H,321);
    // Generic cedge<NNN>/dedge<NNN> fallback (idea 12 attempt 3's sub-slot-phase sweep needs more than
    // the two hardcoded positions above) -- report() prints just the edge-width line for these, not the
    // full cedge320/321-style report.
    if(name.rfind("cedge",0)==0 && name.size()>5) return metrics::cEdge(W,H,atoi(name.c_str()+5));
    if(name.rfind("dedge",0)==0 && name.size()>5) return metrics::dEdge(W,H,atoi(name.c_str()+5));
    if(name=="grath") return metrics::grating(W,H,'h');
    if(name=="gratv") return metrics::grating(W,H,'v');
    if(name=="gratd") return metrics::grating(W,H,'d');
    if(name=="cgrath") return metrics::chromaGrating(W,H,'h');
    if(name=="cgratv") return metrics::chromaGrating(W,H,'v');
    QString path=(name=="card") ? QString(IMAGE_DIR)+"/card.png" : QString::fromStdString(name);
    QImage im(path);
    if(im.isNull()) { fprintf(stderr,"cannot load image %s\n",path.toLatin1().data()); exit(2); }
    im=im.convertToFormat(QImage::Format_RGB32);
    if(im.width()!=W||im.height()!=H) im=im.scaled(W,H,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    return im;
  }

  // ------------------------------------------------------------------ channel
  const double kAudioAmp=24578.;     // what synthesizer::nextSample produces (TXSINEPEAK)

  // a frequency of 0 means silence
  std::vector<double> makeAudio(const std::vector<float> &f48)
  {
    std::vector<double> a(f48.size());
    double ph=0;
    for(size_t i=0;i<f48.size();i++)
      {
        if(f48[i]<=0) { a[i]=0; continue; }
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
  std::vector<quint16> demodChain(const std::vector<double> &audio,bool wide)
  {
    const unsigned block=DOWNSAMPLESIZE;
    downsampleFilter ds(block,true);
    videoFilter vf(RXSTRIPE,wide);
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

  void tone(std::vector<float> &t,double seconds,double freq)
  {
    t.insert(t.end(),(size_t)lround(seconds*48000.),(float)freq);
  }

  // What sstvTx::sendPreamble() and sendVIS() send before the picture (src/sstv/sstvtx.cpp), which is what
  // the receiver needs to recognise the mode. Kept in step with those two functions by hand.
  std::vector<float> visHeader(esstvMode mode)
  {
    std::vector<float> t;
    static const double pre[8]={1900,1500,1900,1500,2300,1500,2300,1500};
    for(double f:pre) tone(t,0.1,f);
    tone(t,0.3,1900); tone(t,0.01,1200); tone(t,0.3,1900);
    int code=SSTVTable[mode].VISCode;
    tone(t,0.030,1200);                                   // start bit
    for(int i=0;i<8;i++) { tone(t,0.030,(code&1) ? 1100 : 1300); code>>=1; }
    tone(t,0.030,1200);                                   // stop bit
    return t;
  }

  // header + picture, `count` times, with silence in front and between (a recording, not a bare picture)
  std::vector<float> recordingTrack(const std::vector<float> &picture,esstvMode mode,bool vis,int count)
  {
    std::vector<float> t;
    tone(t,0.5,0);
    for(int k=0;k<count;k++)
      {
        if(vis) { std::vector<float> h=visHeader(mode); t.insert(t.end(),h.begin(),h.end()); }
        t.insert(t.end(),picture.begin(),picture.end());
        tone(t,1.0,0);
      }
    return t;
  }

  std::vector<float> withTail(std::vector<float> f48)
  {
    f48.insert(f48.end(),48000/2,1500.f);
    while(f48.size()%DOWNSAMPLESIZE) f48.push_back(1500.f);
    return f48;
  }

  // delay (12 kHz samples) between a frequency step entering the TX track and its 50% point in the demod track
  double calibrateDelay(bool wide)
  {
    static double cached[2]={-1,-1};
    double &c=cached[wide ? 1 : 0];
    if(c>=0) return c;
    std::vector<float> f(14400,1500.f);
    f.insert(f.end(),14400,2300.f);
    f.insert(f.end(),9600,1500.f);
    std::vector<quint16> y=demodChain(makeAudio(withTail(f)),wide);
    for(size_t n=1500;n<y.size();n++)
      if(y[n]>=1900 && y[n-1]<1900)
        {
          double frac=(1900.-y[n-1])/((double)y[n]-y[n-1]);
          c=(n-1+frac)-3600.;
          return c;
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

  // A single-column colour impulse in the last line pair (the rest of the picture flat grey), used
  // to measure the real chain's slot-domain Cr/Cb impulse response for RX idea 4's filter design
  // (--dump-slots). JB60's chroma for pair p depends only on pair p's own two rows (see getLine()),
  // so placing the impulse in the LAST pair and reading back lastCr()/lastCb() right after
  // process() returns isolates exactly that one pair's response, uncontaminated by any other pair.
  QImage impulseImage(int x0,int width,QColor fg,QColor bg=QColor(128,128,128))
  {
    QImage im(W,H,QImage::Format_RGB32);
    im.fill(bg.rgb());
    for(int y=H-2;y<H;y++)
      for(int x=x0;x<x0+width && x<W;x++)
        im.setPixel(x,y,fg.rgb());
    return im;
  }

  // ------------------------------------------------------------------ one TX -> channel -> RX pass
  struct Result { QImage rx; double seconds; int rxResult; int lines,imageLines; double delay; };

  // outCr/outCb, when non-null and o.mode==JB60: filled with the last line pair's demodulated,
  // pre-deconvolution Cr/Cb slot arrays (modeJB60::lastCr()/lastCb()) before rx is deleted --
  // used only by --dump-slots.
  Result runChain(const Options &o,const QImage &src,
                   std::vector<unsigned char> *outCr=nullptr,std::vector<unsigned char> *outCb=nullptr)
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
    std::vector<quint16> demodWide;   // JB60's chroma-wide path only (see below); empty means "not used"
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
        if(!o.wav.empty())
          {
            std::vector<double> rec=makeAudio(withTail(recordingTrack(f48,o.mode,o.vis,o.count)));
            if(o.hasSnr) addNoise(rec,o.snr);
            if(o.ssb) bandLimit(rec);
            writeWav(o.wav,rec);
          }
        res.delay=calibrateDelay(o.wide)+o.tshift;
        demod=alignTrack(demodChain(audio,o.wide),res.delay);
        // JB60's per-segment chroma-wide path (RX idea #1): a second track through the wide filter, with its
        // own independently-calibrated delay -- verified equal to the narrow one's, not assumed (see README).
        if(o.mode==JB60) demodWide=alignTrack(demodChain(audio,true),calibrateDelay(true)+o.tshift);
      }
    for(int i=0;i<24000;i++) demod.push_back(1500);   // tail
    for(int i=0;i<24000 && !demodWide.empty();i++) demodWide.push_back(1500);

    modeBase *rx=(o.mode==JB60)?(modeBase*)new modeJB60(o.mode,demod.size(),false,false):(modeBase*)new modePD(o.mode,demod.size(),false,false);
    rx->init(12000.*(1+o.clockErr));
    rx->setRxSampleCounter(0);
    if(!demodWide.empty()) rx->setWideDemod(demodWide.data());
    res.rxResult=(int)rx->process(demod.data(),0,false,0);
    res.lines=rx->receivedLines(); res.imageLines=rx->imageLines();
    res.rx=rxIv.img;
    if(o.mode==JB60 && (outCr||outCb))
      {
        modeJB60 *rxJb=static_cast<modeJB60*>(rx);
        if(outCr) *outCr=rxJb->lastCr();
        if(outCb) *outCb=rxJb->lastCb();
      }
    delete tx; delete rx;
    return res;
  }

  metrics::Rect textRegion(const std::string &image)
  {
    if(image=="card") return {0,120,W,300};
    return {0,0,W,H};
  }

  // Idea 9 probe: prints the true (linear-light) luma at both plateaus of a chroma edge, plus any
  // overshoot/undershoot the transition band shows beyond them -- overshoot/undershoot is evidence of
  // chroma-smear-driven brightness leakage that a constant-luminance transform would prevent; if the
  // profile stays within [min(plateaus),max(plateaus)], there's nothing here for CL to fix.
  void reportTrueLumaLeak(const QImage &rx,int x0)
  {
    const int half=48,plateauN=10;              // see trueLumaProfile()'s comment on why +-48
    std::vector<double> tl=metrics::trueLumaProfile(rx,x0,half);
    double loP=0,hiP=0;
    for(int i=0;i<plateauN;i++) { loP+=tl[i]; hiP+=tl[tl.size()-1-i]; }
    loP/=plateauN; hiP/=plateauN;
    // interior = everything except the two plateau margins themselves, so the overshoot/undershoot
    // search can't just be re-detecting the plateau average it was computed from.
    double mn=*std::min_element(tl.begin()+plateauN,tl.end()-plateauN);
    double mx=*std::max_element(tl.begin()+plateauN,tl.end()-plateauN);
    double lo=std::min(loP,hiP),hi=std::max(loP,hiP);
    printf("  true (linear-light) luma at plateaus: %.2f / %.2f   overshoot %.2f  undershoot %.2f  (idea 9 CL-leakage probe)\n",
           loP,hiP,std::max(0.,mx-hi),std::max(0.,lo-mn));
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
           o.ideal?"ideal (baseband)":(o.wide?"real (downsampler+WIDE video FIR)":"real (downsampler+video FIR)"),o.hasSnr?QString("  snr %1 dB").arg(o.snr).toLatin1().data():"",o.ssb?"  ssb":"");
    printf("  tx %.2f s (table %.2f)  video-path delay %.2f samples  rx result %d, lines %d/%d\n",r.seconds,txSSTVParam.imageTime,r.delay,r.rxResult,r.lines,r.imageLines);
    printf("  PSNR R %.2f G %.2f B %.2f  luma %.2f  all %.2f dB\n",metrics::psnr(src,r.rx,0),metrics::psnr(src,r.rx,1),metrics::psnr(src,r.rx,2),metrics::psnr(src,r.rx,3),metrics::psnr(src,r.rx,4));
    printf("  SSIM full %.3f  text-region %.3f   gradient kept: full %.2f text-region %.2f\n",metrics::ssim(src,r.rx,full),metrics::ssim(src,r.rx,txt),
           metrics::gradEnergy(r.rx,full)/metrics::gradEnergy(src,full),metrics::gradEnergy(r.rx,txt)/metrics::gradEnergy(src,txt));
    if(o.image=="vedge320") printf("  edge 10-90%% rise (x): %.2f px\n",metrics::edgeRiseV(r.rx,320));
    if(o.image=="vedge321") printf("  edge 10-90%% rise (x): %.2f px\n",metrics::edgeRiseV(r.rx,321));
    if(o.image=="hedge248") printf("  edge 10-90%% rise (y): %.2f px\n",metrics::edgeRiseH(r.rx,248));
    if(o.image=="hedge249") printf("  edge 10-90%% rise (y): %.2f px\n",metrics::edgeRiseH(r.rx,249));
    if(o.image=="cedge320")
      {
        printf("  chroma edge 10-90%% rise (x): R %.2f px  B %.2f px   50%% crossing: R %.3f px  B %.3f px\n",
               metrics::edgeRiseChannel(r.rx,320,0),metrics::edgeRiseChannel(r.rx,320,2),
               metrics::edgeCrossChannel(r.rx,320,0),metrics::edgeCrossChannel(r.rx,320,2));
        reportTrueLumaLeak(r.rx,320);
      }
    if(o.image=="cedge321")
      {
        printf("  chroma edge 10-90%% rise (x): R %.2f px  B %.2f px   50%% crossing: R %.3f px  B %.3f px\n",
               metrics::edgeRiseChannel(r.rx,321,0),metrics::edgeRiseChannel(r.rx,321,2),
               metrics::edgeCrossChannel(r.rx,321,0),metrics::edgeCrossChannel(r.rx,321,2));
        reportTrueLumaLeak(r.rx,321);
      }
    if(o.image=="medge320"||o.image=="medge321")
      {
        int x0=(o.image=="medge320")?320:321;
        printf("  luma edge 10-90%% rise (x): %.2f px   chroma: R %.2f px  B %.2f px\n",
               metrics::edgeRiseV(r.rx,x0),metrics::edgeRiseChannel(r.rx,x0,0),metrics::edgeRiseChannel(r.rx,x0,2));
      }
    if(o.image=="dedge320") printf("  D edge 10-90%% rise (x): %.2f px\n",metrics::edgeRiseD(r.rx,320));
    if(o.image=="dedge321") printf("  D edge 10-90%% rise (x): %.2f px\n",metrics::edgeRiseD(r.rx,321));
    if(o.image.rfind("cedge",0)==0 && o.image!="cedge320" && o.image!="cedge321")
      {
        int x0=atoi(o.image.c_str()+5);
        printf("  chroma edge 10-90%% rise (x): R %.2f px  B %.2f px   50%% crossing: R %.3f px  B %.3f px\n",
               metrics::edgeRiseChannel(r.rx,x0,0),metrics::edgeRiseChannel(r.rx,x0,2),
               metrics::edgeCrossChannel(r.rx,x0,0),metrics::edgeCrossChannel(r.rx,x0,2));
      }
    if(o.image.rfind("dedge",0)==0 && o.image!="dedge320" && o.image!="dedge321")
      printf("  D edge 10-90%% rise (x): %.2f px\n",metrics::edgeRiseD(r.rx,atoi(o.image.c_str()+5)));
    if(o.image=="grath") printMtf("horizontal (vertical stripes)",metrics::mtf(r.rx,'h'));
    if(o.image=="gratv") printMtf("vertical (horizontal stripes)",metrics::mtf(r.rx,'v'));
    if(o.image=="gratd") printMtf("diagonal",metrics::mtf(r.rx,'d'));
    if(o.image=="cgrath") printMtf("chroma horizontal (vertical stripes)",metrics::mtfChannel(r.rx,'h',2));
    if(o.image=="cgratv") printMtf("chroma vertical (horizontal stripes)",metrics::mtfChannel(r.rx,'v',2));
  }

  // fixed set of targets: the table in README.md
  void runSuite(Options o)
  {
    struct Row { const char *name; };
    double riseX=0,riseY=0,riseC=0;
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
    for(const char *n:{"cedge320","cedge321"})
      {
        o.image=n; QImage s=loadImage(o.image); QImage rx=runChain(o,s).rx; int x0=atoi(n+5);
        riseC+=(metrics::edgeRiseChannel(rx,x0,0)+metrics::edgeRiseChannel(rx,x0,2))/2/2;   // avg R/B, avg over the two images
      }
    { o.image="grath"; QImage s=loadImage(o.image); mh=metrics::mtf(runChain(o,s).rx,'h'); }
    { o.image="gratv"; QImage s=loadImage(o.image); mv=metrics::mtf(runChain(o,s).rx,'v'); }
    { o.image="gratd"; QImage s=loadImage(o.image); md=metrics::mtf(runChain(o,s).rx,'d'); }
    printf("suite  mode %s  channel %s%s%s\n",txSSTVParam.name.toLatin1().data(),o.ideal?"ideal":(o.wide?"real, wide FIR":"real"),o.hasSnr?QString("  snr %1 dB").arg(o.snr).toLatin1().data():"",o.ssb?"  ssb":"");
    printf("  card: luma PSNR %.2f dB  SSIM text %.3f full %.3f  text gradient kept %.2f\n",cardYpsnr,cardSsimText,cardSsimFull,cardGrad);
    printf("  edge 10-90%% rise: x %.2f px   y %.2f px   chroma %.2f px\n",riseX,riseY,riseC);
    printMtf("horizontal",mh); printMtf("vertical",mv); printMtf("diagonal",md);
  }

  // RX idea 4 design tool: dumps the real chain's Cr/Cb slot-domain impulse response (a flat-
  // background run and a one-column-impulse run, both through the *current* Options -- e.g. pass
  // --chroma-wide to measure the wide filter's response instead of the narrow one) as plain text
  // on stdout, for offline FFT/Wiener-inverse design. See tests/jb60_loopback/README.md, RX idea 4.
  void dumpSlots(const Options &o,const std::string &channel)
  {
    QImage flat=impulseImage(320,0,QColor(128,128,128));   // width 0: no impulse, pure background
    QImage imp=impulseImage(320,1,QColor(255,0,0));        // one saturated column: excites both Cr and Cb
    // Y-held-(near)constant step (128,128,128) -> (255,87,0): R 128->255 and B 128->0 push Cr/Cb hard in
    // opposite directions while G is chosen so luma stays ~128 on both sides (127.8 vs 128.0, <1 count off,
    // rounds to the same guideLut[0] bucket) -- so downsampleChroma()'s luma-guided weights stay uniform
    // across the transition and this is effectively a plain linear box-average + pre-emphasis + channel
    // system being step-excited, at full swing (much better SNR than the single-column impulse above) for
    // deriving the impulse response by differencing. See tests/jb60_loopback/README.md, RX idea 4.
    QImage step=impulseImage(320,320,QColor(255,87,0));
    std::vector<unsigned char> crF,cbF,crI,cbI,crS,cbS;
    runChain(o,flat,&crF,&cbF);
    runChain(o,imp,&crI,&cbI);
    runChain(o,step,&crS,&cbS);
    auto dump=[&](const char *tag,const std::vector<unsigned char> &v)
      {
        printf("%s:",tag);
        for(unsigned char b:v) printf(" %d",(int)b);
        printf("\n");
      };
    if(channel=="cr"||channel=="both") { dump("cr_flat",crF); dump("cr_impulse",crI); dump("cr_step",crS); }
    if(channel=="cb"||channel=="both") { dump("cb_flat",cbF); dump("cb_impulse",cbI); dump("cb_step",cbS); }
  }
}

int main(int argc,char**argv)
{
  Options o;
  if(argc<2) { fprintf(stderr,
      "usage: %s <jb|pd> [--image 0|1|2|card|vedge320|vedge321|hedge248|hedge249|cedge320|cedge321|medge320|medge321|dedge320|dedge321|grath|gratv|gratd|cgrath|cgratv|file.png]\n"
      "          [--suite] [--ideal] [--fir wide|narrow] [--chroma-wide] [--chroma-deconv] [--chroma-edge-boost] [--chroma-grid-phase f] [--chroma-compand-gamma g] [--chroma-pseudo-luma f] [--chroma-triangle] [--snr dB] [--ssb] [--noise-hz Hz (with --ideal)] [--clock-err frac]\n"
      "          [--tshift samples] [--out prefix] [--wav file.wav [--vis] [--count N]] [--dump-slots cr|cb|both]\n"          "       %s --compare a.png b.png\n",argv[0],argv[0]); return 1; }
  if(!strcmp(argv[1],"--compare"))
    {
      if(argc<4) { fprintf(stderr,"--compare a.png b.png\n"); return 1; }
      QGuiApplication capp(argc,argv);
      QImage a=QImage(argv[2]).convertToFormat(QImage::Format_RGB32),b=QImage(argv[3]).convertToFormat(QImage::Format_RGB32);
      if(a.isNull()||b.isNull()||a.size()!=b.size()) { fprintf(stderr,"cannot compare: unreadable or different size\n"); return 2; }
      printf("PSNR luma %.2f dB  all %.2f dB  SSIM %.3f\n",metrics::psnr(a,b,3),metrics::psnr(a,b,4),metrics::ssim(a,b,{0,0,a.width(),a.height()}));
      return 0;
    }
  o.mode=(!strcmp(argv[1],"pd"))?PD120:JB60;
  bool chromaWide=false,chromaDeconv=false,chromaEdgeBoost=false,chromaTriangle=false;
  double gridPhase=0.0;
  double compandGamma=1.0;
  double pseudoLumaAmp=0.0;
  std::string dumpSlotsChannel;
  for(int i=2;i<argc;i++)
    {
      std::string a=argv[i];
      auto val=[&]()->const char*{ if(i+1>=argc) { fprintf(stderr,"%s needs a value\n",a.c_str()); exit(1); } return argv[++i]; };
      if(a=="--image") o.image=val();
      else if(a=="--ideal") o.ideal=true;
      else if(a=="--ssb") o.ssb=true;
      else if(a=="--fir") { std::string v=val(); if(v!="wide"&&v!="narrow") { fprintf(stderr,"--fir wide|narrow\n"); return 1; } o.wide=(v=="wide"); }
      else if(a=="--snr") { o.hasSnr=true; o.snr=atof(val()); }
      else if(a=="--noise-hz") o.noiseHz=atof(val());
      else if(a=="--clock-err") o.clockErr=atof(val());
      else if(a=="--tshift") o.tshift=atof(val());
      else if(a=="--out") o.out=val();
      else if(a=="--wav") o.wav=val();
      else if(a=="--suite") o.suite=true;
      else if(a=="--vis") o.vis=true;
      else if(a=="--count") o.count=std::max(1,atoi(val()));
      else if(a=="--chroma-wide") chromaWide=true;
      else if(a=="--chroma-deconv") chromaDeconv=true;
      else if(a=="--chroma-edge-boost") chromaEdgeBoost=true;
      else if(a=="--chroma-grid-phase") gridPhase=atof(val());
      else if(a=="--chroma-compand-gamma") compandGamma=atof(val());
      else if(a=="--chroma-pseudo-luma") pseudoLumaAmp=atof(val());
      else if(a=="--chroma-triangle") chromaTriangle=true;
      else if(a=="--dump-slots") dumpSlotsChannel=val();
      else { fprintf(stderr,"unknown option %s\n",a.c_str()); return 1; }
    }
  // Deterministic regardless of any real qsstv settings on this machine: off unless --chroma-wide asks for it
  // (this is what modeJB60::getPixels() reads for its per-segment choice; see RX idea #1 in videofilterselection.h).
  setWideVideoFilterOverride(chromaWide ? 1 : 0);
  // Same idea, RX idea #4 (chromadeconvolution.h): off unless --chroma-deconv asks for it.
  setChromaDeconvolutionOverride(chromaDeconv ? 1 : 0);
  // Same idea, TX idea 12 (chromaedgeboost.h): off unless --chroma-edge-boost asks for it.
  setChromaEdgeBoostOverride(chromaEdgeBoost ? 1 : 0);
  // idea 12 attempt 3 (chromagridphase.h): 0.0 by default, bit-identical to the fixed grid.
  setChromaGridPhase((float)gridPhase);
  // idea 14 (chromacompanding.h): 1.0 by default, an exact identity.
  setChromaCompandGamma((float)compandGamma);
  // idea 16 (chromapseudoluma.h): 0.0 by default (off).
  setChromaPseudoLumaAmplitude((float)pseudoLumaAmp);
  // idea 13 (chromatriangledecimation.h): off unless --chroma-triangle asks for it.
  setChromaTriangleDecimationOverride(chromaTriangle ? 1 : 0);
  QGuiApplication app(argc,argv);
  if(!dumpSlotsChannel.empty()) { dumpSlots(o,dumpSlotsChannel); return 0; }
  if(o.suite) { runSuite(o); return 0; }
  QImage src=loadImage(o.image);
  Result r=runChain(o,src);
  report(o,r,src);
  if(!o.out.empty()) { src.save(QString::fromStdString(o.out)+"_src.png"); r.rx.save(QString::fromStdString(o.out)+"_rx.png"); }
  return 0;
}
