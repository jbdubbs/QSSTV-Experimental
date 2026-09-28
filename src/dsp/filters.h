#ifndef FILTERS_H
#define FILTERS_H
#include "filter.h"


class syncFilter
{
public:
  syncFilter(uint maxLength);
  ~syncFilter();
  void process(FILTERPARAMTYPE *dataPtr);
  void init();
  FILTERPARAMTYPE *detect1200Ptr;
  FILTERPARAMTYPE *detect1900Ptr;
private:
  filter sync1200;
  filter sync1900;
  filter sync1200lp;
  filter sync1900lp;


};


/*!
  FM demodulator for the picture data. The standard filter (about +/-600 Hz) suits the slow modes, but it smears
  a 190 us pixel over about four pixels. The wide variant (about +/-1000 Hz, same 181 taps so the same group
  delay) is sharper on clean signals and noisier on weak ones; it is meant for the fast modes.
*/
class videoFilter
{
public:
  videoFilter(uint maxLength,bool wide=false);
  ~videoFilter();
  void process(FILTERPARAMTYPE *dataPtr);
  void init();
  quint16 *demodPtr;
private:
  static void designWideTaps(FILTERPARAMTYPE *taps);
  filter videoFltr;
  filter lpFltr;
  bool wideFilter;
};

class wfFilter
{
public:
  wfFilter(uint maxLength);
  ~wfFilter();
  void process(FILTERPARAMTYPE *dataPtr, uint dataLength=RXSTRIPE);
  void init();
private:
  filter wfFltr;
};

class drmHilbertFilter
{
public:
  drmHilbertFilter(uint maxLength);
  ~drmHilbertFilter();
//  void process(FILTERPARAMTYPE *dataPtr, uint =RXSTRIPE);
  void process(FILTERPARAMTYPE *dataPtr, float *outputPtr,uint dataLength);
  void init();
private:
  filter drmFltr;
};


#endif // FILTERS_H
