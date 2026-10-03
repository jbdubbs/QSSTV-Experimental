#ifndef WWVTICKFIT_H
#define WWVTICKFIT_H

#include <vector>
#include "wwvtickdetector.h"

/*!
  Turns the tick candidates of wwvTickDetector into a sample rate measurement.

  The true ticks lie on a straight line: position = a + b*k for the tick number k, where b is the number of
  samples between two ticks. WWV sends them exactly one second apart, so b IS the true sample rate of the
  audio stream in Hz. Candidates that are not on the line (speech, interference, the minute tone) are
  rejected, missing ticks (seconds 29 and 59, fades) just leave gaps.

  Pure maths, no Qt, so it can be unit-tested.
*/

struct wwvFitResult
{
  bool valid;          //!< a line has been found and has enough ticks
  double rate;         //!< measured samples per second (Hz)
  double ppm;          //!< (rate/nominal-1)*1e6
  double ppmError;     //!< standard error of ppm from the scatter of the ticks (0 if unknown)
  double rmsMs;        //!< rms deviation of the ticks from the line in milliseconds
  int ticks;           //!< number of ticks on the line
  double spanSeconds;  //!< time between first and last tick on the line
};

struct wwvFitPoint
{
  double position;
  double strength;
  long long k;         //!< tick number relative to the line (valid after solve())
  bool inlier;
};

class wwvTickFit
{
public:
  explicit wwvTickFit(double nominalRate);
  void reset();
  void add(const wwvCandidate &c);
  /** fit the candidates added so far; returns result().valid */
  bool solve();
  const wwvFitResult &result() const {return res;}
  const std::vector<wwvFitPoint> &points() const {return pts;}
  double nominalRate() const {return nominal;}
  /** the fitted line (position of tick k is intercept()+slope()*k); only meaningful when result().valid */
  double intercept() const {return lineA;}
  double slope() const {return lineB;}

private:
  double nominal;
  std::vector<wwvFitPoint> pts;
  bool haveLine;
  double lineA;
  double lineB;
  wwvFitResult res;
  bool acquire();
  bool refine(double startTolerance);
};

#endif // WWVTICKFIT_H
