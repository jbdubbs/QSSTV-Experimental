#include "rxfunctions.h"
#include "appglobal.h"
#include "configparams.h"
#include "drmrx.h"
#include "soundbase.h"
#include "dispatcher.h"
#include "rxwidget.h"
#include "sstvrx.h"
#include "sstv/mmsstv_sstv_rx.h"
#include "sstv/engineselection.h"

#include <QApplication>



const QString rxStateStr[rxFunctions::RXINIT+1]=
{
  "IDLE",
  "RUNNING",
  "RESTART",
  "INIT"
};

rxFunctions::rxFunctions(QObject *parent) : QThread(parent)
{
  rxState=RXIDLE;
  sstvRxPtr=new sstvRx;
  drmRxPtr=new drmRx;
  mmsstvRxPtr=new MmsstvSstvRx;
  rxBytes=0;
  setObjectName("rx-thread");
}

rxFunctions::~rxFunctions()
{
  delete sstvRxPtr;
  delete drmRxPtr;
  delete mmsstvRxPtr;
}

//static DSPFLOAT dummyBuf[RXSTRIPE];

void rxFunctions::run()
{
  int count;
  DSPFLOAT tempBuf[RXSTRIPE];
  DSPFLOAT volBuf[RXSTRIPE];
  abort=false;
  while(!abort)
    {
      switch(rxState)
        {
        case RXIDLE:
          msleep(200);
          break;
        case RXRUNNING:
          if((count=soundIOPtr->rxBuffer.count())<RXSTRIPE)
            {
              msleep((250*RXSTRIPE)/rxClock);
              if(!soundIOPtr->isCapturing())
                {
                  switchRxState(RXINIT);
                }
            }
          else
            {
              //              addToLog("Load new buf",LOGPERFORM);
              rxBytes+=RXSTRIPE;
              //              addToLog(QString("rxBytes=%1").arg(rxBytes),LOGRXFUNC);
              soundIOPtr->rxBuffer.copyNoCheck(tempBuf,RXSTRIPE);
              soundIOPtr->rxVolumeBuffer.copyNoCheck(volBuf,RXSTRIPE);
              displayFFTEvent* ce = new displayFFTEvent(tempBuf);
              QApplication::postEvent(dispatcherPtr, ce);

              addToLog("fft display done",LOGPERFORM);
              switch (transmissionModeIndex)
                {
                case TRXDRM:
                  addToLog("drmRxPtr->run",LOGPERFORM);
                  drmRxPtr->run(tempBuf);
                  break;
                case TRXSSTV:
                  // Tell QSSTV's own detector whether mmsstv-core is the one
                  // actually receiving right now (as of the previous
                  // buffer's raw-tap drain below), so it holds off spamming
                  // "No sync" over mmsstv-core's status while it steps aside
                  // -- see sstvRx::setCoreEngineBusy()'s comment.
                  sstvRxPtr->setCoreEngineBusy(mmsstvRxPtr->isTrackingImage());
                  sstvRxPtr->run(tempBuf,volBuf);
                  break;
                case TRXNOMODE:
                  switchRxState(RXIDLE);
                  break;
                }
              // mmsstv-linux-port: independent drain of the raw
              // (un-decimated) audio tap for mmsstv-core's RX engine,
              // alongside (not instead of) sstvRxPtr's own
              // decimated-pipeline dispatch above. Gated on its own
              // buffer's fill level (it fills faster than rxBuffer, since
              // it isn't decimated) and on whether the RX engine
              // preference favors mmsstv-core at all (mmsstvCoreActiveForAnyMode(),
              // which just mirrors rxPreferCoreEngine()) -- RX doesn't know
              // which mode is incoming until VIS locks (mmsstv_sstv_rx.cpp
              // handles the per-mode support check once it does), so this
              // can't gate on one specific mode's setting the way TX does.
              // When the checkbox is off, this is simply skipped and
              // QSSTV's own pipeline handles every mode unmodified (see
              // syncprocessor.cpp's createModeBase()).
              // An engine switch (see switchEngine()) may be pending even
              // when the tap is no longer being fed; if one was, also
              // discard audio that piled up in the tap meanwhile so the
              // (possibly newly enabled) engine starts from live audio.
              if(mmsstvRxPtr->serviceAbort())
                {
                  static DSPFLOAT discardBuf[DOWNSAMPLESIZE];
                  while(soundIOPtr->rawRxBuffer.count()>=DOWNSAMPLESIZE)
                    {
                      soundIOPtr->rawRxBuffer.copyNoCheck(discardBuf,DOWNSAMPLESIZE);
                    }
                }
              if((transmissionModeIndex==TRXSSTV)
                 && mmsstvCoreActiveForAnyMode()
                 && (soundIOPtr->rawRxBuffer.count()>=DOWNSAMPLESIZE))
                {
                  static DSPFLOAT rawBuf[DOWNSAMPLESIZE];
                  soundIOPtr->rawRxBuffer.copyNoCheck(rawBuf,DOWNSAMPLESIZE);
                  mmsstvRxPtr->setQsstvBusy(sstvRxPtr->isReceivingImage());
                  mmsstvRxPtr->processSamples(rawBuf,DOWNSAMPLESIZE);
                }
            }
          break;
        case RXINIT:
          forceInit();
          switchRxState(RXIDLE);
          break;
        case RXRESTART:
          {
            init();
            switchRxState(RXRUNNING);
          }
          break;
        }
    }
  abort=false;
  rxState=RXIDLE;

}

void rxFunctions::stopThread()
{
  abort=true;
  if(!isRunning()) return;
  while(abort)
    {
      qApp->processEvents();
    }
}

void rxFunctions::init()
{
  switchRxState(RXINIT);
}

void rxFunctions::forceInit()
{

  if(transmissionModeIndex==TRXDRM)
    {

      drmRxPtr->init();
    }
  else
    {
      sstvRxPtr->init();
    }
}

bool rxFunctions::rxBusy()
{
  switch (transmissionModeIndex)
    {
    case TRXDRM:
      return drmBusy;
      break;
    case TRXSSTV:
      return sstvRxPtr->isBusy();
      break;
    case TRXNOMODE:
       return false;
      break;
    }
  return false;
}


void rxFunctions::stopAndWait()
{
  if(soundIOPtr) soundIOPtr->idleRX();
  switchRxState(RXINIT);
  if(!isRunning())
    {
      return;
    }
  while((rxState!=RXIDLE) && (isRunning()))
    {
      qApp->processEvents();
    }
}

void rxFunctions::restartRX()
{
  switchRxState(RXRESTART);
}

void rxFunctions::startRX()
{
  switchRxState(RXRUNNING);
}

void rxFunctions::eraseImage()
{
  if(isRunning())
    {
      if(transmissionModeIndex==TRXDRM)
        {
          drmRxPtr->eraseImage();
        }
      else
        {
          sstvRxPtr->eraseImage();
        }
    }
}

void rxFunctions::switchEngine()
{
  // Stop whichever engine is mid-picture (both, to be safe) so they never
  // paint the shared canvas at the same time.
  mmsstvRxPtr->abortImage();
  eraseImage();
}

void rxFunctions::switchRxState(erxState newState)
{
  addToLog(QString("%1 to %2").arg(rxStateStr[rxState]).arg(rxStateStr[newState]),LOGRXFUNC);
  rxState=newState;
}




#ifndef QT_NO_DEBUG
unsigned int rxFunctions::setOffset(unsigned int offset,bool ask)
{
  return sstvRxPtr->setOffset(offset,ask);
}
#endif
