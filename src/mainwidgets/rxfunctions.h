#ifndef RXFUNCTIONS_H
#define RXFUNCTIONS_H

#include <QThread>
#include "appdefs.h"
#include "sstv/syncprocessor.h"
#include "buffermanag.h"




class downsampleFilter;
class iirFilter;
class modeBase;
class sstvRx;
class drmRx;
class MmsstvSstvRx;

class rxFunctions : public QThread
{

  Q_OBJECT
public:
  enum erxState {RXIDLE,RXRUNNING,RXRESTART,RXINIT};
  explicit rxFunctions(QObject *parent = 0);
  ~rxFunctions();
  void run();
  void init();
  void stopAndWait();
  void startRX();
  void restartRX();
  void eraseImage();
  void switchEngine();
  void resetCoreLock();   // drop mmsstv-core's demodulator lock (without clearing the picture)
  QString getModeStr();
  sstvRx  *sstvRxPtr;
  void stopThread();
  bool rxBusy();
  bool isIdle() const {return rxState==RXIDLE;}

#ifndef QT_NO_DEBUG
  unsigned int setOffset(unsigned int offset,bool ask);
#endif
private:

  drmRx *drmRxPtr;
  bool abort;
  erxState rxState;
  void switchRxState(erxState newState);
  uint rxBytes;
  void forceInit();

  // mmsstv-linux-port Step 7/8: independent RX path via mmsstv-core for
  // whichever modes it supports, drained alongside (not instead of)
  // sstvRxPtr's own decimated-pipeline dispatch. See sstv/mmsstv_sstv_rx.h.
  MmsstvSstvRx *mmsstvRxPtr;

};

#endif // RXFUNCTIONS_H
