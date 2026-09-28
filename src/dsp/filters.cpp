#include "filters.h"
#include "filter.h"
#include "appglobal.h"

#include <QDebug>



syncFilter:: syncFilter(uint maxLength):
  sync1200(filter::FTIIR,maxLength),sync1900(filter::FTIIR,maxLength),
  sync1200lp(filter::FTFIR,maxLength),sync1900lp(filter::FTFIR,maxLength)
{
  init();
}

syncFilter::~syncFilter()
{

}

void syncFilter:: init()
{
  // setup the syncFilters
  sync1200.init();
  sync1200.nZeroes=SYNCBPNUMZEROES;
  sync1200.nPoles=SYNCBPNUMPOLES;
  sync1200.gain=SYNCBP1200GAIN;
  sync1200.coefZPtr=(FILTERPARAMTYPE *)z_sync_bp1200;
  sync1200.coefPPtr=(FILTERPARAMTYPE *)p_sync_bp1200;
  sync1200.allocate();

  sync1900.init();
  sync1900.nZeroes=SYNCBPNUMZEROES;
  sync1900.nPoles=SYNCBPNUMPOLES;
  sync1900.gain=SYNCBP1900GAIN;
  sync1900.coefZPtr=(FILTERPARAMTYPE *)z_sync_bp1900;
  sync1900.coefPPtr=(FILTERPARAMTYPE *)p_sync_bp1900;
  sync1900.allocate();

  sync1200lp.init();
  sync1200lp.nZeroes=SYNCLPTAPS;
  sync1200lp.gain=SYNCLPGAIN;
  sync1200lp.coefZPtr=(FILTERPARAMTYPE *)z_sync_lp;
  sync1200lp.allocate();

  sync1900lp.init();
  sync1900lp.nZeroes=SYNCLPTAPS;
  sync1900lp.gain=SYNCLPGAIN;
  sync1900lp.coefZPtr=(FILTERPARAMTYPE *)z_sync_lp;
  sync1900lp.allocate();

  detect1200Ptr= sync1200lp.filteredPtr;
  detect1900Ptr= sync1900lp.filteredPtr;
}

void syncFilter::process(FILTERPARAMTYPE *dataPtr)
{
  sync1200.processIIRRectified(dataPtr);
  sync1200lp.processFIR(sync1200.filteredPtr,sync1200lp.filteredPtr);
#ifndef DISABLENARROW
  sync1900.processIIRRectified(dataPtr);
  sync1900lp.processFIR(sync1900.filteredPtr,sync1900lp.filteredPtr);
#endif
}






videoFilter::videoFilter(uint maxLength,bool wide):videoFltr(filter::FTFIR,maxLength),lpFltr(filter::FTFIR,maxLength),wideFilter(wide)
{
  init();
}

videoFilter::~videoFilter()
{
}

namespace
{
  // modified Bessel function of the first kind, order 0 (power series)
  double besselI0(double x)
  {
    double sum=1,term=1;
    for(int k=1;k<60;k++)
      {
        double h=x/(2*k);
        term*=h*h;
        sum+=term;
        if(term<1e-14*sum) break;
      }
    return sum;
  }
}

/*!
  Kaiser windowed sinc low pass, VIDEOFIRNUMTAPS taps, -6 dB at VIDEOWIDECUTOFF (the ideal cutoff of a windowed
  sinc), unity gain at DC. Runs on the complex baseband, so the pass band is +/-VIDEOWIDECUTOFF around the carrier.
*/
void videoFilter::designWideTaps(FILTERPARAMTYPE *taps)
{
  const int n=VIDEOFIRNUMTAPS;
  const double mid=(n-1)/2.0;
  const double fc=VIDEOWIDECUTOFF/SAMPLERATE;   // cycles per sample
  const double norm=besselI0(VIDEOWIDEKAISERBETA);
  double sum=0;
  int i;
  for(i=0;i<n;i++)
    {
      double x=i-mid;
      double sinc=(x==0) ? 2*fc : sin(2*M_PI*fc*x)/(M_PI*x);
      double r=2.0*i/(n-1)-1.0;
      taps[i]=sinc*besselI0(VIDEOWIDEKAISERBETA*sqrt(1.0-r*r))/norm;
      sum+=taps[i];
    }
  for(i=0;i<n;i++) taps[i]/=sum;
}

void videoFilter::init()
{
  videoFltr.init();
  lpFltr.init();
  videoFltr.volumeAttackIntegrator=0.07;
  videoFltr.volumeDecayIntegrator=0.01;
  videoFltr.nZeroes=VIDEOFIRNUMTAPS-1;
  videoFltr.frCenter=VIDEOFIRCENTER;
  if(wideFilter)
    {
      // owned by videoFltr (freed by filter::deleteBuffers), like filter::setupMatchedFilter's taps
      FILTERPARAMTYPE *taps=new FILTERPARAMTYPE[VIDEOFIRNUMTAPS];
      designWideTaps(taps);
      videoFltr.coefZPtr=taps;
      videoFltr.coefZPtrNewed=true;
      videoFltr.gain=1;
    }
  else
    {
      videoFltr.gain=VIDEOFIRGAIN;
      videoFltr.coefZPtr=(FILTERPARAMTYPE *)videoFilterCoefFIR;
    }
  videoFltr.allocate();
  demodPtr=videoFltr.demodPtr;
  lpFltr.setupMatchedFilter(0,1);
}

void videoFilter::process(FILTERPARAMTYPE *dataPtr)
{

  videoFltr.processFIRDemod(dataPtr,videoFltr.filteredPtr);
  lpFltr.processFIRInt(videoFltr.filteredPtr,videoFltr.demodPtr);
}



wfFilter::wfFilter(uint maxLength):wfFltr(filter::FTFIR,maxLength)
{
  init();
}

wfFilter::~wfFilter()
{

}

void wfFilter::init()
{
  wfFltr.init();
  wfFltr.nZeroes=TXWFNUMTAPS-1;
  wfFltr.gain=1;
  wfFltr.coefZPtr=(FILTERPARAMTYPE *)wfFilterCoef;
  wfFltr.volumeAttackIntegrator=0.07;
  wfFltr.volumeDecayIntegrator=0.01;
  wfFltr.allocate();
}

void  wfFilter::process(double *dataPtr, uint dataLength)
{
  wfFltr.dataLen=dataLength;
  wfFltr.processFIR(dataPtr,dataPtr);
}



drmHilbertFilter::drmHilbertFilter(uint maxLength):drmFltr(filter::FTFIR,maxLength)
{
  init();
}

drmHilbertFilter::~drmHilbertFilter()
{
}

void drmHilbertFilter::init()
{
  drmFltr.init();
  drmFltr.nZeroes=DRMHILBERTTAPS-1;
  drmFltr.nPoles=0;
  drmFltr.coefZPtr=(FILTERPARAMTYPE *)drmHilbertCoef;
  drmFltr.gain=DRMHILBERTGAIN;
  drmFltr.allocate();
}

//void drmHilbertFilter::process(float *dataPtr,uint dataLength)
//{

//  process(dataPtr,dataPtr,dataLength);
//}

void drmHilbertFilter::process(FILTERPARAMTYPE *dataPtr, float *outputPtr,uint dataLength)
{
  drmFltr.dataLen=dataLength;
  drmFltr.processIQ(dataPtr,outputPtr);
}



