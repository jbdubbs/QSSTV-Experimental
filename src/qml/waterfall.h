#ifndef WATERFALL_H
#define WATERFALL_H

#include <QImage>
#include <vector>

/*!
  The waterfall of the Qt Quick Receive screen: the spectrum of the received sound between 200 and 2900 Hz, one new row at the top for
  every FFT the receiver sends, the older rows pushed down. Same arithmetic and colours as the Widgets display (widgets/fftdisplay.cpp),
  but its own picture of a fixed size, so it does not depend on the size of a widget that is never shown.
*/
class Waterfall
{
public:
  static constexpr int lowHz=200;
  static constexpr int highHz=2900;
  static constexpr int columns=400;
  static constexpr int rows=128;   //!< the strip is about twice as tall as it was (#181): about the same dp per row, twice the history

  Waterfall();
  //! the FFT of the receiver: fftLength points at samplingRate Hz
  void init(int fftLength,int samplingRate);
  //! another spectrum, in the half-complex order fftw gives (fftLength values)
  void add(const double *fft);
  const QImage &image() const {return m_image;}

private:
  QImage m_image;
  int m_fftLength=0;
  int m_begin=0;
  int m_end=0;
  double m_max=-25;
  double m_range=35;
  double m_avg=0.9;
  std::vector<double> m_smooth;
};

#endif // WATERFALL_H
