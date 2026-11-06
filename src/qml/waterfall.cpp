#include "waterfall.h"

#include <QColor>
#include <QSettings>
#include <cmath>
#include <cstring>

Waterfall::Waterfall() : m_image(columns,rows,QImage::Format_RGB32), m_smooth(columns,-100.0)
{
  m_image.fill(Qt::black);
  QSettings s;   // the levels the Widgets display uses (the Receive page of the desktop interface sets them)
  s.beginGroup("SPECTRUM");
  m_max=s.value("maxdb",-25).toInt();
  m_range=qMax(1,s.value("range",35).toInt());
  m_avg=s.value("avg",0.90).toDouble();
  s.endGroup();
}

void Waterfall::init(int fftLength,int samplingRate)
{
  m_fftLength=fftLength;
  const double step=double(samplingRate)/double(qMax(1,fftLength));
  m_begin=int(std::rint(lowHz/step));
  m_end=int(std::rint(highHz/step));
}

void Waterfall::add(const double *fft)
{
  if(m_fftLength<=0 || m_end<=m_begin || !fft) return;
  // the older rows move down by one
  std::memmove(m_image.scanLine(1),m_image.scanLine(0),size_t(m_image.bytesPerLine())*size_t(rows-1));
  QRgb *row=reinterpret_cast<QRgb *>(m_image.scanLine(0));
  const int span=m_end-m_begin;
  int bin=m_begin;
  for(int j=0;j<columns;j++)
    {
      const int last=m_begin+((j+1)*span)/columns;   // the bins of this column: bin .. last-1 (at least one)
      double power=0;
      int n=0;
      do
        {
          const double re=fft[bin]/m_fftLength;
          const double im=fft[m_fftLength-bin]/m_fftLength;
          power+=re*re+im*im;
          n++;
          bin++;
        }
      while(bin<last);
      double db=10*std::log10(power/n+1e-30)-77.27;   // 0.5 Vtt is 0 dB, as in the Widgets display
      m_smooth[size_t(j)]=m_smooth[size_t(j)]*(1-m_avg)+m_avg*db;
      if(m_smooth[size_t(j)]<-100) m_smooth[size_t(j)]=-100;
      double t=1-(m_max-m_smooth[size_t(j)])/m_range;
      t=qBound(0.0,t,1.0);
      QColor c;
      c.setHsv(int(240-t*60),255,int(t*255));
      row[j]=c.rgb();
    }
}
