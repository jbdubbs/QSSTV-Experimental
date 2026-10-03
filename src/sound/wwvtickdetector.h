#ifndef WWVTICKDETECTOR_H
#define WWVTICKDETECTOR_H

#include <vector>

/*!
  Finds the once-per-second time ticks of WWV/WWVH (a 5 ms burst of 1000 Hz for WWV, 1200 Hz for WWVH,
  with silent guard times before and after it) in a stream of audio samples.

  Pure DSP: no Qt, no sound card, so it can be unit-tested with synthetic audio.

  The stream is mixed down with the tick tone and summed over one tick length (a matched filter). Its
  magnitude peaks when the window exactly covers a tick. A peak is accepted as a tick when it stands out of
  the running noise level and the matched filter output is low again 10 ms before and after the tick (so
  speech, the long minute tone and continuous tones are rejected). The position is interpolated to a
  fraction of a sample.
*/

struct wwvCandidate
{
  double position;   //!< start of the tick, in samples since the first sample processed (fractional)
  double strength;   //!< matched filter magnitude at the peak, relative to the running noise level
};

class wwvTickDetector
{
public:
  wwvTickDetector(double sampleRate,double toneHz=1000.0);
  void setTone(double toneHz);
  void reset();
  /** process n samples; every tick found is appended to out */
  void process(const double *samples,int n,std::vector<wwvCandidate> &out);
  long long samplesProcessed() const {return sampleCount;}

private:
  double fs;
  double tone;
  int window;                  // tick length in samples
  double phase;
  double phaseStep;
  long long sampleCount;
  std::vector<double> ringRe;  // mixed samples of the last `window` samples
  std::vector<double> ringIm;
  double sumRe;
  double sumIm;
  std::vector<double> magRing; // matched filter magnitude of the recent past (to look back 2*window)
  double noise;                // running average of the magnitude
  double prevMag;
  bool inPeak;
  double bestMag;
  long long bestIndex;
  void init();
};

#endif // WWVTICKDETECTOR_H
