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
  // minMatchedLines maxLineDistanceModeDetect onRatio offRatio minVolume maxTempOutOfSyncLines maxOutOfSyncLines minOutOfSyncSeconds lineTolerance;
  {         4,                2,                0.5,    0.3,   2000,        10, 15, 10, 0.008},
  {         4,                2,                0.45,   0.3,   1000,        10, 20, 20, 0.008},
  {         4,                2,                0.38,   0.25,  100,         15, 30, 30, 0.0095},
  {         4,                2,                0.38,   0.25,  500,         15, 30,  0, 0.0095}
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
  currentModePtr=NULL;
  visMode=NOTVALID;
  trDetectCounter=trFreezeCounter=0;
  nearHits=nearRun=0;
  proven=false;
  trustedEnd=0;
  trViaVis=false;
  visEndCounter=0;
  visBackfillLines=0;
  trHits=trFarHits=trFreezes=trResyncs=trMaxGapLines=0;
  trStripes=trStarts=trAccepted=trRetrace=trVis=trGateOpen=0;
  trLevelSum=0;
  trLevelPeak=0;
  maxLineSamples=getMaxLineSamples();
  //  syncFound=false;
  if(!detectNarrow)
  {
    connect(streamDecode.getFskDecoderPtr(),SIGNAL(callReceived(QString)),SLOT(slotNewCall(QString)));
    connect(streamDecode.getVisDecoderPtr(),SIGNAL(visCodeWideDetected(int,uint)),SLOT(slotVisCodeDetected(int,uint)));
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
 * @brief initialize sync processor
 * idxStart and idxEnd are set for narrow or wide modes in Auto mode, else idxStart=idxEnd= selected mode
 * enableSyncDetection is set to false for AVT modes.
 *
 */
void syncProcessor::init()
{
  enableSyncDetection=true;
  //  syncAvg=0;

  if(detectNarrow)
  {
    idxStart=STARTNARROW;
    idxEnd=ENDNARROW;
  }
  else
  {
    idxStart=STARTWIDE;
    idxEnd=ENDWIDE;
  }

  if(sstvModeIndexRx!=0)
  {
    //      if((sstvModeIndexRx>=idxStart) && (sstvModeIndexRx<=idxEnd))
    {
      idxEnd=idxStart=(esstvMode)(sstvModeIndexRx-1);
      if((idxStart>=AVT24)&&(idxStart<=AVT94))
      {
        enableSyncDetection=false;
      }
    }
  }
  visMode=NOTVALID; // and reset the visMode
  syncProcesState=MODEDETECT;
  modifiedClock=rxClock/SUBSAMPLINGFACTOR;
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
  streamDecode.process(freqPtr,sampleCounter); // only VIS and FSK if not narrow
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

  if((syncProcesState==INSYNC) && (enableSyncDetection))
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
    checkSyncArray();
    switch(syncProcesState)
    {
    case MODEDETECT:
      if(findMatch())
      {
        trViaVis=(visMode!=NOTVALID);
        visMode=NOTVALID; //reset visMode;
        //we have a new mode
        if(!createModeBase())
        {
          addToLog("Error creating modeBase",LOGALL);
          result=false;
        }
        else
        {
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
          visBackfillLines=0;
          slantAdjust(true);
          visBackfillLines=visBackfill();
          if(visBackfillLines) syncPosition-=(quint32)round(visBackfillLines*samplesPerLine);
          switchProcessState(INSYNC);
          trDetectCounter=sampleCounter;
          trHits=trFarHits=trFreezes=trResyncs=trMaxGapLines=0;
          nearHits=nearRun=0;
          proven=trViaVis && ((int)sampleCounter-(int)visEndCounter)>=0 && ((int)sampleCounter-(int)visEndCounter)<=VISFRESHSECONDS*SAMPLERATE;
          trustedEnd=syncArray[activeChainPtr->last()->to].end;
          if(rxTraceOn()) rxTrace("DETECT",QString("%1\tproc=%2\tmode=%3\tvia=%4\tvis_age_s=%5\tchain=%6\tfract=%7\tline_ms=%8\tsens=%9\tmin_volume=%10\tvis_to_start_ms=%11\tbackfill_lines=%12")
                                  .arg(traceTime(sampleCounter)).arg(detectNarrow?"N":"W").arg(getSSTVModeNameShort(currentMode)).arg(trViaVis?"vis":"sync")
                                  .arg(trViaVis?QString::number(((int)sampleCounter-(int)visEndCounter)/(double)SAMPLERATE,'f',1):QString("-")).arg(activeChainPtr->count())
                                  .arg(calcTotalFract(activeChainPtr),0,'f',4).arg(samplesPerLine*1000./SAMPLERATE,0,'f',1).arg(sensitivity).arg(sensitivityArray[sensitivity].minVolume)
                                  .arg(trViaVis?QString::number(((int)syncPosition+(int)round(visBackfillLines*samplesPerLine)-(int)visEndCounter)*1000./SAMPLERATE,'f',1):QString("-")).arg(visBackfillLines));
          addToLog(QString("Mode detected: %1, sync at %2").arg(getSSTVModeNameShort(currentMode)).arg(syncPosition),LOGSYNCEVAL);
        }
      }
      break;
    case INSYNC:
      if(enableSyncDetection) trackSyncs();
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
    if(falseSlantSync>=10)
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
  if(rxTraceOn()) rxTrace("VIS",QString("%1\tproc=%2\tmode=%3\taccepted=%4\tstate=%5").arg(traceTime(sampleCounter)).arg(detectNarrow?"N":"W").arg(getSSTVModeNameShort((esstvMode)mode)).arg((mode>=idxStart) && (mode<=idxEnd)).arg(stateStr[syncProcesState]));
  if((mode>=idxStart) && (mode<=idxEnd))
  {
    visMode=(esstvMode)mode;
    if((visMode>=AVT24)&&(visMode<=AVT94))
    {
      enableSyncDetection=false;
    }
    addToLog(QString("viscode %1 accepted").arg(visMode),LOGSYNCACCEPTED);
    idxStart=idxEnd=(esstvMode)mode;
    minMatchedLines=3;
    visEndCounter=visSampleCounter;
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
  if(missingLines>5)
  {
    str=QString("FS: %1,%2 ").arg(fs).arg(((fs*3)/25));
    syncQuality-=((fs*3)/25);
  }
  if(missingLines>=maxOutOfSyncLines())
  {
    str+=QString("missingLines: %1 ").arg(missingLines);
    syncQuality=-1;
  }
  syncQuality-=(falseSlantSync*2);
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
  return QString("proc=%1\tvia=%2\tage_s=%3\thits=%4\tfar_hits=%5\tfreezes=%6\tresyncs=%7\tmax_gap_s=%8\thits_per_line=%9\tnear_hits=%10\tproven=%11")
      .arg(detectNarrow?"N":"W").arg(trViaVis?"vis":"sync").arg(age,0,'f',1).arg(trHits).arg(trFarHits).arg(trFreezes).arg(trResyncs)
      .arg(trMaxGapLines*samplesPerLine/SAMPLERATE,0,'f',1).arg(ageLines>0?trHits/ageLines:0.,0,'f',3).arg(nearHits).arg(proven);
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

  if(syncArray[syncArrayIndex].retrace)
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
  if(syncProcesState==MODEDETECT)
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

  //  addToLog(QString("regr. params line %1 a:%2 b:%3").arg(slantAdjustLine).arg(a).arg(b),LOGSLANT);
  slantAdjustLine+=5;
  if(!autoSlantAdjust) return false;
  if(initial)
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
