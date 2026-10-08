#include "drmrx.h"
#include "appglobal.h"
#include "drm.h"
#include "demodulator.h"
#include "dispatcher.h"
#include "filters.h"


drmRxStats drmStats;

drmRx::drmRx(QObject *parent) : QObject(parent),iqFilter(RXSTRIPE)
{
  srcDecoder=new sourceDecoder;
  demodulatorPtr=new demodulator;
}

drmRx::~drmRx()
{
  delete srcDecoder;
}

void drmRx::init()
{
  avgMER=0;
  avgMERAvailable=false;
  n = DRMBUFSIZE;
  /* initialisations */
  demodulatorPtr->init();
  initGetmode( n / 4);
  rRation = 1.000;
  samplerate_offset_estimation = 0.0;
  runstate = RUN_STATE_POWER_ON;		/* POWER_ON */
  channel_decoding();
  runstate = RUN_STATE_INIT;		/* INIT */
  channel_decoding();
  runstate = RUN_STATE_FIRST;			/* FIRSTRUN */
  runstate = RUN_STATE_NORMAL;			/* NORMAL RUN */
  srcDecoder->init();
}



void drmRx::run(DSPFLOAT *dataPtr)
{
  bool done=false;
  DSPFLOAT temp;
  displayDRMStatEvent *ce1;
  displayDRMInfoEvent *ce2 ;

  temp=WMERFAC;
  if(temp<0) temp=0;
  if(avgMERAvailable)
    {
      avgMER=(1-0.05)*avgMER+0.05*temp;
      ce1 = new displayDRMStatEvent(avgMER);
      ce1->waitFor(&done);
      QApplication::postEvent(dispatcherPtr, ce1);
      while(!done) { usleep(10);}

    }

 if(input_samples_buffer_request ==0)
    {
      demodulatorPtr->demodulate(resamp_signal,0);
    }
  iqFilter.process(dataPtr,resamp_signal,RXSTRIPE);
  im=RXSTRIPE;

  demodulatorPtr->demodulate(resamp_signal,im);

  {
    drmStats.stripes++;
    if(demodulatorPtr->isTimeSync()) drmStats.timeSync++;
    if(demodulatorPtr->isFrameSync()) drmStats.frameSync++;
    if(fac_valid==1)
      {
        drmStats.facValid++;
        drmStats.merSum+=(WMERFAC<0 ? 0 : WMERFAC);
        drmStats.merCount++;
        if(WMERMSC>=0 && msc_valid!=INVALID)
          {
            drmStats.merMscSum+=WMERMSC;
            drmStats.merMscCount++;
          }
        drmStats.mode=robustness_mode;
        drmStats.occupancy=spectrum_occupancy;
      }
    bool msc=(msc_valid!=INVALID);
    if(msc) drmStats.mscValid++;
    else if(drmStats.prevMsc) drmStats.mscFlaps++;
    drmStats.prevMsc=msc;
  }

  done=false;
  ce2 = new displayDRMInfoEvent;
  ce2->waitFor(&done);
  QApplication::postEvent(dispatcherPtr, ce2);
  while(!done) { usleep(10);}
}

