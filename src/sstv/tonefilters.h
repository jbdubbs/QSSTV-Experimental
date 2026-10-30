#ifndef TONEFILTERS_H
#define TONEFILTERS_H
#include "appglobal.h"
#include <cmath>

/**
 * The filters of the tone detectors (visToneDecoder, syncTrainDetector): the band-pass in front of the limiter and the resonators
 * that give the strength of one tone. They work on the audio at SAMPLERATE after a hard limiter, so noise, a signal and a crash all
 * count the same and what a resonator says is the share of the energy that is at its frequency.
 */

struct toneBiquad
{
  double b0,b1,b2,a1,a2,z1,z2;
  void lowpass(double f,double q)
  {
    double w=2*M_PI*f/SAMPLERATE,al=sin(w)/(2*q),c=cos(w),a0=1+al;
    b0=(1-c)/2/a0; b1=(1-c)/a0; b2=(1-c)/2/a0; a1=-2*c/a0; a2=(1-al)/a0;
    clear();
  }
  void highpass(double f,double q)
  {
    double w=2*M_PI*f/SAMPLERATE,al=sin(w)/(2*q),c=cos(w),a0=1+al;
    b0=(1+c)/2/a0; b1=-(1+c)/a0; b2=(1+c)/2/a0; a1=-2*c/a0; a2=(1-al)/a0;
    clear();
  }
  double run(double x)
  {
    double y=b0*x+z1;
    z1=b1*x-a1*y+z2;
    z2=b2*x-a2*y;
    return y;
  }
  void clear() {z1=z2=0;}
};

/** two pole resonator, rectifier and a 50 Hz low-pass: the strength of one tone (1 = a limited tone in the middle of the resonator) */
struct toneTank
{
  double a0,b1,b2,z1,z2,scale;
  uint t50;                 // samples a tone needs to bring the strength to half of its end value
  toneBiquad lp;
  void init(double f,double bw)
  {
    double r=exp(-M_PI*bw/SAMPLERATE);
    b1=2*r*cos(2*M_PI*f/SAMPLERATE);
    b2=-r*r;
    a0=sin(2*M_PI*f/SAMPLERATE)/((SAMPLERATE/6.0)/bw);
    lp.lowpass(50,0.70710678);
    scale=1;
    clear();
    // the strength of a limited tone (a square wave) at the middle of the resonator is 1
    double d=1;
    for(int i=0;i<SAMPLERATE/2;i++) d=run(sin(2*M_PI*f*i/SAMPLERATE)>=0?1.:-1.);
    scale=1./d;
    clear();
    t50=0;
    for(int i=0;i<SAMPLERATE/10;i++)
      {
        if(run(sin(2*M_PI*f*i/SAMPLERATE)>=0?1.:-1.)>=0.5)
          {
            t50=i;
            break;
          }
      }
    clear();
  }
  double run(double x)
  {
    double y=a0*x+b1*z1+b2*z2;
    z2=z1;
    z1=y;
    return lp.run(fabs(y))*scale;
  }
  void clear()
  {
    z1=z2=0;
    lp.clear();
  }
};

#endif // TONEFILTERS_H
