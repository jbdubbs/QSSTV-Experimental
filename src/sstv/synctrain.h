#ifndef SYNCTRAIN_H
#define SYNCTRAIN_H
#include "appglobal.h"
#include "sstvparam.h"
#include "tonefilters.h"

/**
 * A weak picture is found by the line rhythm of its sync pulses, without a VIS code and without the pulse table of the sync processor.
 *
 * The pulse detector of the sync processor compares the 1200 Hz energy with the level of the input and keeps every pulse of 4 to 25 ms
 * in a table; in the band noise of HF there are 16 to 29 such pulses per line, and a chain of four pulses that are a line apart is lost
 * among them (it is refused when 20 other entries lie between its pulses). Here a sync event is a moment where the strength of 1200 Hz
 * is stronger than that of 1900 Hz, after a hard limiter: noise adds about as much to both, so the difference stays near zero between
 * the pulses and there are only about four events per line. One position per event (the peak of the strength), and the intervals
 * between the last events are compared with the line time of each mode (or two or three line times, for lines that were missed).
 *
 * The method is the one of MMSSTV (Makoto Mori, JE3HHT; the same as visToneDecoder uses for the VIS code); the code is written
 * from the method, not from its source. Measured on recordings (qsstv-captures/live/batch/STUDY.md) it starts a picture where the
 * pulse table does not: 28 of 31 real weak pictures against 22, and 0.3 false trains an hour.
 */

#define STMAXPEAKS 9           // events kept: a train of 8 intervals
#define STMAXINTERVALS 8
#define STBANKS 3
#define STBANKSPACING 45       // Hz between the banks of resonators (the signal may be off frequency)

struct strainResult
{
  esstvMode mode;              // the mode whose line time the intervals fit best
  uint count;                  // events of the train, oldest first (back-filled while the intervals keep fitting)
  int peak[STMAXPEAKS];        // their positions in the sample counter of the caller
  uint ambiguous;              // other modes that fit as well
  esstvMode other[4];
};

class syncTrainDetector
{
public:
  syncTrainDetector();
  void reset();
  /** the line times follow the clock of the receiver (samples per second, as for getLineLength) */
  void setClock(double clock);
  /**
   * @brief process one stripe of audio (what the receiver hears)
   * @param audio RXSTRIPE samples at SAMPLERATE
   * @param syncSampleCtr sample counter of the sync processor at the start of the stripe; positions are reported in it
   * @param first the modes that may be found (also: the ones the receiver is set to)
   * @param last ...
   * @param level the strength by which 1200 Hz has to exceed 1900 Hz for a sync event; 0: nothing is looked for
   * @param armed if false the events are followed but no train is reported (the receiver has a picture)
   * @return true if a train has been found (see result())
   */
  bool process(const float *audio,unsigned int syncSampleCtr,esstvMode first,esstvMode last,double level,bool armed);
  const strainResult &result() const {return found;}
  /** the events that ended in the last stripe: their positions in the sample counter of the caller */
  uint freshCount() const {return nFresh;}
  int fresh(uint i) const {return freshPos[i];}
  double lineTime(esstvMode mode) const {return period[mode];}
  /**
   * Where the sync processor finds the pulse of a clean signal, after the event of this detector (ms): its end, and the point it positions
   * the picture by ((start+end)/2 + sync width/2). Measured per mode on transmitted pictures (qsstv-captures/live/batch/calib_delay.py).
   */
  static void pulseDelayMs(esstvMode mode,double &end,double &corrected);

private:
  struct bank
  {
    toneTank t12,t19;
  };
  void event(uint pos,int counterNow,uint localNow,esstvMode first,esstvMode last,bool armed);
  bool match(esstvMode first,esstvMode last,int counterNow,uint localNow);
  int fits(double w,double T) const;

  toneBiquad pre[4];
  bank banks[STBANKS];
  uint localPos;                       // samples seen since the last reset; only differences are used (the sample counter jumps back on a rewind)
  bool inRun;
  double runBest;
  uint runBestPos;
  uint acc[STMAXPEAKS];                // positions of the events kept (localPos)
  uint nAcc;
  uint ivs[STMAXINTERVALS];            // the intervals between them
  uint nIvs;
  bool busy;
  uint busyUntil;                      // localPos; no train is reported before it (the same train would be found on every line)
  double period[NUMSSTVMODES];        // line time of each mode in samples, 0: not a mode that is looked for
  int need[NUMSSTVMODES];              // older intervals that have to fit besides the newest one
  strainResult found;
  bool hit;
  uint nFresh;
  int freshPos[4];
};

#endif // SYNCTRAIN_H
