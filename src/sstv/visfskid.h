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
  enum evisState  {VISINIT,WAITSTART1900,WAITEND1900,WAITSTART2100,WAITEND2100,WAITSTARTBIT,GETCODE};   // the narrow code
  visDecoder();
  void extract(unsigned int syncSampleCtr, bool narrow);
  void extractNarrow();
  void reset();
  uint getCode();
  esstvMode mode;
signals:
  void visCodeNarrowDetected(int,uint);

private:
  void switchState(evisState   newState, unsigned int i);
  bool leaderEnded(unsigned int i);
  void leaderDropout(unsigned int i);
  evisState  visState;
  unsigned int leaderTotal;   //!< samples of 1900 Hz leader seen so far (runs of at least VISMINLEADERRUN)
  unsigned int leaderGap;     //!< samples since the leader was last seen, while waiting for it to come back
};




/**
 * The wide VIS code, found from the strength of its tones in the audio instead of the frequency the FM demodulator makes of it.
 *
 * The frequency of a weak signal is noise (an FM demodulator needs about +15 dB SNR, and that is what the frequency based decoder
 * above needed). The tones are found by resonators 80-100 Hz wide after a limiter, which keeps working down to about -3 dB SNR in
 * 2.4 kHz; the same method is used by MMSSTV (Makoto Mori, JE3HHT), the code here is written from the method, not from its source.
 * No 1900 Hz leader is needed: the start bit (15 ms of 1200 Hz stronger than 1900 Hz), the 8 or 16 data bits (1100 Hz = 1 stronger
 * than 1300 Hz = 0, sampled in the middle of each 30 ms) and the 1200 Hz stop bit are enough.
 */

#define VTHIST 720      // 60 ms of strengths
#define VTLEAD 5400     // 450 ms: where the 1900 Hz leader before the start bit is looked for

class visToneDecoder : public QObject
{
  Q_OBJECT
public:
  visToneDecoder();
  void reset();
  void process(const float *audio, unsigned int syncSampleCtr);
signals:
  void visCodeWideDetected(int,uint);
private:
  struct biquad
  {
    double b0,b1,b2,a1,a2,z1,z2;
    void lowpass(double f,double q);
    void highpass(double f,double q);
    double run(double x);
    void clear() {z1=z2=0;}
  };
  struct tank   // two pole resonator, rectifier and a 50 Hz low-pass: the strength of one tone
  {
    double a0,b1,b2,z1,z2,scale;
    uint t50;                 // samples a tone needs to bring the strength to half of its end value
    biquad lp;
    void init(double f,double bw);
    double run(double x);
    void clear();
  };
  enum evstate {IDLE,STARTBIT,BITS,STOP};
  /**
   * One set of tone detectors and the state of a code being read with them. A transmitter or a receiver that is some tens of Hz off
   * frequency moves all tones; a weak signal is only found when its tones are in the resonators, so there are several banks, a
   * few tens of Hz apart, side by side (the first that reads a code wins).
   */
  struct bank
  {
    tank t1080,t1200,t1320,t1900;
    evstate state;
    uint runStart;            // localPos where d1200 stayed over the start bit level
    uint onset;               // localPos of the estimated beginning of the start bit (when the bits are read)
    uint onsetEmit;           // ... and the one the first line is timed from: where the 1200 Hz strength has risen half way, if it was clear
    double trimMs;            // what has to be added to the time a code ends for the way onsetEmit was found
    double plateau;           // how strong the 1200 Hz of the start bit was in this bank: the bank that is tuned best times the code best
    uint bitIndex,bitsWanted;
    uint symbol;
    double sum11,sum13,sum19;
    uint sumCount;
    double stopSum12,stopSum19;
    uint stopCount;
    float hist[VTHIST];       // the last strengths of 1200 Hz: the start bit's edge is found in them
    uint histPos;
    unsigned char lead[VTLEAD];   // 1 where 1900 Hz was stronger than 1200 Hz: a code is only looked for after a leader
    uint leadPos,leadCount;
    void init(double offset);
    void clear();
    void startOver();
  };
  bool step(bank &b,double y,uint localPos,uint syncSampleCtr,int i);
  struct scandidate     // a code that has been read; the banks finish within a few ms of each other, the best tuned one is reported
  {
    bool valid;
    int mode;
    int counter;
    double plateau;
    uint due;             // localPos at which the code is reported
  } candidate;

  biquad pre[4];            // band-pass in front of the limiter
  bank banks[3];
  uint localPos;            // samples seen since the last reset; only differences are used (the sample counter jumps back on a rewind)
  uint quiet;               // samples to stay idle after a code (the same code is not read twice)
};


class streamDecoder
{
public:
  streamDecoder(bool narrow);
  void reset();
  void process(quint16 *freqPtr, const float *audioPtr, unsigned int syncSampleCtr);
  uint getVisCode() {return visCoder.getCode();}
  fskIdDecoder *getFskDecoderPtr() {return &fskCoder;}
  visDecoder *getVisDecoderPtr() {return &visCoder;}
  visToneDecoder *getToneVisPtr() {return &toneVisCoder;}
private:
  DSPFLOAT avgFreq;
  DSPFLOAT avgBuffer[RXSTRIPE];
  fskIdDecoder fskCoder;
  visDecoder visCoder;
  visToneDecoder toneVisCoder;
//  retraceDetector retracer;
  quint16 freqPtr();
  bool isNarrow;
};


#endif // VISFSKID_H
