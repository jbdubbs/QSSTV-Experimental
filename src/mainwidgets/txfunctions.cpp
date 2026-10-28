/***************************************************************************
 *   Copyright (C) 2000-2014 by Johan Maes                                 *
 *   on4qz@telenet.be                                                      *
 *   https://www.qsl.net/o/on4qz                                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "txfunctions.h"
#include "kgstvcontroller.h"
#include <QElapsedTimer>
#include "appglobal.h"
#include "sstv/sstvtx.h"
#include "cw.h"


#include "configparams.h"
#include <qmutex.h>
#include "synthes.h"
#include "modes/modes.h"
#include "dispatcher.h"
#include "cw.h"
#include <QApplication>
#include <assert.h>
#include "txwidget.h"
#include "soundbase.h"
#include "waterfalltext.h"
#include "hybridcrypt.h"
#include "rigcontrol.h"
#include "drm.h"
#include "drmtx.h"
#include "fixform.h"
#include "mainwindow.h"



QMutex txMutex;

int templateIndex;
bool useTemplate;
bool useCW;
bool useVOX;

const QString txStateStr[txFunctions::TXTEST+1]=
{
  "IDLE",
  "ACTIVE",
  "SENDTONE",
  "SENDWFID",
  "SENDCWID",
  "SENDDRM",
  "SENDDRMPIC",
  "SENDDRMBINARY",
  "SENDDRMBSR",
  "SENDDRMFIX",
  "SENDDRMTXT",
  "SSTVIMAGE",
  "SSTVPOST",
  "RESTART",
  "PREPARESSTV",
  "PREPAREDRMPIC",
  "PREPAREDRMBINARY",
  "PREPAREKGSTV",
  "SENDKGSTV",
  "TXTEST"
};


txFunctions::txFunctions(QObject *parent) : QThread(parent)
{

  txState=TXIDLE;
  started=false;
  sstvTxPtr=new sstvTx;
  drmTxPtr=new drmTx;
}

txFunctions::~txFunctions()
{
  delete synthesPtr;
  delete sstvTxPtr;
  delete drmTxPtr;
}



void txFunctions::init()
{
  synthesPtr=new synthesizer(txClock);
  switchTxState(TXIDLE);
  addToLog("txFunc: Init",LOGTXFUNC);
  drmTxPtr->init();

}

void txFunctions::run()
{
  double waterfallTime;
  bool cwAtEnd=false;
  QString startWFTxt,endWFTxt;


  abort=false;
  init();
  while(!abort)
    {

      started=true;
      switch (txState)
        {
        case TXIDLE:
          msleep(10);
          break;
        case TXACTIVE:
          msleep(1);
          break;
        case TXSENDTONE:
          waitTxOn();
          addToLog("txFunc: entered TXSENDTONE",LOGTXFUNC);
          startProgress(toneDuration);
          synthesPtr->sendTone(toneDuration,toneLowerFrequency,toneUpperFrequency,false);
          addToLog("txFunc: TXSENDTONE waiting for end",LOGTXFUNC);
          waitEnd();
          switchTxState(TXIDLE);
          break;

        case TXPREPAREDRMBINARY:
          {
            bool ok=true;
            if (useHybrid)
              ok=drmTxPtr->ftpDRMHybrid(binaryFilename, drmTxPtr->getTxFileName(binaryFilename));
            switchTxState(TXIDLE);
            prepareTXComplete(ok);
          }
          break;
        case TXPREPAREDRMPIC:
          {
            bool ok=true;
            if (useHybrid)
              ok=drmTxPtr->ftpDRMHybrid("", drmTxPtr->getTxFileName(""));
            switchTxState(TXIDLE);
            prepareTXComplete(ok);
          }
          break;

        case TXSENDDRMPIC:
          if(repeaterEnabled)
            {
              startWFTxt = startRepeaterWF;
              endWFTxt   = endRepeaterWF;
            }
          else
            {
              startWFTxt = startPicWF;
              endWFTxt   = endPicWF;
            }

          if (drmTxPtr->initDRMImage(false,""))
            {
              drmTxPtr->updateTxList();
              switchTxState(TXSENDDRM);
            }
          else
            switchTxState(TXIDLE);
          break;

        case TXSENDDRMBINARY:
          startWFTxt = startBinWF;
          endWFTxt   = endBinWF;
          if (drmTxPtr->initDRMImage(true,binaryFilename))
            {
              drmTxPtr->updateTxList();
              switchTxState(TXSENDDRM);
            }
          else
            switchTxState(TXIDLE);
          break;

        case TXSENDDRMBSR:
          startWFTxt = bsrWF;
          endWFTxt   = "";
          switchTxState(TXSENDDRM);
          break;

        case TXSENDDRMFIX:
          startWFTxt = fixWF;
          endWFTxt   = "";
          switchTxState(TXSENDDRM);
          break;

        case TXSENDDRM:
          waitTxOn();
          waterfallTime=waterfallPtr->getDuration(endWFTxt);
          waterfallTime+=waterfallPtr->getDuration(startWFTxt);
          cwAtEnd=useCW && !endWFTxt.isEmpty();
          if(cwAtEnd)
            {
              initCW(cwText);
              waterfallTime+=0.5+getCWDuration(); // silence gap + morse
            }

          startProgress(drmTxPtr->calcTxTime(waterfallTime));
          addToLog("start of wf",LOGTXFUNC);
          if(useVOX) synthesPtr->sendTone(1.,1700.,0,false);
          waterfallPtr->setText(startWFTxt);
          synthesPtr->sendWFText();
          addToLog("start of txDrm",LOGTXFUNC);
          drmTxPtr->start();

          addToLog("end of txDrm",LOGTXFUNC);
          if(txState==TXSENDDRM) // abort if txState is idle
            {
              waterfallPtr->setText(endWFTxt);
              synthesPtr->sendWFText();
              addToLog("end of wf",LOGTXFUNC);
              if(cwAtEnd) sendCW();
              addToLog("txFunc: TXSENDDRM waiting for end",LOGTXFUNC);
              waitEnd();
            }
          switchTxState(TXIDLE);
          break;

        case TXSENDDRMTXT:
          waitTxOn();
          break;

        case TXSENDWFID:
          addToLog("Entered TXSENDID",LOGTXFUNC);
          waitTxOn();
          addToLog("after txon TXSENDID",LOGTXFUNC);
          startProgress(waterfallPtr->getDuration());
          if(useVOX) synthesPtr->sendTone(1.,1700.,0,false);
          synthesPtr->sendWFText();
          addToLog("TXSENDID waiting for end",LOGTXFUNC);
          waitEnd();
          addToLog("TXSENDID  end",LOGTXFUNC);
          switchTxState(TXIDLE);
          break;

        case TXSENDCWID:
          waitTxOn();
          if(useVOX) synthesPtr->sendTone(1.,1700.,0,false);
          sendCW();
          waitEnd();
          switchTxState(TXIDLE);
          break;

        case TXPREPARESSTV:
          switchTxState(TXIDLE);
          prepareTXComplete(true);
          break;

        case TXSSTVIMAGE:
          waitTxOn();
          addToLog("Entered SSTVTXIMAGE",LOGTXFUNC);
          startProgress(sstvTxPtr->calcTxTime(0));
          addToLog("Entered before SSTVTXIMAGE",LOGTXFUNC);
          if(sstvTxPtr->sendImage(txWidgetPtr->getImageViewerPtr()))
            {
              switchTxState(TXSSTVPOST);
            }
          else
            {
              switchTxState(TXIDLE);
            }
          break;
        case TXSSTVPOST:
          addToLog("Entered TXSSTVPOST ",LOGTXFUNC);
          if (useCW || myCallsign.isEmpty())
            {
              sendPostamble();
            }
          if (useCW)
            {
              sendCW();
            }
          else
            {
              sendFSKID();
            }
          waitEnd();
          switchTxState(TXRESTART);
          break;
        case TXRESTART:
          switchTxState(TXIDLE);
          break;
        case TXPREPAREKGSTV:
          {
            kgstvWorking.store(true);QImage image;quint64 ticket;
            {QMutexLocker lock(&txMutex);image=kgstvImage;ticket=kgstvTicket;}
            QString error;
            const bool ok=!kgstvAbort.load() && kgstvControllerPtr && kgstvControllerPtr->prepareImageJob(image,myCallsign,&error);
            if(!ok && kgstvControllerPtr && !error.isEmpty())emit kgstvControllerPtr->statusChanged(error);
            const bool ready=ok && !kgstvAbort.load();
            if(!ready && kgstvControllerPtr)kgstvControllerPtr->cancelTransmit();
            kgstvWorking.store(false);switchTxState(TXIDLE);
            QApplication::postEvent(dispatcherPtr,new txPrepareCompleteEvent(ready,ticket));
          }
          break;
        case TXSENDKGSTV:
          kgstvWorking.store(true);sendKgstv();kgstvWorking.store(false);switchTxState(TXIDLE);
          break;
        case TXTEST:
          sendTestPattern();
          break;

        }
    }
  started=false;
  addToLog("txFunc stopped",LOGTXFUNC);
  abort=false;
  txState=TXIDLE;
}

int txFunctions::calcTxTime(bool binary, int overhead)
{
  bool ok=false;
  int txTime=0;
  if(transmissionModeIndex==TRXKGSTV)return kgstvControllerPtr?int(std::ceil(kgstvControllerPtr->duration())):0;
  if(transmissionModeIndex==TRXSSTV)
    {
      txTime=sstvTxPtr->calcTxTime(0);
    }
  else
    {
  if (binary)
    {
      ok=drmTxPtr->initDRMImage(true, binaryFilename);
    }
  else
    {
      ok=drmTxPtr->initDRMImage(false, "");
    }
  if (ok) txTime = drmTxPtr->calcTxTime(overhead);
  addToLog(QString("ok=%1, time=%2").arg(ok).arg(txTime), LOGTXFUNC);
}
  return txTime;



}


void txFunctions::startProgress(double duration)
{
  progressTXEvent *ce;
  ce=new progressTXEvent(duration+rigControllerPtr->getTxDelay()+SILENCEDELAY);
  QApplication::postEvent( dispatcherPtr, ce );  // Qt will delete it when done
}

void txFunctions::stopThread()
{
  kgstvAbort.store(true);
  abort=true;
  if(!isRunning()) return;
  while(abort && isRunning())
    {
      qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    }
}

bool txFunctions::txBusy()
{
  if(isRunning() && txState!=TXIDLE)
    {
      return true;
    }
  return false;
}

void txFunctions::startTX(etxState state)
{
  kgstvRunToken.fetch_add(1);
  if(state==TXSENDKGSTV)kgstvAbort.store(false);
  switchTxState(state);
}

void txFunctions::prepareTXComplete(bool ok)
{
  txPrepareCompleteEvent *ce;
  ce=new txPrepareCompleteEvent(ok);
  QApplication::postEvent( dispatcherPtr, ce );  // Qt will delete it when done
}

void txFunctions::prepareTX(etxState state)
{
  if (txState != TXIDLE)
    {
      addToLog("txState is not TXIDLE",LOGTXFUNC);
      return;
    }

  switch (state)
    {
    case TXPREPAREKGSTV:
      kgstvAbort.store(false);switchTxState(TXPREPAREKGSTV);
      break;
    case TXPREPARESSTV:
    case TXSSTVIMAGE:
      switchTxState(TXPREPARESSTV);
      break;

    case TXPREPAREDRMPIC:
    case TXSENDDRMPIC:
      switchTxState(TXPREPAREDRMPIC);
      break;

    case TXPREPAREDRMBINARY:
    case TXSENDDRMBINARY:
      switchTxState(TXPREPAREDRMBINARY);
      break;

    default:
      addToLog(QString("Invalid prepareTX state:%1").arg(state),LOGTXFUNC);
    }
}

void txFunctions::waitTxOn()
{
  double txDelay;
  txDelay=rigControllerPtr->getTxDelay();
  if(txDelay!=0)
    {
      synthesPtr->sendSilence(txDelay);
    }
}

// End-of-image tones, matching MMSSTV's SendSSTV() footer when no FSK ID is
// sent: a trailing tone one line long (capped at 0.5 s), then
// 1900/1500/1900/1500 at 100 ms each. With VOX the whole footer is 1900 Hz.
// (With an FSK ID the 300 ms lead-in in sendFSKID() plays that role.)
void txFunctions::sendPostamble()
{
  double lineTime=0.1;
  if (txSSTVParam.numberOfDataLines>0 && txSSTVParam.imageTime>0)
    {
      lineTime=txSSTVParam.imageTime/txSSTVParam.numberOfDataLines;
    }
  if (lineTime>0.5) lineTime=0.5;
  if (useVOX)
    {
      synthesPtr->sendTone(lineTime,1900.,0,true);
      return;
    }
  synthesPtr->sendTone(lineTime,1500.,0,true);
  synthesPtr->sendTone(0.1,1900.,0,true);
  synthesPtr->sendTone(0.1,1500.,0,true);
  synthesPtr->sendTone(0.1,1900.,0,true);
  synthesPtr->sendTone(0.1,1500.,0,true);
}

void txFunctions::waitEnd()
{
  if(synthesPtr->abortTone) return; // stopped by the user: the caller is already tearing TX down
  synthesPtr->sendTone(SILENCEDELAY,00,0,true); // send silence
  soundBase::txFileDone=true;   // --encode: everything is queued now, an empty buffer means the end
  addToLog("waitEnd() posting endTXImage",LOGTXFUNC);
  endImageTXEvent *ce=new endImageTXEvent;
  QApplication::postEvent(dispatcherPtr, ce );  // Qt will delete it when done
}

void txFunctions::stopAndWait()
{
  kgstvRunToken.fetch_add(1);
  if(txState==TXSENDKGSTV || txState==TXPREPAREKGSTV || kgstvWorking.load()) {
    kgstvAbort.store(true);
    if(soundIOPtr)soundIOPtr->idleTX();
    if(kgstvControllerPtr)kgstvControllerPtr->cancelTransmit();
    while(kgstvWorking.load()) {qApp->processEvents(QEventLoop::ExcludeUserInputEvents);QThread::msleep(1);}
    switchTxState(TXIDLE);
  }

  stopDRM=true;
  if(txState==TXSENDTONE) synthesPtr->abortTone=true;
  // abort based on what is actually running, not on the (possibly already changed) mode index
  if(transmissionModeIndex==TRXSSTV || txState==TXPREPARESSTV || txState==TXSSTVIMAGE || txState==TXSSTVPOST)
    {
      sstvTxPtr->abort();
      switchTxState(TXRESTART);
      while(txState!=TXIDLE)
        {
          qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
        }
    }
  addToLog("txFunc: stop initiated",LOGTXFUNC);
  switchTxState(TXIDLE);
  while(txState!=TXIDLE)
    {
      qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    }
  synthesPtr->abortTone=false;
  if(soundIOPtr) soundIOPtr->idleTX();
  addToLog("txFunc: stop executed",LOGTXFUNC);
}


void txFunctions::sendTestPattern()
{
  syncBurst();
}


/** send a burst of syncs */
void txFunctions::syncBurst()
{
  // send 5msec 1200Hz followed by 50 msec silence
  //	synthesPtr->sendTone(0.005,1200.);
  for (int i=0;i<50;i++)
    {
      synthesPtr->sendTone(0.005,1200,0,true);
      synthesPtr->sendTone(0.020,1700,0,true);
    }
  synthesPtr->sendSilence(0.100);;
}


void txFunctions::sendCW()
{
  addToLog("txFunc:sendCW",LOGTXFUNC);
  float tone;
  float duration;
  initCW(cwText);
  synthesPtr->sendSilence(0.5);
  while(sendTextCW(tone,duration))
    {
      synthesPtr->sendTone(duration,tone,0,true);
    }
}


void txFunctions:: sendFSKChar(int IDChar)
{
  int TxBit;

  for (int i=0;i<6;i++)
    {
      TxBit = IDChar & 0x01;
      IDChar = IDChar >> 1;
      if (TxBit == 0x01)
        synthesPtr->sendTone(0.022,1900.,0,true);
      else
        synthesPtr->sendTone(0.022,2100.,0,true);
    }
}

// sendFSKID by DL3YAP






void txFunctions:: sendFSKID()
{
  int idx;
  int l;
  int IDChar;
  int Checksum;


  if (myCallsign.isEmpty()) return;
  // addToLog("txFunc:sendFSKID",LOGFSKID);

  l=myCallsign.size();
  idx=0;

  // synthesPtr->sendTone(2.0,00,0,false);
  synthesPtr->sendTone(0.3,1500.,0,false);
  synthesPtr->sendTone(0.1,2100.,0,true);
  synthesPtr->sendTone(0.022,1900.,0,true);

  IDChar = 0x2A;
  sendFSKChar(IDChar);

  QChar IDText=	QChar(myCallsign[idx]);
  IDChar=int(IDText.toLatin1());

  IDChar = (IDChar - 0x20);
  Checksum = IDChar;

  while (idx < l)
    {
      sendFSKChar(IDChar);
      idx++;

      if (idx < l)
        {
          QChar IDText=QChar(myCallsign[idx].toUpper());
          IDChar=int(IDText.toLatin1());
          IDChar = (IDChar - 0x20);
          Checksum = Checksum ^ IDChar;
        }
    }

  IDChar = 0x01;

  sendFSKChar(IDChar);

  IDChar = Checksum & 0x3F ;

  sendFSKChar(IDChar);
  synthesPtr->sendTone(0.1,2100.,0,true);
}

void txFunctions::sendBSR(QByteArray *p,drmTxParams dp)
{
  if(p==NULL) return;
  drmTxPtr->sendBSR(p,dp);
}


//void txFunctions::sendHybrid(QString fn)
//{
//  drmTxPtr->sendHybrid(fn);
//}

bool txFunctions::prepareFIX(QByteArray bsrByteArray)
{
  int trID;
  displayMBoxEvent *stce;
  txSession *sessionPtr;
  fixForm fx(mainWindowPtr);
  trID=drmTxPtr->processFIX(bsrByteArray);
  if(trID<0)  return false;
  sessionPtr=drmTxPtr->getSessionPtr((uint)trID);
  if (sessionPtr==NULL)
    {
      stce= new displayMBoxEvent("BSR Received","This BSR is not for you");
      QApplication::postEvent( dispatcherPtr, stce );  // Qt will delete it when done
      return false;
    }
  else
    {
      // take it from the transmitlist
      fx.setInfoInternal(paramsToMode(sessionPtr->drmParams),sessionPtr->filename,fixBlockList.count(),&sessionPtr->ba);
      if(fx.exec()==QDialog::Rejected) return false;
      drmTxPtr->initDRMFIX(sessionPtr);
    }
  return true;
}

bool txFunctions::prepareBinary(QString fileName)
{
  binaryFilename=fileName;
  return true;
}


void txFunctions::applyTemplate(imageViewer *ivPtr,QString templateFilename)
{
  if(transmissionModeIndex==TRXKGSTV) {ivPtr->setGrid(1,1);ivPtr->setParam(templateFilename,useTemplate,320,240);return;}
  if(transmissionModeIndex==TRXSSTV)
    {
      sstvTxPtr->applyTemplate(templateFilename,useTemplate,ivPtr);
    }
  else
    {
      drmTxPtr->applyTemplate(templateFilename,useTemplate,ivPtr);
    }
}

void txFunctions::forgetTxFileName()
{
  drmTxPtr->forgetTxFileName();
}

void txFunctions::switchTxState(etxState newState)
{
  addToLog(QString("%1 to %2").arg(txStateStr[txState]).arg(txStateStr[newState]),LOGTXFUNC);
  txState=newState;
}

void txFunctions::txTestPattern(imageViewer *ivPtr,etpSelect sel)
{
  sstvTxPtr->createTestPattern(ivPtr,sel);
}


QImage txFunctions::kgstvPreparedImage(){QMutexLocker lock(&txMutex);return kgstvImage;}

void txFunctions::setKgstvImage(const QImage &image,quint64 ticket)
{
  QMutexLocker lock(&txMutex);kgstvImage=image;kgstvTicket=ticket;
}

void txFunctions::sendKgstv()
{
  const quint64 token=kgstvRunToken.load();
  const double clock=txClock;
  QString error;
  bool ok=!kgstvAbort.load() && kgstvControllerPtr && kgstvControllerPtr->beginTransmit(&error,clock);
  if(!ok) {
    if(soundIOPtr)soundIOPtr->idleTX();
    if(kgstvControllerPtr){kgstvControllerPtr->completeTransmit(false);if(!error.isEmpty())emit kgstvControllerPtr->statusChanged(error);}
    QApplication::postEvent(dispatcherPtr,new endImageTXEvent(token));return;
  }
  double samples[TXSTRIPE]={};
  if(ok)QApplication::postEvent(dispatcherPtr,new progressTXEvent(kgstvControllerPtr->duration()+rigControllerPtr->getTxDelay()+SILENCEDELAY+(useVOX?1.0:0.0),token));
  int delay=int(rigControllerPtr->getTxDelay()*clock);
  while(ok && delay>0 && !kgstvAbort.load()) {
    const int count=qMin(delay,TXSTRIPE);ok=synthesPtr->writePcm(samples,count,kgstvAbort);delay-=count;
  }
  int vox=useVOX?int(clock):0;double phase=0;
  while(ok && vox>0 && !kgstvAbort.load()) {
    const int count=qMin(vox,TXSTRIPE);
    for(int i=0;i<count;++i){phase+=2*M_PI*1700/clock;if(phase>=2*M_PI)phase-=2*M_PI;samples[i]=0.7*std::sin(phase);}
    ok=synthesPtr->writePcm(samples,count,kgstvAbort);vox-=count;
  }
  while(ok && !kgstvAbort.load()) {
    const int count=kgstvControllerPtr->render(samples,TXSTRIPE,&error);
    if(!error.isEmpty()){ok=false;break;}
    if(count==0)break;
    ok=synthesPtr->writePcm(samples,count,kgstvAbort);
  }
  std::fill(samples,samples+TXSTRIPE,0.0);
  int silence=int(SILENCEDELAY*clock);
  while(ok && silence>0 && !kgstvAbort.load()) {
    const int count=qMin(silence,TXSTRIPE);ok=synthesPtr->writePcm(samples,count,kgstvAbort);silence-=count;
  }
  soundBase::txFileDone=true; // file output must know no further PCM is coming
  QElapsedTimer deadline;deadline.start();
  while(ok && soundIOPtr->isPlaying() && !kgstvAbort.load()) {
    if(deadline.elapsed()>10000){ok=false;error="KG-STV audio drain timed out.";break;}
    QThread::msleep(2);
  }
  if(ok && !kgstvAbort.load() && soundIOPtr->txBuffer.count()!=0){ok=false;error="KG-STV playback ended before the output buffer drained.";}
  ok=ok && !kgstvAbort.load();
  if(!ok && soundIOPtr)soundIOPtr->idleTX();
  if(kgstvControllerPtr) {
    kgstvControllerPtr->completeTransmit(ok);
    if(!error.isEmpty())emit kgstvControllerPtr->statusChanged(error);
  }
  QApplication::postEvent(dispatcherPtr,new endImageTXEvent(token));
}
