#ifndef VISFSKID_H
#define VISFSKID_H
#include "appglobal.h"
#include <QObject>
#include "sstvparam.h"


#define FSKMIN1500 ((SAMPLERATE*100)/1000)
#define FSKMIN2100 ((SAMPLERATE*70)/1000)
#define FSKBIT     ((SAMPLERATE*22)/1000)
#define FSKAVGCOUNT   25
#define VISAVGCOUNT   25

#define FREQAVG 0.05

// The 1900 Hz leader before the VIS code is 300 ms. A fade or a noise burst in it must not hide the code, so the leader does
// not have to be one unbroken tone: runs of at least VISMINLEADERRUN count, dropouts of up to VISLEADERGAP are bridged, and
// VISMINLEADER of leader in all is enough to go on to the start bit.
#define VISMINLEADER     ((SAMPLERATE*100)/1000)
#define VISMINLEADERRUN  ((SAMPLERATE*20)/1000)
#define VISLEADERGAP     ((SAMPLERATE*100)/1000)
#define VISBITWIDE      ((SAMPLERATE*30)/1000)

#define VISBITNARROW      ((SAMPLERATE*22)/1000)




#define MINRETRACEWIDTH ((SAMPLERATE*290)/1000)

class fskDecoder :  public QObject
{
  Q_OBJECT
public:
  fskDecoder();
  virtual void extract(unsigned int syncSampleCtr, bool narrow)=0;
  void setDataPtr(DSPFLOAT *dataPtr) {freqPtr=dataPtr;}
  virtual void reset()=0;


 protected:
  unsigned char bitCounter;
  quint32 symbol;
  quint32 bit;

  unsigned char checksum;
    unsigned int  code;
  unsigned int sampleCounter;
  unsigned int startSampleCounter;
  unsigned int syncSampleCounter;
  unsigned int timeoutCounter;
  bool validCode;
  DSPFLOAT *freqPtr;
  DSPFLOAT avgFreq;
  uint avgCounter;
  uint avgCount;

  bool waitStartFreq(unsigned int freqL, unsigned int freqH);
  bool waitEndFreq(unsigned int freqL,unsigned int freqH);
  bool waitStartFreq(unsigned int freqL,unsigned int freqH,unsigned long maxWait,bool &timeout);
  bool waitEndFreq(unsigned int freqL, unsigned int freqH, unsigned long maxWait, bool &timeout);
  void init();
};

class fskIdDecoder : public fskDecoder
{
  Q_OBJECT
public:
  enum efskState  {FSKINIT,WAITSTART1500,WAITEND1500,WAITSTART1900,WAITEND1900,WAITSTART2100,WAITEND2100,GETID};
  fskIdDecoder();
  void extract(unsigned int syncSampleCtr,bool narrow);
  void reset();

signals:
  void callReceived(QString);

private:
  void switchState(efskState   newState, unsigned int i);
  bool assemble(bool reset);

  QString fskIDStr;
  bool headerFound;
  bool endFound;
  efskState  fskState;
};




class visDecoder : public fskDecoder
{
  Q_OBJECT
public:
  enum evisState  {VISINIT,WAITSTART1200,WAITEND1200,WAITSTART1900,WAITEND1900,WAITSTART2100,WAITEND2100,WAITSTARTBIT,GETCODE};
  visDecoder();
  void extract(unsigned int syncSampleCtr, bool narrow);
  void extractNarrow();
  void extractWide();
  void reset();
  uint getCode();
  esstvMode mode;
signals:
  void visCodeNarrowDetected(int,uint);
  void visCodeWideDetected(int,uint );

private:
  void switchState(evisState   newState, unsigned int i);
  bool leaderEnded(unsigned int i);
  void leaderDropout(unsigned int i);
  evisState  visState;
  unsigned int leaderTotal;   //!< samples of 1900 Hz leader seen so far (runs of at least VISMINLEADERRUN)
  unsigned int leaderGap;     //!< samples since the leader was last seen, while waiting for it to come back
};




class streamDecoder
{
public:
  streamDecoder(bool narrow);
  void reset();
  void process(quint16 *freqPtr, unsigned int syncSampleCtr);
  uint getVisCode() {return visCoder.getCode();}
  fskIdDecoder *getFskDecoderPtr() {return &fskCoder;}
  visDecoder *getVisDecoderPtr() {return &visCoder;}
private:
  DSPFLOAT avgFreq;
  DSPFLOAT avgBuffer[RXSTRIPE];
  fskIdDecoder fskCoder;
  visDecoder visCoder;
//  retraceDetector retracer;
  quint16 freqPtr();
  bool isNarrow;
};


#endif // VISFSKID_H
