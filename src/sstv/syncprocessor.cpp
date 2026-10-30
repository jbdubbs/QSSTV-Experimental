#include "syncprocessor.h"
#include "linematch.h"
#include <QApplication>
#include <algorithm>
#include <cmath>
#include "appglobal.h"
#include "sstvparam.h"
#include "configparams.h"
#include "dispatchevents.h"
#include "dispatcher.h"
#include "modes/modes.h"
#include "rxtrace.h"

#ifndef QT_NO_DEBUG
#include "scope/scopeview.h"
#endif

#define FILTERDELAYCORRECTION  +9
//#define MINVOLUME 80
#define MINSYNCWIDTHRATIO 0.8
#define MAXSYNCWIDTHRATIO 2


#define NUMBEROFSENSITIVITIES 4
#define OUTOFSYNCEXTRALINES 4 // lines after the expected end of the picture that are still waited for (the end is an estimate)

// What noise can and cannot do to a picture it started (issue #135). On a busy band 15 or so pulses a second pass the width test,
// and now and then four of them fall on the line grid of some mode and start a picture. A pulse at a whole number of lines from
// the last one is then found again by chance every few seconds, so a picture of noise must not be kept alive by such pulses.
#define NEARLINES 2           // a pulse at most this many lines after the last one: the picture is being followed
#define TRUSTEDLINES 4        // ... at most this many lines after the last one: it restarts the wait for sync
#define PROVENPULSES 12       // near pulses after which a picture that did not start from a VIS code has proven itself
#define RESYNCPULSES 3        // near pulses in a row that wake a frozen display
#define VISFRESHSECONDS 10    // a VIS code at most this old started the picture
#define SLANTMINPOINTS 8      // the clock is only corrected from a regression over at least this many pulses ...
#define SLANTMINLINES 16      // ... that span at least this many lines (a few noisy pulses give a slope that is wrong by a good fraction of a percent)
#define VISSTAYMS 3           // a picture that runs by its VIS code moves to the position of its pulses only if they are this far (ms) from it
#define VISADOPTMS 12         // a chain of pulses is adopted by a picture that runs by its VIS code only if its line grid is this close (ms) to the one of the code
#define VISSTOPBITMS 40       // a retrace pulse that ends this soon after the first line of a picture that a VIS code started is the stop bit of the code
#define TRAINTOLERANCEMS 3    // an event of the sync train detector counts as a pulse of the picture if it is this close (ms) to the line grid ...
#define TRAINDRIFT 0.0005     // ... plus this share of the distance from the reference (a clock that is a little off)
#define TRAINSMOOTH 0.25      // how much of the distance of such an event moves the reference of the grid
#define REANCHORENTRIES 4     // a pulse that does not fit the last pulse of the chain is tried against this many of the ones before it

// A VIS code says where the picture starts, even when the first lines are in the noise and the sync detector cannot follow them
// yet (the lines before the first chain of pulses were cut off the top of the picture).
#define VISBACKFILLSECONDS 30     // a VIS code at most this old still counts; an older one is forgotten
#define VISTOSTARTMS 16           // the first line starts this long before the end of the code (the same in every mode, measured)
#define VISBACKFILLTOLERANCE 0.25 // the chain's line grid may be this far from the one the code gives (a fraction of a line) ...
#define VISBACKFILLMAXMS 50       // ... and this far (ms), whichever is less



// onRatio/offRatio: a sync pulse starts when the 1200 Hz energy exceeds onRatio times the input level and ends when it falls under
// offRatio times it. In noise the pulse sinks in and out of the level, so lower ratios and a wider line tolerance find it more often.
// Measured on pictures at 3 to -6 dB SNR and on an hour of real band noise (tests/sensitivity, 2026-10-29): High finds the pulses of a
// -2 dB picture on 7 of 9 runs where 0.5/0.3 and 0.008 found 3, and noise kept the receiver busy for 80 s of the hour where it took
// 203 s (an off ratio of 0.2 or a wider pulse width window lost pictures or started many more on noise). Low keeps the old values.
ssenitivity sensitivityArray[NUMBEROFSENSITIVITIES]=
{
  // minMatchedLines maxLineDistanceModeDetect onRatio offRatio minVolume maxTempOutOfSyncLines maxOutOfSyncLines minOutOfSyncSeconds lineTolerance trainLevel;
  {         4,                2,                0.5,    0.3,   2000,        10, 15, 10, 0.008,  0},
  {         4,                2,                0.45,   0.3,   1000,        10, 20, 20, 0.008,  0.35},
  {         4,                2,                0.38,   0.25,  100,         15, 30, 30, 0.0095, 0.30},
  {         4,                2,                0.38,   0.25,  500,         15, 30,  0, 0.0095, 0.30}
};




const QString syncStateStr[syncProcessor::SYNCVALID+1]=
{
  "SYNCOFF",
  "SYNCUP",
  "SYNCVALID"
};

const QString stateStr[syncProcessor::RETRACEWAIT+1]=
{
  "Mode detect",
  "In Sync",
  "Sync Lost New Mode",
  "Sync Lost False syncs",
  "Sync Lost Missing Lines",
  "Sync Lost",
  "Retrace Wait"
};

static QString traceTime(quint32 counter)
{
  return QString("t=%1").arg(counter/(double)SAMPLERATE,0,'f',2);
}

/*!
 * \brief syncProcessor::syncProcessor constructor
 *
 * Process the received sync pulses. When a sync pulse is detected, the samplecounter reference is stored in the syncArray.
 * Concurrent with this processing, the VIS decoding and FSKid decoding is done.
 *
 * \param narrow if true then this instance will use narrow settings, else wide settings
 * \param parent
 */



syncProcessor::syncProcessor(bool narrow, QObject *parent) :  QObject(parent),streamDecode(narrow)
{
  detectNarrow=narrow;
  audioPtr=NULL;
  currentModePtr=NULL;
  visMode=NOTVALID;
  trDetectCounter=trFreezeCounter=0;
  slantPoints=0;
  slantSpanLines=0;
  slantOffset0=0;
  clockAdjusted=false;
  nearHits=nearRun=0;
  proven=false;
  trustedEnd=0;
  trViaVis=false;
  trViaTrain=false;
  pendingTrain=false;
  trainRef=trainDelayEnd=trainWidth=0;
  trTrainPulses=0;
  visEndCounter=0;
  visBackfillLines=0;
  visAnchored=false;
  pendingVisStart=false;
  visRestart.valid=false;
  trHits=trFarHits=trFreezes=trResyncs=trMaxGapLines=0;
  trStripes=trStarts=trAccepted=trRetrace=trVis=trGateOpen=0;
  trLevelSum=0;
  trLevelPeak=0;
  maxLineSamples=getMaxLineSamples();
  //  syncFound=false;
  if(!detectNarrow)
  {
    connect(streamDecode.getFskDecoderPtr(),SIGNAL(callReceived(QString)),SLOT(slotNewCall(QString)));
    connect(streamDecode.getToneVisPtr(),SIGNAL(visCodeWideDetected(int,uint)),SLOT(slotVisCodeDetected(int,uint)));
  }
  else
  {
    connect(streamDecode.getVisDecoderPtr(),SIGNAL(visCodeNarrowDetected(int,uint)),SLOT(slotVisCodeDetected(int,uint)));
  }
}

syncProcessor::~syncProcessor()
{
  int i;
  for(i=0;i<=ENDNARROW;i++)
  {
    matchArray[i].clear();
  }
  if(currentModePtr!=NULL) delete currentModePtr;
}

void syncProcessor::reset()
{
  sampleCounter=0;
  init();
  streamDecode.reset();
  train.reset();

#ifndef QT_NO_DEBUG
  scopeViewerSyncNarrow->setCurveName("SYNC VOL",SCDATA1);
  scopeViewerSyncNarrow->setCurveName("INPUT VOL",SCDATA2);
  scopeViewerSyncNarrow->setCurveName("SYNC STATE",SCDATA3);
  scopeViewerSyncNarrow->setCurveName("FREQ",SCDATA4);
  scopeViewerSyncNarrow->setAxisTitles("Samples","int","State");

  scopeViewerSyncWide->setCurveName("SYNC VOL",SCDATA1);
  scopeViewerSyncWide->setCurveName("INPUT VOL",SCDATA2);
  scopeViewerSyncWide->setCurveName("SYNC STATE",SCDATA3);
  scopeViewerSyncWide->setCurveName("FREQ",SCDATA4);
  scopeViewerSyncWide->setAxisTitles("Samples","int","State");
#endif

}

/**
 * @brief syncProcessor::selectedModes the modes the receiver is set to: the narrow or the wide ones in Auto mode, else the selected mode
 *
 * idxStart and idxEnd narrow down to the mode of a VIS code while a picture runs, this is what a new code is checked against.
 */

void syncProcessor::selectedModes(esstvMode &first,esstvMode &last) const
{
  if(sstvModeIndexRx!=0)
  {
    first=last=(esstvMode)(sstvModeIndexRx-1);
  }
  else if(detectNarrow)
  {
    first=STARTNARROW;
    last=ENDNARROW;
  }
  else
  {
    first=STARTWIDE;
    last=ENDWIDE;
  }
}

/**
 * @brief initialize sync processor
 * idxStart and idxEnd are set for narrow or wide modes in Auto mode, else idxStart=idxEnd= selected mode
 * enableSyncDetection is set to false for AVT modes.
 *
 */
void syncProcessor::init()
{
  enableSyncDetection=true;
  //  syncAvg=0;

  selectedModes(idxStart,idxEnd);
  if((sstvModeIndexRx!=0) && (idxStart>=AVT24)&&(idxStart<=AVT94))
  {
    enableSyncDetection=false;
  }
  visMode=NOTVALID; // and reset the visMode
  syncProcesState=MODEDETECT;
  modifiedClock=rxClock/SUBSAMPLINGFACTOR;
  if(!detectNarrow) train.setClock(modifiedClock);
  syncArrayIndex=0;
  syncArray[0].init();
  retraceFlag=false;
  syncState=SYNCOFF;
  displaySyncEvent* ce;
  ce = new displaySyncEvent(0); // reset sync vuMeter
  QApplication::postEvent(dispatcherPtr, ce);
  addToLog("init called",LOGSYNCSTATE);
  clearMatchArray();
  currentMode=NOTVALID;
  lineTolerance=sensitivityArray[sensitivity].lineTolerance;
  minMatchedLines=sensitivityArray[sensitivity].minMatchedLines;
  visAnchored=false;
  pendingVisStart=false;
  pendingTrain=false;
  if(visRestart.valid && !detectNarrow && visRestart.mode>=idxStart && visRestart.mode<=idxEnd)
  {
    // a VIS code came while the last picture was still running: it is the start of this one
    visMode=visRestart.mode;
    idxStart=idxEnd=visMode;
    minMatchedLines=3;
    visEndCounter=visRestart.counter;
    pendingVisStart=true;
  }
  visRestart.valid=false;
}

/**
 * @brief process the data buffer to extract syncs
 *
 *
 * It calls extract sync and the streamDecode to extarct FSKid and VIS Code
 *
 */


void syncProcessor::process()
{
  streamDecode.process(freqPtr,audioPtr,sampleCounter); // only VIS and FSK if not narrow
  if(!detectNarrow) trainStep();
  extractSync();
  if(rxTraceOn()) traceStats();

#ifndef QT_NO_DEBUG
  if(detectNarrow)
  {
    scopeViewerSyncNarrow->addData(SCDATA1,syncVolumePtr,sampleCounter,RXSTRIPE);
    //      scopeViewerSyncNarrow->addData(SCDATA2,inputVolumePtr,sampleCounter,RXSTRIPE);
    scopeViewerSyncNarrow->addData(SCDATA2,inputVolumePtr,sampleCounter,RXSTRIPE);
    scopeViewerSyncNarrow->addData(SCDATA3,syncStateBuffer,sampleCounter,RXSTRIPE);
    scopeViewerSyncNarrow->addData(SCDATA4,freqPtr,sampleCounter,RXSTRIPE);
  }
  else
  {
    scopeViewerSyncWide->addData(SCDATA1,syncVolumePtr,sampleCounter,RXSTRIPE);
    //      scopeViewerSyncWide->addData(SCDATA2,syncAvgPtr,sampleCounter,RXSTRIPE);
    scopeViewerSyncWide->addData(SCDATA2,inputVolumePtr,sampleCounter,RXSTRIPE);
    scopeViewerSyncWide->addData(SCDATA3,syncStateBuffer,sampleCounter,RXSTRIPE);
    scopeViewerSyncWide->addData(SCDATA4,freqPtr,sampleCounter,RXSTRIPE);
  }
#endif
}


/**
 * @brief syncProcessor::trainStep look for a picture by the rhythm of its sync pulses (syncTrainDetector)
 *
 * Runs on the audio of every stripe (the events must be followed all the time), reports a train only while the receiver is
 * hunting for a picture.
 */

void syncProcessor::trainStep()
{
  if(audioPtr==NULL) return;
  // QSSTV_TRAIN_LEVEL=<level> in the environment replaces the level of the Sensitivity setting; 0 switches the detector off
  static const bool levelSet=qEnvironmentVariableIsSet("QSSTV_TRAIN_LEVEL");
  static const double levelEnv=qEnvironmentVariable("QSSTV_TRAIN_LEVEL").toDouble();
  double level=levelSet ? levelEnv : sensitivityArray[sensitivity].trainLevel;
  bool armed=(syncProcesState==MODEDETECT) && enableSyncDetection && !pendingVisStart;
  bool found=train.process(audioPtr,sampleCounter,idxStart,idxEnd,level,armed);
  if(rxTracePulses()) for(uint k=0;k<train.freshCount();k++) rxTrace("TPEAK",QString("pos=%1").arg(train.fresh(k)));
  if(trViaTrain && syncProcesState==INSYNC)
  {
    for(uint k=0;k<train.freshCount();k++) trainPulse(train.fresh(k));
  }
  if(!found) return;
  const strainResult &r=train.result();
  if(armed)
  {
    pendingTrain=true;
    trainFound=r;
  }
  if(rxTraceOn())
  {
    QString peaks,others;
    for(uint k=0;k<r.count;k++) peaks+=QString(k?",%1":"%1").arg(r.peak[k]/(double)SAMPLERATE,0,'f',3);
    for(uint k=0;k<r.ambiguous;k++) others+=QString(k?",%1":"%1").arg(getSSTVModeNameShort(r.other[k]));
    rxTrace("TRAIN",QString("%1\tproc=W\tmode=%2\tpeaks=%3\tpeak_t=%4\tambiguous=%5\tstate=%6")
            .arg(traceTime(sampleCounter)).arg(getSSTVModeNameShort(r.mode)).arg(r.count).arg(peaks).arg(others.isEmpty()?QString("-"):others).arg(stateStr[syncProcesState]));
  }
}

/**
 * @brief extract the syncs from the data
 *
 * The function fills in the syncArray en calls validateSync.
 * If syncProcesState==INSYNC then the out of sync is tracked
 * The last check is to see if we received a valid VIS code
 *
 */

void syncProcessor::extractSync()
{
  int i;
  int lastSync;
  for(i=0;i<RXSTRIPE;i++)
  {
    switch(syncState)
    {
    case SYNCVALID:
    case SYNCOFF:
      if(inputVolumePtr[i]<sensitivityArray[sensitivity].minVolume) break;
      if(syncVolumePtr[i]>sensitivityArray[sensitivity].onRatio*inputVolumePtr[i])
      {
        syncArray[syncArrayIndex].start=sampleCounter+i;
        syncArray[syncArrayIndex].startVolume=syncVolumePtr[i];
        trStarts++;
        switchSyncState(SYNCACTIVE,sampleCounter+i);
      }
      break;
    case SYNCACTIVE:
      if(inputVolumePtr[i]<sensitivityArray[sensitivity].minVolume)
      {
        switchSyncState(SYNCOFF,sampleCounter+i);
        break;
      }
      if(syncVolumePtr[i]<sensitivityArray[sensitivity].offRatio*inputVolumePtr[i])
      {
        syncArray[syncArrayIndex].end=sampleCounter+i;
        if(validateSync())
        {
          trAccepted++;
          switchSyncState(SYNCVALID,sampleCounter+i);
        }
        else
        {
          switchSyncState(SYNCOFF,sampleCounter+i);
        }
      }
      break;
    }
#ifndef QT_NO_DEBUG
    syncStateBuffer[i]=(unsigned char)syncState*STATESCALER;
#endif
  }

  if((syncProcesState==INSYNC) && (enableSyncDetection) && visAnchored)
  {
    // started by a VIS code: runs by the clock to the end of the picture, whatever the pulses do (sstvRx ends it at its last line)
    missingLines=0;
    syncQuality=10;
  }
  else if((syncProcesState==INSYNC) && (enableSyncDetection))
  {
    if(activeChainPtr==NULL)
    {
      if(rxTraceOn()) traceLost("nochain");
      switchProcessState(SYNCLOST);
    }
    else
    {
      lastSync=trustedEnd;
      if((sampleCounter+RXSTRIPE-RXSTRIPE/7)>(lastSync+sensitivityArray[sensitivity].maxTempOutOfSyncLines*samplesPerLine))
      {
        if(sensitivity!=(NUMBEROFSENSITIVITIES-1)) // no temp out of sync if DX
        {
          if(!tempOutOfSync)
          {
            trFreezes++;
            trFreezeCounter=sampleCounter;
            nearRun=0;
            if(rxTraceOn()) rxTrace("FREEZE",QString("%1\tproc=%2\tmode=%3\tlines=%4\tchain=%5").arg(traceTime(sampleCounter)).arg(detectNarrow?"N":"W").arg(getSSTVModeNameShort(currentMode)).arg(currentModePtr->receivedLines()).arg(activeChainPtr->count()));
          }
          tempOutOfSync=true;
        }
      }
      missingLines=(uint) round(((sampleCounter+RXSTRIPE-RXSTRIPE/7)-(lastSync+samplesPerLine))/samplesPerLine+1);
      calcSyncQuality();
    }
  }
  if(pendingVisStart && syncProcesState==MODEDETECT) startFromVis();
  if(pendingTrain)
  {
    if(syncProcesState==MODEDETECT && !pendingVisStart && enableSyncDetection) startFromTrain();
    pendingTrain=false;
  }
  if((visMode>=AVT24)&&(visMode<=AVT94))
  {
    currentMode=visMode;
    enableSyncDetection=false;
    createModeBase();
    visMode=NOTVALID;
    switchProcessState(INSYNC);
  }
  else if((visMode!=NOTVALID) && (syncProcesState==MODEDETECT) && ((int)sampleCounter-(int)visEndCounter)>VISBACKFILLSECONDS*SAMPLERATE)
  {
    // a VIS code that no picture followed (noise can make one): the receiver was held to its mode and to short chains, and
    // would not hear a picture of another mode
    if(rxTraceOn()) rxTrace("VISEXPIRE",QString("%1\tproc=%2\tmode=%3").arg(traceTime(sampleCounter)).arg(detectNarrow?"N":"W").arg(getSSTVModeNameShort(visMode)));
    init();
  }
}

/**
 * @brief validate sync and build up syncArray
 *
 * The function evaluates the sync pulse width and sets the retrace flag
 * *
 * \retval false if no valid sync and no further processing is needed
 *         true if a new syncArray entry is added and further processing is necessary
 */


bool syncProcessor::validateSync()
{
  bool result;
  if(syncArray[syncArrayIndex].diffStartEnd()>=MINRETRACEWIDTH)
  {
    syncArray[syncArrayIndex].retrace=true; // simply set retrace true
    result=true;
  }
  else if(syncArray[syncArrayIndex].diffStartEnd()>=MINRETRACEWIDTH/4)
  {
    if((syncArrayIndex>2)
       && (syncArray[syncArrayIndex].end-syncArray[syncArrayIndex-1].start)>=MINRETRACEWIDTH
       && (syncArray[syncArrayIndex].start-syncArray[syncArrayIndex-1].end)<=MINRETRACEWIDTH/5)
    {
      syncArray[syncArrayIndex-1].end=syncArray[syncArrayIndex].end;
      syncArray[syncArrayIndex-1].retrace=true;
      syncArray[syncArrayIndex-1].diffStartEnd(); // just calculate the width;
      syncArrayIndex--;
    }
    result=true;
  }
  else if((syncArray[syncArrayIndex].diffStartEnd()>=0.004*SAMPLERATE) && (syncArray[syncArrayIndex].diffStartEnd()<=0.025*SAMPLERATE))
  {
    syncArray[syncArrayIndex].retrace=false;
    //      addToLog(QString("index %1, mid:=%2").arg(syncArrayIndex).arg(syncArray[syncArrayIndex].mid),LOGSYNCACCEPTED);
    result=true;
  }
  else
  {
    //      addToLog(QString("Sync rejected:%1 end:%2 width %3").arg(syncArray[syncArrayIndex].start)
    //               .arg(syncArray[syncArrayIndex].end).arg(syncArray[syncArrayIndex].diffStartEnd()),LOGSYNCREJECTED);
    result=false;
  }

  if(result)
  {
    if(rxTracePulses() && !syncArray[syncArrayIndex].retrace) rxTrace("PULSE",QString("c=%1\tstart=%2\tend=%3").arg(sampleCounter).arg(syncArray[syncArrayIndex].start).arg(syncArray[syncArrayIndex].end));
    checkSyncArray();
    switch(syncProcesState)
    {
    case MODEDETECT:
      if(findMatch())
      {
        if(!beginPicture(false)) result=false;
      }
      break;
    case INSYNC:
      if(enableSyncDetection)
      {
        if(visAnchored && activeChainPtr==NULL) adoptChain();
        else trackSyncs();
      }
      break;
    case SYNCLOSTNEWMODE:
    case SYNCLOSTFALSESYNC:
    case SYNCLOSTMISSINGLINES:
    case SYNCLOST:
      addToLog("synclost detected",LOGSYNCSTATE);
      break;
    case RETRACEWAIT:
      break;

    }
    syncArrayIndex++;
    syncArray[syncArrayIndex].init();
  }
  return result;
}


/**
 * @brief syncProcessor::beginPicture the receiver has found a picture: set it up and switch to INSYNC
 * @param viaTrain the chain of pulses in activeChainPtr (with the first one in syncArray[0]) was made from the events of the
 *        syncTrainDetector, not found by the pulse detector
 * @return false if the mode could not be set up
 */

bool syncProcessor::beginPicture(bool viaTrain)
{
  trViaVis=(!viaTrain) && (visMode!=NOTVALID);
  trViaTrain=viaTrain;
  visMode=NOTVALID; //reset visMode;
  //we have a new mode
  if(!createModeBase())
  {
    addToLog("Error creating modeBase",LOGALL);
    return false;
  }
  falseSyncs=0;
  lineTolerance=sensitivityArray[sensitivity].lineTolerance;
  // when we have S1,S2 or SDX then we have to set the syncposition at the beginning of the green line
  //          syncPosition=currentModePtr->adjustSyncPosition(syncArray[0].end)- FILTERDELAYCORRECTION; // type 1 sync end
  unsigned int syncCorrected;;
  syncWidth=getSyncWidth(currentMode ,modifiedClock);

  if(syncArray[0].retrace)
  {
    syncCorrected=syncArray[0].end;
  }
  else
  {
    syncCorrected=(syncArray[0].start+syncArray[0].end)/2+syncWidth/2;
  }

  syncPosition=currentModePtr->adjustSyncPosition(syncCorrected,syncArray[0].retrace)+FILTERDELAYCORRECTION; // type 2 sync end
  tempOutOfSync=false;
  slantAdjustLine=6;
  clockAdjusted=false;
  visBackfillLines=0;
  slantAdjust(true);
  visBackfillLines=visBackfill();
  if(visBackfillLines) syncPosition-=(quint32)round(visBackfillLines*samplesPerLine);
  switchProcessState(INSYNC);
  trDetectCounter=sampleCounter;
  trHits=trFarHits=trFreezes=trResyncs=trMaxGapLines=0;
  trTrainPulses=0;
  nearHits=nearRun=0;
  proven=trViaVis && ((int)sampleCounter-(int)visEndCounter)>=0 && ((int)sampleCounter-(int)visEndCounter)<=VISFRESHSECONDS*SAMPLERATE;
  trustedEnd=syncArray[activeChainPtr->last()->to].end;
  if(rxTraceOn()) rxTrace("DETECT",QString("%1\tproc=%2\tmode=%3\tvia=%4\tvis_age_s=%5\tchain=%6\tfract=%7\tline_ms=%8\tsens=%9\tmin_volume=%10\tvis_to_start_ms=%11\tbackfill_lines=%12")
                          .arg(traceTime(sampleCounter)).arg(detectNarrow?"N":"W").arg(getSSTVModeNameShort(currentMode)).arg(viaTrain?"train":(trViaVis?"vis":"sync"))
                          .arg(trViaVis?QString::number(((int)sampleCounter-(int)visEndCounter)/(double)SAMPLERATE,'f',1):QString("-")).arg(activeChainPtr->count())
                          .arg(calcTotalFract(activeChainPtr),0,'f',4).arg(samplesPerLine*1000./SAMPLERATE,0,'f',1).arg(sensitivity).arg(sensitivityArray[sensitivity].minVolume)
                          .arg(trViaVis?QString::number(((int)syncPosition+(int)round(visBackfillLines*samplesPerLine)-(int)visEndCounter)*1000./SAMPLERATE,'f',1):QString("-")).arg(visBackfillLines));
  addToLog(QString("Mode detected: %1, sync at %2").arg(getSSTVModeNameShort(currentMode)).arg(syncPosition),LOGSYNCEVAL);
  return true;
}

/**
 * @brief syncProcessor::startFromTrain start the picture a train of sync events (syncTrainDetector) announced
 *
 * The events are where the sync pulses of the lines are (the first, back-filled one is the first line of the picture, as the first pulse
 * of a chain is). Pulses of the sync processor are made of them: on the line grid that fits the events best (their mean phase), at the
 * place the sync processor finds the pulse of a clean signal (syncTrainDetector::pulseDelayMs), and chained as a chain of pulses is. The
 * picture is then set up and followed as any picture a chain started: it has not proven itself, and the pulses that come refine the clock
 * and the position, or it is given up (maxOutOfSyncLines).
 */

void syncProcessor::startFromTrain()
{
  pendingTrain=false;
  const strainResult r=trainFound;
  if(syncProcesState!=MODEDETECT || r.count<2 || r.mode<idxStart || r.mode>idxEnd) return;
  esstvMode mode=r.mode;
  modifiedClock=rxClock/SUBSAMPLINGFACTOR;
  double T=getLineLength(mode,modifiedClock);
  double W=getSyncWidth(mode,modifiedClock);
  double delayEnd,delayCorr;
  syncTrainDetector::pulseDelayMs(mode,delayEnd,delayCorr);
  delayEnd*=SAMPLERATE/1000.;
  delayCorr*=SAMPLERATE/1000.;
  // the pulse in progress (SYNCACTIVE) keeps its start: its end comes after this stripe
  bool active=(syncState==SYNCACTIVE);
  uint activeStart=syncArray[syncArrayIndex].start;
  uint activeVolume=syncArray[syncArrayIndex].startVolume;
  currentMode=mode;
  samplesPerLine=T;
  dropChains();
  int lines[STMAXPEAKS];
  double phase=0;
  for(uint k=0;k<r.count;k++)
  {
    lines[k]=(int)round((r.peak[k]-r.peak[0])/T);
    phase+=r.peak[k]-lines[k]*T;
  }
  phase/=r.count;
  double width=W+2*(delayEnd-delayCorr);           // a pulse of this width, ending delayEnd after the event, has its point at delayCorr
  if(width<W*MINSYNCWIDTHRATIO*1.01) width=W*MINSYNCWIDTHRATIO*1.01;
  if(width>W*MAXSYNCWIDTHRATIO*0.99) width=W*MAXSYNCWIDTHRATIO*0.99;
  trainDelayEnd=delayEnd;
  trainWidth=width;
  trainRef=phase+lines[r.count-1]*T+delayEnd;
  for(uint k=0;k<r.count;k++)
  {
    syncArray[k].init();
    syncArray[k].end=(uint)round(phase+lines[k]*T+delayEnd);
    syncArray[k].start=syncArray[k].end-(uint)round(width);
    syncArray[k].diffStartEnd();
    syncArray[k].inUse=true;
  }
  modeMatchList *chain=new modeMatchList;
  matchArray[mode].append(chain);
  for(uint k=0;k+1<r.count;k++)
  {
    int spacing=lines[k+1]-lines[k];
    double fraction=fabs((double)((int)syncArray[k+1].end-(int)syncArray[k].end)-spacing*T)/T;
    chain->append(new smatchEntry(k,k+1,spacing,fraction,syncArray[k].end,syncArray[k+1].end));
  }
  activeChainPtr=chain;
  syncArrayIndex=r.count-1;
  for(uint k=0;k<syncArrayIndex;k++) calculateLineNumber(k,k+1);
  bool ok=beginPicture(true);
  if(ok) syncArrayIndex++;
  else
  {
    // (the mode could not be set up: nothing of the train is kept)
    dropChains();
    currentMode=NOTVALID;
    syncArrayIndex=0;
  }
  syncArray[syncArrayIndex].init();
  if(active)
  {
    syncArray[syncArrayIndex].start=activeStart;
    syncArray[syncArrayIndex].startVolume=activeVolume;
  }
}

/**
 * @brief syncProcessor::reanchorChain the new pulse does not follow the last pulse of the chain: try the ones before it
 * @return true if the new pulse was appended to the chain, after the entries behind the pulse it fits were dropped
 *
 * Every pulse is only compared with the one before it, so a pulse that falls on the line grid by chance (or whose edge noise pushed
 * late) is accepted, and then the real pulses, which are on the grid, no longer fit it: nothing is added to the chain until the picture
 * is given up. The wider line tolerances of the sensitivity table make that happen more often. Such a pulse is the last entry, or one of
 * the last few: the new pulse is tried against the pulses before it and the entries after the one it fits are taken off the chain.
 */

bool syncProcessor::reanchorChain()
{
  for(int back=1;back<=REANCHORENTRIES && activeChainPtr->count()>back;back++)
  {
    int keep=activeChainPtr->count()-back;                 // entries that stay; the new pulse follows the last of them
    quint16 lines;
    double fraction;
    if(!lineCompare(samplesPerLine,activeChainPtr->at(keep-1)->to,syncArrayIndex,lines,fraction)) continue;
    QList<smatchEntry *> removed;
    while(activeChainPtr->count()>keep) removed.prepend(activeChainPtr->takeLast());
    if(addToChain(currentMode,activeChainPtr->last()->to))
    {
      if(rxTraceOn()) rxTrace("REANCHOR",QString("%1\tproc=%2\tmode=%3\tdropped=%4").arg(traceTime(sampleCounter)).arg(detectNarrow?"N":"W").arg(getSSTVModeNameShort(currentMode)).arg(removed.count()));
      qDeleteAll(removed);
      return true;
    }
    for(smatchEntry *e:removed) activeChainPtr->append(e);    // the width test failed: nothing changes
  }
  return false;
}

/**
 * @brief syncProcessor::trainPulse an event of the sync train detector during a picture that a train started
 * @param peak position of the event in the sample counter
 *
 * With the pulse detector a weak picture is followed on one line in five or ten, and an unproven picture is given up after
 * maxOutOfSyncLines lines without a pulse. The events are there on most lines. An event that falls on the line grid of the picture
 * (within the tolerance of the detector) is a pulse like one the pulse detector found. The grid is a smoothed reference, so that
 * an event of noise that was let through moves it only a little and the real ones are not compared with it.
 */

void syncProcessor::trainPulse(int peak)
{
  if(syncProcesState!=INSYNC || !trViaTrain || !enableSyncDetection || activeChainPtr==NULL || activeChainPtr->count()==0) return;
  // the sync array is full: the oldest entry goes (and with it, if it was the only one, the chain)
  if(syncArrayIndex>=(MAXSYNCENTRIES-2))
  {
    dropTop();
    if(activeChainPtr==NULL || activeChainPtr->count()==0) return;
  }
  double c=peak+trainDelayEnd;
  double lastEnd=syncArray[activeChainPtr->last()->to].end;
  if(c-lastEnd<samplesPerLine/2) return;                 // the line of the last pulse (the pulse detector found it as well)
  double n=round((c-trainRef)/samplesPerLine);
  if(n<1) return;
  double resid=c-(trainRef+n*samplesPerLine);
  double tolerance=TRAINTOLERANCEMS*SAMPLERATE/1000.+TRAINDRIFT*(c-trainRef);
  if(fabs(resid)>tolerance) return;
  trainRef+=n*samplesPerLine+TRAINSMOOTH*resid;
  // the pulse goes into the sync array in front of the one in progress (its end is still to come)
  bool active=(syncState==SYNCACTIVE);
  uint activeStart=syncArray[syncArrayIndex].start;
  uint activeVolume=syncArray[syncArrayIndex].startVolume;
  uint idx=syncArrayIndex;
  syncArray[idx].init();
  syncArray[idx].end=(uint)round(c);
  syncArray[idx].start=syncArray[idx].end-(uint)round(trainWidth);
  syncArray[idx].diffStartEnd();
  syncArray[idx].inUse=true;
  uint from=activeChainPtr->last()->to;
  double delta=syncArray[idx].end-lastEnd;
  int spacing=(int)round(delta/samplesPerLine);
  if(spacing<1) spacing=1;
  double fraction=fabs(resid)/samplesPerLine;
  activeChainPtr->append(new smatchEntry(from,idx,spacing,fraction,syncArray[from].end,syncArray[idx].end));
  currentModeMatchChanged=true;
  syncArrayIndex++;
  syncArray[syncArrayIndex].init();
  if(active)
  {
    syncArray[syncArrayIndex].start=activeStart;
    syncArray[syncArrayIndex].startVolume=activeVolume;
  }
  trTrainPulses++;
  chainUpdated(true);
}

void  syncProcessor::trackSyncs()
{
  if(activeChainPtr==NULL)
  {
    return;
  }
  if(activeChainPtr->count()==0)
  {
    return;
  }
  bool added=addToChain(currentMode,activeChainPtr->last()->to);
  if(!added && proven) added=reanchorChain();      // only a picture that has proven itself: a chain of noise must not live on by it
  chainUpdated(added);
}

/**
 * @brief syncProcessor::chainUpdated what the picture that is being followed does with a pulse that was (or was not) added to its chain
 *
 * Hits, the proof, the wait for the next pulse, the clock; a pulse of the pulse detector and an event of the sync train detector
 * (trainPulse) are the same to it.
 */

void syncProcessor::chainUpdated(bool added)
{
  if(added)
  {
    uint spacing=activeChainPtr->last()->lineSpacing;
    uint missedBefore=missingLines;
    bool wasFrozen=tempOutOfSync;
    bool resynced=false;
    int hitEnd=syncArray[activeChainPtr->last()->to].end;
    falseSyncs=0;
    missingLines=0;   // (the wait is counted from trustedEnd, see extractSync)
    if(spacing<=NEARLINES)
    {
      nearHits++;
      nearRun++;
      if(nearHits>=PROVENPULSES) proven=true;
    }
    else
    {
      nearRun=0;
    }
    if(tempOutOfSync && nearRun>=RESYNCPULSES)
    {
      // check if we can resynchronize: a signal that is back gives a pulse on every line, a pulse that fits by chance does not
      tempOutOfSync=false;
      resynced=true;
    }
    // a pulse far from the last one, or one while the display waits for the signal, does not restart the wait for sync until it has been confirmed
    if(resynced || (!wasFrozen && spacing<=TRUSTEDLINES)) trustedEnd=hitEnd;
    trHits++;
    if(spacing>trMaxGapLines) trMaxGapLines=spacing;
    if(spacing>2) trFarHits++;
    if(resynced) trResyncs++;
    if(rxTraceOn())
    {
      if(spacing>2) rxTrace("HIT",QString("%1\tproc=%2\tmode=%3\tspacing=%4\tmissed=%5\tfrozen=%6\tresync=%7")
                            .arg(traceTime(sampleCounter)).arg(detectNarrow?"N":"W").arg(getSSTVModeNameShort(currentMode)).arg(spacing).arg(missedBefore).arg(wasFrozen).arg(resynced));
      if(resynced) rxTrace("RESYNC",QString("%1\tproc=%2\tmode=%3\tfrozen_s=%4\tspacing=%5\tprev_spacing=%6\tlines=%7")
                           .arg(traceTime(sampleCounter)).arg(detectNarrow?"N":"W").arg(getSSTVModeNameShort(currentMode)).arg(((int)sampleCounter-(int)trFreezeCounter)/(double)SAMPLERATE,0,'f',1)
                           .arg(spacing).arg(activeChainPtr->count()>=2?(int)activeChainPtr->at(activeChainPtr->count()-2)->lineSpacing:0).arg(currentModePtr->receivedLines()));
    }
  }

  else
  {
    falseSyncs++;
  }
  if(retraceFlag)
  {
    switchProcessState(RETRACEWAIT);
    return;
  }
  if(currentModeMatchChanged)
  {
    currentModeMatchChanged=false;
    calculateLineNumber(activeChainPtr->last()->from,activeChainPtr->last()->to);
    lastSyncTest=activeChainPtr->last()->endTo;
    slantAdjust(false);
    if(falseSlantSync>=10 && !visAnchored && !proven)
    {
      if(rxTraceOn()) traceLost("falseslant");
      switchProcessState(SYNCLOSTFALSESYNC);
    }
  }
}

void syncProcessor::slotNewCall(QString call)
{
  if(rxTraceOn()) rxTrace("FSKID",QString("%1\tproc=%2\tcall=%3\tstate=%4").arg(traceTime(sampleCounter)).arg(detectNarrow?"N":"W").arg(call).arg(stateStr[syncProcesState]));
  emit callReceived(call);
  // the ID follows the picture: only a picture that is still being tracked is told to stop tracking; once it has ended
  // the flag would stay set and switch off sync tracking for the next picture
  if(syncProcesState==INSYNC) retraceFlag=true;
}

void syncProcessor::slotVisCodeDetected(int mode,uint visSampleCounter)
{
  trVis++;
  esstvMode first=idxStart;
  esstvMode last=idxEnd;
  // A code that ends the picture that runs is the start of the next one. That is a picture that a VIS code started (it runs by the
  // clock and never gives up), or one that has just seen the retrace pulse that comes before a VIS code (the code is read at its end,
  // the picture is saved and the receiver started again before that). A picture that a VIS code started has narrowed the range to
  // its own mode: the code that ends it is any mode the receiver is set to.
  bool endsPicture=(visAnchored && syncProcesState==INSYNC) || (syncProcesState==RETRACEWAIT);
  if(endsPicture) selectedModes(first,last);
  bool allowed=(mode>=first) && (mode<=last);
  if(rxTraceOn()) rxTrace("VIS",QString("%1\tproc=%2\tmode=%3\taccepted=%4\tstate=%5\tend=%6").arg(traceTime(sampleCounter)).arg(detectNarrow?"N":"W").arg(getSSTVModeNameShort((esstvMode)mode)).arg(allowed).arg(stateStr[syncProcesState]).arg(visSampleCounter/(double)SAMPLERATE,0,'f',4));
  if(allowed)
  {
    visMode=(esstvMode)mode;
    if((visMode>=AVT24)&&(visMode<=AVT94))
    {
      enableSyncDetection=false;
    }
    addToLog(QString("viscode %1 accepted").arg(visMode),LOGSYNCACCEPTED);
    if(endsPicture)
    {
      // a picture that was started by a VIS code never gives up, so the next code is what ends it: the picture is saved as it is
      // and the receiver starts again with this code (MMSSTV does the same); a code that was noise costs the picture its last part
      visRestart.valid=true;
      visRestart.mode=(esstvMode)mode;
      visRestart.counter=visSampleCounter;
      retraceFlag=true;
      visMode=NOTVALID;
      return;
    }
    idxStart=idxEnd=(esstvMode)mode;
    minMatchedLines=3;
    visEndCounter=visSampleCounter;
    if(!detectNarrow && syncProcesState==MODEDETECT && enableSyncDetection) pendingVisStart=true;
  }
  else
  {
    visMode=NOTVALID;
  }
}

void  syncProcessor::calcSyncQuality()
{
  int k;
  quint16 fs=0;
  quint16 tmp;
  syncQuality=10;
  QString str;
  //calc missing syncs in the last 10
  if(activeChainPtr->count()>=10)
  {
    for(k=activeChainPtr->count()-10;k<activeChainPtr->count()-1;k++)
    {
      // a false sync is a pulse that looks like a sync pulse of this mode (its width) but is not on the line grid: with the lower
      // ratios of the sensitivity table the noise puts many more pulses in the table, and those of any other width say nothing
      // about the picture
      tmp=0;
      for(uint idx=activeChainPtr->at(k)->from+1;idx<activeChainPtr->at(k)->to;idx++)
      {
        DSPFLOAT w=syncArray[idx].diffStartEnd();
        if(w>=syncWidth*MINSYNCWIDTHRATIO && w<=syncWidth*MAXSYNCWIDTHRATIO) tmp++;
      }
      if(activeChainPtr->at(k)->lineSpacing>0) tmp/=activeChainPtr->at(k)->lineSpacing; // this gives us the number of false syncs per line
      fs+=tmp;
    }
  }
  // False syncs and a clock that the pulses do not fit end a picture of noise early. A picture that has proven itself is waited for as long as
  // maxOutOfSyncLines() says: with the pulses found at the High and DX levels there are always many false ones between the real ones, and
  // a fade of six lines ended real pictures (2026-10-29, three Martin 1 on 20 m, MMSSTV received them whole)
  if(missingLines>5 && !proven)
  {
    str=QString("FS: %1,%2 ").arg(fs).arg(((fs*3)/25));
    syncQuality-=((fs*3)/25);
  }
  if(missingLines>=maxOutOfSyncLines())
  {
    str+=QString("missingLines: %1 ").arg(missingLines);
    syncQuality=-1;
  }
  if(!proven) syncQuality-=(falseSlantSync*2);
  str+=QString("falseSlantSync: %1").arg(falseSlantSync);
  if(syncQuality<0) syncQuality=0;
  if((syncQuality<=0) && (sensitivity!=(NUMBEROFSENSITIVITIES-1)))  // i.e DX Mode
  {

    addToLog(QString("syncQuality SYNCLOST %1").arg(str),LOGSYNCQUALITY);
    if(rxTraceOn()) traceLost(missingLines>=maxOutOfSyncLines() ? "missing" : "slant");
    switchProcessState(SYNCLOST);
  }
}

/**
 * @brief syncProcessor::maxOutOfSyncLines
 * @return the number of lines without a sync pulse after which the picture is given up
 *
 * The table gives a number of lines, which is a very different time per mode (20 lines is 3 s for Robot 36 and 10 s
 * for PD120). A fade longer than that ended the picture, and the signal coming back started a new one, so the
 * sensitivity also has a time that is waited at least. That only holds while the picture should still be running:
 * once its last line is due there is nothing left to wait for, and the receiver goes back to hunting (issue #129).
 *
 * Only a picture that has proven itself gets the time: noise starts pictures too, and with the time it kept the receiver
 * busy and ended up in the gallery as a picture of noise (issue #135).
 */

uint syncProcessor::maxOutOfSyncLines()
{
  uint lines=sensitivityArray[sensitivity].maxOutOfSyncLines;
  if(samplesPerLine<=0 || !proven) return lines;
  uint floorLines=(uint)ceil(sensitivityArray[sensitivity].minOutOfSyncSeconds*SAMPLERATE/samplesPerLine);
  if(floorLines>lines)
  {
    DSPFLOAT imageEnd=syncPosition+SSTVTable[currentMode].imageTime*modifiedClock+OUTOFSYNCEXTRALINES*samplesPerLine;
    if((sampleCounter+RXSTRIPE)<imageEnd) lines=floorLines;
  }
  return lines;
}

/**
 * @brief syncProcessor::traceStats
 *
 * Receiver trace (rxtrace.h): once a minute, how many sync-like pulses started, how many passed the width test, and the
 * level of the input.
 */

void syncProcessor::traceStats()
{
  for(int i=0;i<RXSTRIPE;i++)
  {
    DSPFLOAT v=inputVolumePtr[i];
    trLevelSum+=v;
    if(v>trLevelPeak) trLevelPeak=v;
    if(v>=sensitivityArray[sensitivity].minVolume) trGateOpen++;
  }
  trStripes++;
  if(trStripes>=(60*SAMPLERATE)/RXSTRIPE)
  {
    double samples=(double)trStripes*RXSTRIPE;
    rxTrace("PULSE-STATS",QString("%1\tproc=%2\tseconds=%3\tstarts=%4\taccepted=%5\tretrace=%6\tvis=%7\tlevel_mean=%8\tlevel_peak=%9\tgate_open_pct=%10\tstate=%11")
            .arg(traceTime(sampleCounter)).arg(detectNarrow?"N":"W").arg(samples/SAMPLERATE,0,'f',1).arg(trStarts).arg(trAccepted).arg(trRetrace).arg(trVis)
            .arg(trLevelSum/samples,0,'f',0).arg(trLevelPeak,0,'f',0).arg(100.*trGateOpen/samples,0,'f',1).arg(stateStr[syncProcesState]));
    trStripes=trStarts=trAccepted=trRetrace=trVis=trGateOpen=0;
    trLevelSum=0;
    trLevelPeak=0;
  }
}

/**
 * @brief syncProcessor::traceLost
 * @param reason why the picture is given up
 */

void syncProcessor::traceLost(const char *reason)
{
  rxTrace("LOST",QString("%1\tproc=%2\tmode=%3\treason=%4\tmissing_lines=%5\tlimit_lines=%6\ttable_lines=%7\tfalse_slant=%8\tage_s=%9\tlines=%10")
          .arg(traceTime(sampleCounter)).arg(detectNarrow?"N":"W").arg(getSSTVModeNameShort(currentMode)).arg(reason).arg(missingLines).arg(maxOutOfSyncLines())
          .arg(sensitivityArray[sensitivity].maxOutOfSyncLines).arg(falseSlantSync).arg(((int)sampleCounter-(int)trDetectCounter)/(double)SAMPLERATE,0,'f',1)
          .arg(currentModePtr?(int)currentModePtr->receivedLines():-1));
}

/**
 * @brief syncProcessor::traceSummary
 * @return the fields of the receiver trace's SAVE line that come from the sync tracking
 */

QString syncProcessor::traceSummary()
{
  double age=((int)sampleCounter-(int)trDetectCounter)/(double)SAMPLERATE;
  double ageLines=(samplesPerLine>0) ? age*SAMPLERATE/samplesPerLine : 0;
  return QString("proc=%1\tvia=%2\tage_s=%3\thits=%4\tfar_hits=%5\tfreezes=%6\tresyncs=%7\tmax_gap_s=%8\thits_per_line=%9\tnear_hits=%10\tproven=%11\ttrain_pulses=%12")
      .arg(detectNarrow?"N":"W").arg(trViaVis?"vis":(trViaTrain?"train":"sync")).arg(age,0,'f',1).arg(trHits).arg(trFarHits).arg(trFreezes).arg(trResyncs)
      .arg(trMaxGapLines*samplesPerLine/SAMPLERATE,0,'f',1).arg(ageLines>0?trHits/ageLines:0.,0,'f',3).arg(nearHits).arg(proven).arg(trTrainPulses);
}

/**
 * @brief syncProcessor::startFromVis start the picture the VIS code announced, without waiting for sync pulses
 *
 * In noise the pulses of a weak picture come on one line in five, and a chain of them may take half the picture to form. The VIS
 * code says where the picture starts (VISTOSTARTMS before the end of the code) and what mode it is: the picture is started
 * there and runs by the clock. The pulses that come refine its position (adoptChain), they do not decide whether it exists.
 */

void syncProcessor::startFromVis()
{
  if(!pendingVisStart) return;
  if(visMode==NOTVALID || (visMode>=AVT24 && visMode<=AVT94))
  {
    pendingVisStart=false;
    return;
  }
  int target=(int)visEndCounter-(int)(VISTOSTARTMS*SAMPLERATE/1000);
  // sstvRx takes the picture up at the stripe that holds its first line: that stripe has to be there
  if(target>=(int)(sampleCounter+RXSTRIPE)) return;
  pendingVisStart=false;
  if(target<0) target=0;
  currentMode=visMode;
  modifiedClock=rxClock/SUBSAMPLINGFACTOR;
  samplesPerLine=getLineLength(currentMode,modifiedClock);
  syncWidth=getSyncWidth(currentMode,modifiedClock);
  dropChains();
  if(!createModeBase())
  {
    addToLog("Error creating modeBase",LOGALL);
    currentMode=NOTVALID;
    return;
  }
  trViaVis=true;
  trViaTrain=false;
  visMode=NOTVALID;
  falseSyncs=0;
  lineTolerance=sensitivityArray[sensitivity].lineTolerance;
  syncPosition=(quint32)target;
  lastValidSyncCounter=syncPosition;
  tempOutOfSync=false;
  slantAdjustLine=6;
  clockAdjusted=false;
  visBackfillLines=0;
  visAnchored=true;
  missingLines=0;
  switchProcessState(INSYNC);
  trDetectCounter=sampleCounter;
  trHits=trFarHits=trFreezes=trResyncs=trMaxGapLines=0;
  nearHits=nearRun=0;
  proven=true;
  trustedEnd=target;
  if(rxTraceOn()) rxTrace("DETECT",QString("%1\tproc=%2\tmode=%3\tvia=vis-start\tvis_age_s=%4\tchain=0\tfract=0\tline_ms=%5\tsens=%6\tmin_volume=%7\tvis_to_start_ms=%8\tbackfill_lines=0")
                          .arg(traceTime(sampleCounter)).arg(detectNarrow?"N":"W").arg(getSSTVModeNameShort(currentMode))
                          .arg(((int)sampleCounter-(int)visEndCounter)/(double)SAMPLERATE,0,'f',1).arg(samplesPerLine*1000./SAMPLERATE,0,'f',1).arg(sensitivity)
                          .arg(sensitivityArray[sensitivity].minVolume).arg(-VISTOSTARTMS));
  addToLog(QString("Mode started by its VIS code: %1, line 0 at %2").arg(getSSTVModeNameShort(currentMode)).arg(syncPosition),LOGSYNCEVAL);
}

/**
 * @brief syncProcessor::dropChains forget the chains of pulses without leaving the process state
 */

void syncProcessor::dropChains()
{
  int i;
  for(i=idxStart;i<=idxEnd;i++)
  {
    while(matchArray[i].count()>0) removeMatchArrayChain((esstvMode)i,0);
  }
  activeChainPtr=NULL;
}

/**
 * @brief syncProcessor::adoptChain a picture that runs by its VIS code has found a chain of pulses: take its position from the pulses
 * @return true if a chain was adopted
 *
 * The chain's first pulse is some lines after the picture's first line; the VIS code says which line it is (as for visBackfill, but
 * without the age limit: the picture may be minutes old). A chain whose grid is not where the code puts it is noise and is dropped.
 * The picture is decoded again from the new position (the receiver does that when it gets a new clock).
 */

bool syncProcessor::adoptChain()
{
  if(!findMatch()) return false;
  syncWidth=getSyncWidth(currentMode,modifiedClock);
  unsigned int syncCorrected;
  if(syncArray[0].retrace) syncCorrected=syncArray[0].end;
  else syncCorrected=(syncArray[0].start+syncArray[0].end)/2+syncWidth/2;
  quint32 chainPosition=currentModePtr->adjustSyncPosition(syncCorrected,syncArray[0].retrace)+FILTERDELAYCORRECTION;
  double visStart=(double)visEndCounter-VISTOSTARTMS*SAMPLERATE/1000.;
  double d=(double)chainPosition-visStart;
  double n=round(d/samplesPerLine);
  double tolerance=VISADOPTMS*SAMPLERATE/1000.;       // the code is good to a few ms: a chain that is further off is noise
  if(n<0 || fabs(d-n*samplesPerLine)>tolerance || n*samplesPerLine>=SSTVTable[currentMode].imageTime*modifiedClock)
  {
    if(rxTraceOn()) rxTrace("ADOPT",QString("%1\tproc=%2\tmode=%3\taccepted=0\toff_lines=%4").arg(traceTime(sampleCounter)).arg(detectNarrow?"N":"W").arg(getSSTVModeNameShort(currentMode)).arg(d/samplesPerLine,0,'f',3));
    dropChains();
    return false;
  }
  lineTolerance=sensitivityArray[sensitivity].lineTolerance;
  int before=(int)syncPosition;
  quint32 anchor=syncPosition;
  syncPosition=chainPosition;
  visBackfillLines=0;
  slantAdjust(true);                       // (does nothing for a short chain)
  visBackfillLines=(uint)n;
  syncPosition-=(quint32)round(n*samplesPerLine);
  // The code is good to a fraction of a millisecond on a clean signal; the pulses are in some modes up to 1.5 ms away from where
  // the code puts the first line (their edges depend on the width of the pulse). The picture stays where the code put it unless the
  // pulses disagree by more than that.
  double pulsesMs=((int)syncPosition-(int)anchor)*1000./SAMPLERATE;       // where the pulses put the picture against where the code did
  if(abs((int)syncPosition-(int)anchor)<(int)(VISSTAYMS*SAMPLERATE/1000)) syncPosition=anchor;
  else newClock=true;                      // decode the picture again from the position the pulses give
  falseSyncs=0;
  trustedEnd=syncArray[activeChainPtr->last()->to].end;
  if(rxTraceOn()) rxTrace("ADOPT",QString("%1\tproc=%2\tmode=%3\taccepted=1\tchain=%4\tbackfill_lines=%5\tmoved_ms=%6\tpulses_vs_code_ms=%7").arg(traceTime(sampleCounter)).arg(detectNarrow?"N":"W").arg(getSSTVModeNameShort(currentMode))
                          .arg(activeChainPtr->count()).arg(visBackfillLines).arg(((int)syncPosition-before)*1000./SAMPLERATE,0,'f',1).arg(pulsesMs,0,'f',1));
  return true;
}

/**
 * @brief syncProcessor::visBackfill
 * @return the number of sync intervals the picture starts before the first pulse of the chain that was just found (0: none)
 *
 * The chain starts at the first pulses the sync detector could follow. Lines before that, in the noise, were cut off the top of
 * the picture and everything below moved up by as many rows. When a VIS code announced the mode, the picture starts a known
 * time after its end, so it is started there: the whole number of intervals before the chain. The code is only believed if
 * the chain's line grid falls where the code puts it (to a fraction of a line) and if it is not too old; the audio of those
 * lines is still in the buffers.
 */

uint syncProcessor::visBackfill()
{
  if(!trViaVis || samplesPerLine<=0) return 0;
  int age=(int)sampleCounter-(int)visEndCounter;
  if(age<0 || age>VISBACKFILLSECONDS*SAMPLERATE) return 0;
  double visStart=(double)visEndCounter-VISTOSTARTMS*SAMPLERATE/1000.;
  double d=(double)syncPosition-visStart;
  if(d<=0) return 0;
  double n=round(d/samplesPerLine);
  if(n<1) return 0;
  double tolerance=std::min(VISBACKFILLTOLERANCE*samplesPerLine,(double)VISBACKFILLMAXMS*SAMPLERATE/1000.);
  if(fabs(d-n*samplesPerLine)>tolerance) return 0;
  if(n*samplesPerLine>=SSTVTable[currentMode].imageTime*modifiedClock) return 0;
  return (uint)n;
}

/**
 * @brief syncProcessor::calculateLineNumber
 * @param fromIdx
 * @param toIdx
 */

void  syncProcessor::calculateLineNumber(uint fromIdx,uint toIdx)
{
  quint16 lnbr;
  double fract;
  lineCompare(samplesPerLine,0,toIdx,lnbr,fract);
  syncArray[toIdx].length=syncArray[toIdx].end-syncArray[fromIdx].end;
  syncArray[toIdx].lineNumber=lnbr;
  lastUpdatedSync=toIdx;
}

void syncProcessor::checkSyncArray()
{

  // The receiver takes the buffers back when a picture ends by a retrace, so a picture that starts at the VIS code of its successor reads the
  // pulses of that code again: the last of them, the stop bit, is wide like a retrace pulse and ends where the first line starts. It is not
  // the end of this picture (the limit is a stop bit's width after the start of the first line)
  if(syncArray[syncArrayIndex].retrace && !(visAnchored && (int)syncArray[syncArrayIndex].end<(int)syncPosition+(int)(VISSTOPBITMS*SAMPLERATE/1000)))
  {
    trRetrace++;
    syncArray[0]=syncArray[syncArrayIndex];
    syncArrayIndex=0;
    //      syncPosition=syncArray[0].end + FILTERDELAYCORRECTION;
    addToLog(QString("Found retrace: start:%1 end:%2  width %3").arg(syncArray[0].start).arg(syncArray[0].end).arg(syncArray[0].end-syncArray[0].start),LOGSYNCACCEPTED);
    if(syncProcesState==INSYNC)
    {
      if(rxTraceOn()) rxTrace("RETRACE",QString("%1\tproc=%2\tmode=%3\tage_s=%4\tlines=%5").arg(traceTime(sampleCounter)).arg(detectNarrow?"N":"W").arg(getSSTVModeNameShort(currentMode)).arg(((int)sampleCounter-(int)trDetectCounter)/(double)SAMPLERATE,0,'f',1).arg(currentModePtr->receivedLines()));
      retraceFlag=true;
      syncProcesState=RETRACEWAIT;
    }
  }

  if(syncArrayIndex>=(MAXSYNCENTRIES-1))
  {
    //shift syncArray
    dropTop();
  }
  addToLog(QString("Found sync: start:%1 end:%2 width %3 at %4").arg(syncArray[syncArrayIndex].start).arg(syncArray[syncArrayIndex].end).arg(syncArray[syncArrayIndex].end-syncArray[syncArrayIndex].start).arg(syncArrayIndex),LOGSYNCACCEPTED);
}


/**
 * @brief syncProcessor::findMatch
 The syncArray is filled with valid synPositions.
 idxStart and idxEnd indicate the sstv modes to compare the sync pulse intervals with.


* @return true if match

*/


bool syncProcessor::findMatch()
{
  int i,j,k,m;
  int fs;
  uint minTotLines=9999;
  double minFract=1;
  int idx=-1;
  QList<modeMatchList *> changeList;
  QList <int> modeList;
  QList<uint> totalLinesList;
  QList<double> totalFractList;

  if(enableSyncDetection)
  {
    //      addToLog (QString(" checking match for syncArrayIndex %1 at %2").arg(syncArrayIndex).arg(syncArray[syncArrayIndex].end),LOGSYNCMATCH);
    for(i=idxStart;i<=idxEnd;i++)
    {
      syncWidth=getSyncWidth((esstvMode)i ,modifiedClock);
      if(addToMatch((esstvMode)i))
      {
        for(j=0;j<matchArray[i].count();j++)
        {
          // only a chain this pulse ended: chains left over from earlier pulses are not removed, and once the table
          // scrolls their false-sync count can fall under the limit, so one that ends minutes ago would start the picture
          if((matchArray[i][j]->count()>=(int)minMatchedLines) && (matchArray[i][j]->last()->to==syncArrayIndex))
          {
            fs=0;
            for(k=0;k<matchArray[i][j]->count()-1;k++)
            {
              fs+=matchArray[i][j]->at(k)->to-matchArray[i][j]->at(k)->from-1;
            }
            if (fs<20)
            {
              changeList.append(matchArray[i][j]);
              modeList.append(i);
            }
          }

        }
      }
    }
    for (m=0;m<changeList.count();m++)
    {
      totalLinesList.append(calcTotalLines( changeList.at(m)));
      totalFractList.append(calcTotalFract( changeList.at(m)));
    }
    for (m=0;m<changeList.count();m++)
    {
      if((minTotLines>=totalLinesList.at(m)) && (totalFractList.at(m)<=minFract))
      {
        idx=m;
        minTotLines=totalLinesList.at(m);
        minFract=totalFractList.at(m);
      }
    }


    if(idx>=0)
    {
      if(visMode!=NOTVALID)
      {
        currentMode=visMode;
      }
      else
      {
        currentMode=(esstvMode)modeList.at(idx);
      }

      activeChainPtr=changeList.at(idx);
      samplesPerLine=getLineLength(currentMode,modifiedClock);
      cleanupMatchArray();
      return true;
    }
    return false;
  }

  return false;
}


uint syncProcessor::calcTotalLines(modeMatchList *mlPtr)
{
  int i;
  uint lines=0;
  for(i=0;i<mlPtr->count();i++)
  {
    lines+=mlPtr->at(i)->lineSpacing;
  }
  return lines;
}

double syncProcessor::calcTotalFract(modeMatchList *mlPtr)
{

  int i;
  double fract=0;
  for(i=0;i<mlPtr->count();i++)
  {
    fract+=mlPtr->at(i)->fraction;
  }
  return fract;

}

/**
 * @brief syncProcessor::addToMatch
 * @param mode sstv mode to be evaluated
 * @return true if added to chain
 */

bool syncProcessor::addToMatch(esstvMode mode)
{
  int  i;
  
  if(syncArrayIndex<1) return false;
  for(i=syncArrayIndex-1;i>=0;i--)
  {

    if(addToChain(mode,i))
    {
      return true;
    }
  }
  return false;
}






bool syncProcessor::addToChain(esstvMode mode,  uint fromIdx)
{
  int i;
  double fract;
  quint16 lineSpacing;

  samplesPerLine=getLineLength(mode,modifiedClock);
  
  if(!lineCompare(samplesPerLine,fromIdx,syncArrayIndex,lineSpacing,fract))
  {
    return false;
  }


  if((syncArray[syncArrayIndex].diffStartEnd()<syncWidth*MINSYNCWIDTHRATIO)
     || (syncArray[syncArrayIndex].diffStartEnd()>syncWidth*MAXSYNCWIDTHRATIO)
     || (syncArray[fromIdx].diffStartEnd()<syncWidth*MINSYNCWIDTHRATIO))
  {

    return false;
  }

  if (syncArray[fromIdx].diffStartEnd()>syncWidth*MAXSYNCWIDTHRATIO)
  {
    if(fromIdx!=0 || !syncArray[0].retrace)
    {
      return false;
    }
  }
  if(syncProcesState==MODEDETECT || (visAnchored && activeChainPtr==NULL))     // (the latter: a picture running by its VIS code looks for its first chain)
  {
    if(lineSpacing>sensitivityArray[sensitivity].maxLineDistanceModeDetect)
    {
      return false;
    }
  }

  bool found=false;
  if(matchArray[mode].count()==0) // we don't have a chain yet
  {
    matchArray[mode].append(new modeMatchList);
    matchArray[mode][0]->append(new smatchEntry(fromIdx,syncArrayIndex,lineSpacing,fract,syncArray[fromIdx].end,syncArray[syncArrayIndex].end));
    //      addToLog(QString("Match: mode=%1,new chain=%2 syncIndex=%3 end=%4").arg(getSSTVModeNameShort(mode)).arg(matchArray[mode].count()-1).arg(syncArrayIndex).arg(syncArray[syncArrayIndex].end),LOGSYNCMATCH);
  }
  else
  {
    // can we append this to an existing chain?
    for(i=0;i<matchArray[mode].count();i++)
    {
      if(matchArray[mode][i]->last()->to==fromIdx)
      {
        if((syncProcesState==INSYNC) && (i==0) )
        {
          currentModeMatchChanged=true;
        }
        matchArray[mode][i]->append(new smatchEntry(fromIdx,syncArrayIndex,lineSpacing,fract,syncArray[fromIdx].end,syncArray[syncArrayIndex].end));
        //              addToLog(QString("Match: mode=%1,chain=%2 syncIndex=%3 end=%4").arg(getSSTVModeNameShort(mode)).arg(i).arg(syncArrayIndex).arg(syncArray[syncArrayIndex].end),LOGSYNCMATCH);
        found=true;
        break;
      }
    }
    if(!found)
    {
      matchArray[mode].append(new modeMatchList);
      matchArray[mode].last()->append(new smatchEntry(fromIdx,syncArrayIndex,lineSpacing,fract,syncArray[fromIdx].end,syncArray[syncArrayIndex].end));
      //          addToLog(QString("Match: mode=%1,new chain=%2 syncIndex=%3 end=%4").arg(getSSTVModeNameShort(mode)).arg(matchArray[mode].count()-1).arg(syncArrayIndex).arg(syncArray[syncArrayIndex].end),LOGSYNCMATCH);
    }

  }
  return true;
}


void syncProcessor::clearMatchArray()
{
  int i,j;
  for(i=idxStart;i<=idxEnd;i++)
  {
    for(j=0;j<matchArray[i].count();j++)
    {
      removeMatchArrayChain((esstvMode)i,0);
    }
    matchArray[i].clear();
  }
  activeChainPtr=NULL;
  switchProcessState(MODEDETECT);
}

void syncProcessor::removeMatchArrayChain(esstvMode mode,int chainIdx)
{
  int i;
  for(i=0;i<matchArray[mode][chainIdx]->count();i++)
  {
    delete matchArray[mode][chainIdx]->at(i);
  }
  matchArray[mode][chainIdx]->clear();
  delete matchArray[mode][chainIdx];
  matchArray[mode].takeAt(chainIdx);
}

void syncProcessor::cleanupMatchArray()
{
  int i,j;
  //  double fract=0;
  //  quint16 lnbr=0;
  for(i=idxStart;i<=idxEnd;i++)
  {
    for(j=0;j<matchArray[i].count();)
    {
      if(activeChainPtr!=matchArray[i][j])
      {
        removeMatchArrayChain((esstvMode)i,j);
      }
      else
      {
        j++;
      }
    }
  }

  for(i=0;i<activeChainPtr->count();i++)
  {
    syncArray[activeChainPtr->at(i)->from].inUse=true;
    syncArray[activeChainPtr->at(i)->to].inUse=true;
  }
  for(i=0;i<syncArrayIndex;)
  {
    if(!syncArray[i].inUse)
    {
      deleteSyncArrayEntry(i);
    }
    else
    {
      i++;
    }
  }
  for(i=0;i<syncArrayIndex;i++)
  {
    calculateLineNumber(i,i+1);
  }
}

void syncProcessor::dropTop()
{
  deleteSyncArrayEntry(0);
}

void syncProcessor::deleteSyncArrayEntry(uint entry)
{
  int i,j,k;
  modeMatchList *ml;
  //  smatchEntry *tempPtr;
  if(entry>=syncArrayIndex) return;
  // delete or adapt the matchArrays
  for(i=idxStart;i<=idxEnd;i++)   //all modes
  {
    for(j=0;j<matchArray[i].count();) //all chains
    {
      for(k=0;k<matchArray[i][j]->count();)
      {
        ml=matchArray[i][j];
        if (ml->at(k)->from==entry)
        {
          delete matchArray[i][j]->takeAt(k);
        }
        else
        {
          if(ml->at(k)->from>entry)
          {
            ml->at(k)->from--;
          }
          if(ml->at(k)->to>entry)
          {
            ml->at(k)->to--;
          }
          k++;
        }
      }
      if(matchArray[i][j]->count()==0)
      {
        if(activeChainPtr==matchArray[i][j])
        {
          activeChainPtr=NULL;
        }
        delete matchArray[i].takeAt(j);
      }
      else
      {
        j++;
      }
    }
  }
  for(i=entry;i<(int)syncArrayIndex;i++)
  {
    syncArray[i]=syncArray[i+1];
  }
  syncArrayIndex--;
}


/**
 * @brief syncProcessor::recalculateMatchArray
 * \todo recalculate
 */

void syncProcessor::recalculateMatchArray()
{
}


/**
 * @brief syncProcessor::lineCompare
 * @param samPerLine samples per line
 * @param srcIdx from sync array entry
 * @param dstIdx to sync array entry
 * @param lineNumber number of lines
 * @param fraction fractional part
 * @return true if a match for that line (i.e fractional part is smaller than lineTolerance
 */


bool syncProcessor::lineCompare(DSPFLOAT samPerLine, int srcIdx, int dstIdx, quint16 &lineNumber, double &fraction)
{
  double delta;
  delta=(double)(syncArray[dstIdx].end-syncArray[srcIdx].end);

  bool match=lineMatch(delta,samPerLine,lineTolerance,lineNumber,fraction);
  //  if(match)
  //  addToLog(QString("Lnbr: %1, fract: %2, delta: %3 src: %4,dest: %5, OK %6")
  //           .arg(lineNumber).arg(fraction).arg(delta).arg(srcIdx).arg(dstIdx).arg(match)
  //           ,LOGSYNCCOMP);
  return match;
}

void syncProcessor::resetRetraceFlag()
{
  retraceFlag=false;
  train.reset();      // (the receiver takes the buffers back: the audio of the last stripes comes again)
  // (the receiver takes the buffers back, and sampleCounter with them: the counters stay positions in the signal, the code of visRestart is where it was)
  clearMatchArray();
}


bool  syncProcessor::createModeBase()
{
  bool done=false;
  if(currentModePtr) delete currentModePtr;
  currentModePtr=NULL;
  switch (currentMode)
  {
  case M1:
    currentModePtr=new modeGBR(currentMode,RXSTRIPE,false,false);
    break;
  case M2:
    currentModePtr=new modeGBR(currentMode,RXSTRIPE,false,false);
    break;
  case S1:
  case S2:
  case SDX:
    currentModePtr=new modeGBR2(currentMode,RXSTRIPE,false,false);
    break;
  case R36:
    currentModePtr=new modeRobot1(currentMode,RXSTRIPE,false,false);
    break;
  case R24:
  case R72:
    currentModePtr=new modeRobot2(currentMode,RXSTRIPE,false,false);
    break;
  case ML180:
  case ML240:
  case ML280:
  case ML320:
  case MR73:
  case MR90:
  case MR115:
  case MR140:
  case MR175:
    currentModePtr=new modeRobot2(currentMode,RXSTRIPE,false,false);
    break;
  case SC2_60:
  case SC2_120:
  case SC2_180:
  case P3:
  case P5:
  case P7:
    currentModePtr=new modeRGB(currentMode,RXSTRIPE,false,false);
    break;
  case MC110N:
  case MC140N:
  case MC180N:
    currentModePtr=new modeRGB(currentMode,RXSTRIPE,false,true);
    break;
  case FAX480:
  case BW8:
  case BW12:
    currentModePtr=new modeBW(currentMode,RXSTRIPE,false,false);
    break;
  case AVT24:
  case AVT90:
  case AVT94:
    currentModePtr=new modeAVT(currentMode,RXSTRIPE,false,false);
    break;
  case PD50:
  case PD90:
  case PD120:
  case PD160:
  case PD180:
  case PD240:
  case PD290:
  case MP73:
  case MP115:
  case MP140:
  case MP175:
    currentModePtr=new modePD(currentMode,RXSTRIPE,false,false);
    break;
  case PD120W:
    currentModePtr=new modePD(currentMode,RXSTRIPE,false,false);
    break;
  case JB60:
    currentModePtr=new modeJB60(currentMode,RXSTRIPE,false,false);
    break;
  case MP73N:
  case MP110N:
  case MP140N:
    currentModePtr=new modePD(currentMode,RXSTRIPE,false,true);
    break;
  default:
    currentMode=NOTVALID;
    break;
  }
  if (currentMode!=NOTVALID)
  {
    initializeSSTVParametersIndex(currentMode,false);
    QString s=getSSTVModeNameLong(currentMode);
    addToLog(QString("create RX mode: %1").arg(getSSTVModeNameShort(currentMode)),LOGSYNCSTATE);
    currentModePtr->init(modifiedClock);
    startImageRXEvent* ce = new startImageRXEvent(QSize(currentModePtr->imagePixels(),currentModePtr->imageLines()));
    ce->waitFor(&done);
    QApplication::postEvent(dispatcherPtr, ce);
    while(!done)
    {
      QApplication::processEvents();
    }
  }
  return (currentMode!=NOTVALID);
}


void syncProcessor::regression(DSPFLOAT &a,DSPFLOAT &b,bool initial)
{
  /* calculate linear regression
    formula x=a+by
    b=sum((x[i]-xm)*(y[i]-ym))/sum((y[i]-ym)*(y[i]-ym))
    a=xm-b*ym
  */
  int j;

  int count=activeChainPtr->count();
  falseSlantSync=0;
  DSPFLOAT sum_x,sum_y,sum_xx,sum_xy;
  sum_x=sum_y=sum_xx=sum_xy=a=b=0;
  unsigned int endZero;
  unsigned int tempCount=0;
  j=0;
  endZero=syncArray[0].end;
  for(;j<count;j++)
  {

    if((activeChainPtr->at(j)->fraction>0.006)&&(!initial))
    {
      continue;
    }

    slantXYArray[tempCount].y=(DSPFLOAT)(syncArray[activeChainPtr->at(j)->to].end-endZero);
    slantXYArray[tempCount].x= syncArray[activeChainPtr->at(j)->to].lineNumber*samplesPerLine;
    addToLog(QString("pos: %1, x=%2 y=%3 syncIndex:%4, diff %5").arg(tempCount).arg(slantXYArray[tempCount].x).arg(slantXYArray[tempCount].y).arg(activeChainPtr->at(j)->to).arg(slantXYArray[tempCount].x-slantXYArray[tempCount].y) ,LOGSLANT);
    if((fabs(slantXYArray[tempCount].x-slantXYArray[tempCount].y)>150.)&&(!initial))
    {
      falseSlantSync++;
      continue;
    }
    sum_x+=slantXYArray[tempCount].x;
    sum_y+=slantXYArray[tempCount].y;
    sum_xx+=slantXYArray[tempCount].x*slantXYArray[tempCount].x;
    sum_xy+=slantXYArray[tempCount].x*slantXYArray[tempCount].y;
    lastValidSyncCounter=syncArray[activeChainPtr->at(j)->to].end;
    tempCount++;
  }
  slantPoints=tempCount;
  slantOffset0=tempCount?(sum_y-sum_x)/tempCount:0;       // where the pulses are on average against the line grid, if the clock is right
  slantSpanLines=0;
  if(tempCount>1 && samplesPerLine>0) slantSpanLines=(slantXYArray[tempCount-1].x-slantXYArray[0].x)/samplesPerLine;
  if(tempCount<2)
  {
    a=0;
    b=1;
    return;
  }
  b=((tempCount)*sum_xy-(sum_x*sum_y))/((tempCount)*sum_xx-(sum_x*sum_x));
  a=sum_y/(tempCount)-(b*sum_x)/(tempCount);
}



bool syncProcessor::slantAdjust(bool initial)
{
  DSPFLOAT a,b;
  if ((currentMode>=AVT24) && (currentMode <= AVT94)) return true;
  if(currentMode==NOTVALID) return true;
  falseSlantSync=0;
  if(!initial)
  {
    if(syncArray[activeChainPtr->last()->to].lineNumber<slantAdjustLine) return false;
  }
  regression(a,b,initial);
  // A clock correction may be big (up to 2% the first time), so it is only believed when it rests on enough pulses that are spread
  // over enough lines: from the four of a chain that a VIS code lets through, in noise, it came out 0.65% off and the pulses that
  // followed no longer fitted; a picture whose clock is a little wrong comes out sheared.
  if(slantPoints<SLANTMINPOINTS || slantSpanLines<SLANTMINLINES)
  {
    if(!initial || slantPoints<2)
    {
      if(!initial) slantAdjustLine+=5;
      return false;
    }
    // too few pulses for a clock: the picture still starts where the pulses are on average, not where the first one is
    b=1;
    a=slantOffset0;
  }

  //  addToLog(QString("regr. params line %1 a:%2 b:%3").arg(slantAdjustLine).arg(a).arg(b),LOGSLANT);
  slantAdjustLine+=5;
  if(!autoSlantAdjust) return false;
  if(initial || !clockAdjusted)
  {
    if((fabs(1.-b)>0.02) || (fabs(a)>100)) return false;
  }
  else
  {
    if((fabs(1.-b)>0.005) || (fabs(a)>50)) return false;
  }
  if  (((fabs(1.-b)>0.00001)  || (fabs(a)>1)  )    && autoSlantAdjust)
  {
    newClock=true;
    clockAdjusted=true;
    modifiedClock*=b;
    samplesPerLine=getLineLength(currentMode,modifiedClock); //recalculate the samples per line
    addToLog(QString("new clock accepted: %1 a=%2,b=%3").arg(modifiedClock).arg(a).arg(b),LOGSLANT);

    syncArray[0].end+=(long)round(a);
    syncArray[0].start+=(long)round(a);

    unsigned int syncCorrected;
    if(syncArray[0].retrace)
    {
      syncCorrected=syncArray[0].end;
    }
    else
    {
      syncCorrected=(syncArray[0].start+syncArray[0].end)/2+getSyncWidth(currentMode,modifiedClock)/2;
    }

    syncPosition=currentModePtr->adjustSyncPosition(syncCorrected,syncArray[0].retrace)+FILTERDELAYCORRECTION; // type 2 sync end
    syncPosition-=(quint32)round(visBackfillLines*samplesPerLine);   // a picture that starts at its VIS code stays there

    recalculateMatchArray();
    addToLog(QString("slantAdjust: modified  syncpos:=%1").arg(syncPosition),LOGSLANT);

    return true;
  }
  return false;
}



void syncProcessor::switchSyncState(esyncState newState,quint32 sampleCntr)
{
  Q_UNUSED(sampleCntr)
  if(syncState!=newState)
  {
    addToLog(QString("switching from %1 to %2 at %3").arg(syncStateStr[syncState]).arg(syncStateStr[newState]).arg(sampleCntr),LOGSYNCSTATE);
    syncState=newState;
  }
}

void syncProcessor::switchProcessState(esyncProcessState  newState)
{
  addToLog(QString("syncProcessState %1 to %2").arg(stateStr[syncProcesState]).arg(stateStr[newState]),LOGSYNCPROCESSSTATE);
  if((newState==SYNCLOSTFALSESYNC) || (newState==SYNCLOSTNEWMODE)||(newState==SYNCLOSTMISSINGLINES) || (newState==SYNCLOST))
  {
    newState=SYNCLOST;
  }
  syncProcesState=newState;
}

#ifndef QT_NO_DEBUG
void syncProcessor::setOffset(unsigned int dataScopeOffset)
{
  xOffset=dataScopeOffset;
  scopeViewerSyncNarrow->setOffset(xOffset);
  scopeViewerSyncWide->setOffset(xOffset);
}

void syncProcessor::clear()
{
  scopeViewerSyncNarrow->clear();
  scopeViewerSyncWide->clear();
  xOffset=0;
  scopeViewerSyncNarrow->setOffset(xOffset);
  scopeViewerSyncWide->setOffset(xOffset);
}


#endif
