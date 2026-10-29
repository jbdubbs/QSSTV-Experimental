#ifndef SYNCPROCESSOR_H
#define SYNCPROCESSOR_H

#include <sstvparam.h>
#include "visfskid.h"
#include <QObject>
#include <QList>
#include "filters.h"

#define MAXSYNCENTRIES 2048
#define STATESCALER 100



class modeBase;


struct ssenitivity
{
  unsigned int minMatchedLines;
  unsigned int maxLineDistanceModeDetect;
  DSPFLOAT onRatio;
  DSPFLOAT offRatio;
  int minVolume;
  int maxTempOutOfSyncLines;
  int maxOutOfSyncLines;
  DSPFLOAT minOutOfSyncSeconds;   // a fade this long does not end the picture (as long as the picture is still running)
  DSPFLOAT lineTolerance;         // how far (fraction of a line) a pulse may be from the line grid of the picture and still count
};

struct ssyncEntry
{
  ssyncEntry()
  {
    init();
  }

  void init()
  {
    start=0;
    end=0;
    startVolume=0;
    maxVolume=0;
    width=0;
    inUse=false;
    retrace=false;
    lineNumber=0;
    length=0;
  }
  int diffStartEnd()
  {
    width=end-start;
    return width;
  }
  uint start;
  uint startVolume;
  uint end;
  uint maxVolume;
  uint width;
  bool inUse;
  bool retrace;
  uint lineNumber;
  uint length;
};

struct smatchEntry
{
  smatchEntry()
  {
    init();
  }

  smatchEntry(uint fromIdx,uint toIdx,uint lineSpace,double fract,uint endFromSample,uint endToSample)
  {
    from=fromIdx;
    to=toIdx;
    lineSpacing=lineSpace;
    fraction=fract;
    endFrom=endFromSample;
    endTo=endToSample;
  }
  void init()
  {
    from=0;
    to=0;
    lineSpacing=0;
    fraction=0;
    endFrom=0;
    endTo=0;
  }
  uint from; /**< the from index pointing to the syncArray */
  uint to;   /**< the to index pointing to  the syncArray */
  uint lineSpacing;
  double fraction;
  uint endFrom; /**< sampleCounter From */
  uint endTo; /**< sampleCounter To */
};

struct sslantXY
{
  DSPFLOAT x;
  DSPFLOAT y;
};

typedef QList<smatchEntry *> modeMatchList;
typedef QList<modeMatchList *>  modeMatchChain;

class syncProcessor : public QObject
{
  Q_OBJECT
public:
  //  enum esyncState {SYNCOFF,SYNCUP,SYNCSTART,SYNCON,SYNCDOWN,SYNCEND,SYNCVALID};
  enum esyncState {SYNCOFF,SYNCACTIVE,SYNCVALID};
  enum esyncProcessState {MODEDETECT,INSYNC,SYNCLOSTNEWMODE,SYNCLOSTFALSESYNC,SYNCLOSTMISSINGLINES,SYNCLOST,RETRACEWAIT};
  explicit syncProcessor(bool narrow,QObject *parent = 0);
  ~syncProcessor();
  void init();
  void reset();
  void process();
  esyncProcessState getSyncState(quint32  &syncPos) {syncPos=syncPosition; return syncProcesState; }
  esstvMode getMode() {return currentMode;}
  void resetRetraceFlag(uint rewound=0);
  bool hasNewClock()
  {
    bool nc=newClock;
    newClock=false;
    return nc;
  }
  void clear();
  void recalculateMatchArray();
  DSPFLOAT getNewClock() {return modifiedClock;}
  void setSyncDetectionEnabled(bool enable) {enableSyncDetection=enable;}
  QString traceSummary();


  quint32 sampleCounter;
  quint32 syncPosition;
  quint32 lastValidSyncCounter;
  //  DSPFLOAT trackMax;
  int      syncQuality;
  modeBase *currentModePtr;

  quint16      *freqPtr;
  const float  *audioPtr;          // what the receiver hears (the VIS tone detector)
  DSPFLOAT     *syncVolumePtr;
  DSPFLOAT     *inputVolumePtr;
  videoFilter  *videoFilterPtr;
  bool         retraceFlag;
  bool tempOutOfSync;

#ifndef QT_NO_DEBUG
  void setOffset(unsigned int dataScopeOffset);
  unsigned int xOffset;
  unsigned int syncStateBuffer[RXSTRIPE];
#endif

public slots:
  void slotNewCall(QString call);
  void slotVisCodeDetected(int, uint visSampleCounter);

signals:
  void callReceived(QString);

private:
  quint32 maxLineSamples;
  quint16 syncArrayIndex;
  ssyncEntry syncArray[MAXSYNCENTRIES];
  modeMatchChain matchArray[NOTVALID];
  quint16 slantAdjustLine;
  bool clockAdjusted;        // a clock correction has been accepted for the picture being received
  uint slantPoints;          // pulses the last regression used, and the lines they span
  double slantSpanLines;
  double slantOffset0;
  esstvMode currentMode;
  esyncState syncState;
  esyncProcessState syncProcesState;
  bool newClock;
  esstvMode idxStart;
  esstvMode idxEnd;
  DSPFLOAT modifiedClock;
  DSPFLOAT samplesPerLine;
  streamDecoder streamDecode;
  DSPFLOAT lineTolerance;
  modeMatchList *activeChainPtr;

  bool currentModeMatchChanged;
  uint lastSyncTest;
  uint lastUpdatedSync;
  sslantXY slantXYArray[MAXSYNCENTRIES];
  esstvMode visMode;

  void extractSync();
  bool validateSync();
  bool findMatch();
  bool addToMatch(esstvMode mode);
  bool addToChain(esstvMode mode, uint fromIdx);
  bool lineCompare(DSPFLOAT samPerLine, int srcIdx, int dstIdx, quint16 &lineNumber, double &fraction);
  void switchSyncState(esyncState newState, quint32 sampleCntr);
  void switchProcessState(esyncProcessState  newState);
  uint calcTotalLines(modeMatchList *mlPtr);
  double calcTotalFract(modeMatchList *mlPtr);
  void clearMatchArray();
  void removeMatchArrayChain(esstvMode mode,int chainIdx);
  void cleanupMatchArray();
  void dropTop();
  void deleteSyncArrayEntry(uint entry);
  bool createModeBase();
  void checkSyncArray();
  void trackSyncs();
  bool reanchorChain();
  void calcSyncQuality();
  uint maxOutOfSyncLines();
  uint visBackfill();
  void startFromVis();
  bool adoptChain();
  void dropChains();
  void calculateLineNumber(uint fromIdx, uint toIdx);
  void regression(DSPFLOAT &a, DSPFLOAT &b, bool initial);
  bool slantAdjust(bool initial);
  DSPFLOAT syncWidth;


  // signal quality
  quint16 falseSlantSync;
  quint16 falseSyncs;
  quint16 missingLines;

  // how well the picture being received has proven itself, and which pulses count for its wait (issue #135)
  uint nearHits;             // pulses at most NEARLINES lines after the last one since detection
  uint nearRun;              // ... in a row; starts again when the display freezes
  bool proven;               // started by a fresh VIS code, or enough near pulses
  int trustedEnd;            // end of the last pulse that restarts the wait for sync
  uint visBackfillLines;     // sync intervals the picture was started before its first pulse, at its VIS code (0: at the pulse)
  bool visAnchored;          // the picture was started by its VIS code and runs by the clock: it never gives up, pulses only refine its position
  bool pendingVisStart;      // a VIS code was received: start the picture as soon as the stripe with its first line has been processed
  struct svisRestart {bool valid; esstvMode mode; uint counter;} visRestart;   // a VIS code that arrived during a picture: starts the next one

  // receiver trace (rxtrace.h, QSSTV_RX_TRACE): only written, never read by the receiver
  void traceStats();
  void traceLost(const char *reason);
  quint32 trDetectCounter;   // sampleCounter when the picture was detected
  quint32 trFreezeCounter;   // sampleCounter when the display froze
  bool trViaVis;             // the picture was started with a VIS code (also decides `proven`, below)
  uint trHits;               // pulses added to the chain since detection
  uint trFarHits;            // ... of which more than 2 lines after the last one
  uint trFreezes;
  uint trResyncs;
  uint trMaxGapLines;
  uint trStripes;            // PULSE-STATS: stripes in this minute, sync-like pulses started / accepted, retrace pulses, VIS codes
  uint trStarts;
  uint trAccepted;
  uint trRetrace;
  uint trVis;
  uint trGateOpen;           // samples with an input level of at least minVolume
  double trLevelSum;
  DSPFLOAT trLevelPeak;

  bool detectNarrow;
  bool enableSyncDetection;
  uint  minMatchedLines;
  uint  visEndCounter;
//  DSPFLOAT syncAvgPtr[RXSTRIPE];
//  DSPFLOAT syncAvg;

};

#endif // SYNCPROCESSOR_H




