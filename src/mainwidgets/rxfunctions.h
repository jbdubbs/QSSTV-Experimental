#ifndef RXFUNCTIONS_H
#define RXFUNCTIONS_H

#include <QThread>
#include <atomic>
#include "appdefs.h"
#include "sstv/syncprocessor.h"
#include "buffermanag.h"




class downsampleFilter;
class iirFilter;
class modeBase;
class sstvRx;
class drmRx;

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
  sstvRx  *sstvRxPtr;
  void stopThread();
  bool rxBusy();
  bool isIdle() const {return rxState==RXIDLE;}

#ifndef QT_NO_DEBUG
  unsigned int setOffset(unsigned int offset,bool ask);
#endif
private:

  drmRx *drmRxPtr;
  std::atomic_bool abort{false};
  std::atomic<erxState> rxState;
  void switchRxState(erxState newState);
  uint rxBytes;
  void forceInit();

};

#endif // RXFUNCTIONS_H
