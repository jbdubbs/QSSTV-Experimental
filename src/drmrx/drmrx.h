#ifndef DRMRX_H
#define DRMRX_H

#include <QObject>
#include "appdefs.h"
#include "drmdefs.h"
#include "filters.h"


//! receive statistics of one decode (headless test harness: tests/drm/run.sh); counted per RX stripe
struct drmRxStats
{
  long mscBlocks=0,pktCrcOk=0,pktCrcBad=0,pktBad=0,hdrSeg=0,dataSeg=0,newSeg=0;
  long stripes=0,timeSync=0,frameSync=0,facValid=0,mscValid=0,mscFlaps=0;
  double merSum=0;
  int merCount=0;
  double merMscSum=0;
  int merMscCount=0;
  int mode=-1,occupancy=-1;
  bool prevMsc=false;
  //! equalised MSC cell amplitude per carrier (sum of |z|, count): a flat profile means the channel estimate is right
  double carrierSum[512]={0};
  long carrierN[512]={0};
  //! largest relative deviation of a carrier's mean |z| from the overall mean (carriers with enough cells); 0 when too few cells
  double ampDeviation() const
  {
    double sum=0;long n=0;
    for(int c=0;c<512;c++) {sum+=carrierSum[c];n+=carrierN[c];}
    if(n<1000) return 0;
    const double mean=sum/n;
    double dev=0;
    for(int c=0;c<512;c++)
      {
        if(carrierN[c]<200) continue;
        const double d=carrierSum[c]/carrierN[c]/mean-1;
        if(d>dev) dev=d;
        if(-d>dev) dev=-d;
      }
    return dev;
  }
  void reset() {*this=drmRxStats();}
};
extern drmRxStats drmStats;

class drmRx : public QObject
{
  Q_OBJECT
public:
  explicit drmRx(QObject *parent = 0);
  ~drmRx();
  void init();
  void run(DSPFLOAT *dataPtr);
  void eraseImage(){}

signals:

public slots:
  private:
  int n,im;
  float rRation;
  float resamp_signal[2 * DRMBUFSIZE];
  drmHilbertFilter iqFilter;
};

#endif // DRMRX_H
