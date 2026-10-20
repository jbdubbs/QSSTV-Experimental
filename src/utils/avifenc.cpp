#include "avifenc.h"
#include <QBuffer>
#include <QImageWriter>

#define AVIFMINQUALITY 1
#define AVIFMAXQUALITY 99   // 100 is lossless

bool avifWriteSupported()
{
  return QImageWriter::supportedImageFormats().contains("avif");
}

static QByteArray avifAtQuality(const QImage &image,int quality)
{
  QByteArray ba;
  QBuffer buf(&ba);
  buf.open(QIODevice::WriteOnly);
  QImageWriter writer(&buf,"avif");
  writer.setQuality(quality);
  if(!writer.write(image)) return QByteArray();
  return ba;
}

QByteArray avifEncode(const QImage &image,int targetBytes,QImage *decoded,int *qualityUsed)
{
  QByteArray best;
  int bestQuality=0;
  int lo=AVIFMINQUALITY,hi=AVIFMAXQUALITY;
  // size is not strictly monotonic in quality (chroma subsampling changes at the high end), so keep the best fit
  while(lo<=hi)
    {
      int mid=(lo+hi)/2;
      QByteArray d=avifAtQuality(image,mid);
      if(d.isEmpty()) return QByteArray();
      if(d.size()<=targetBytes)
        {
          best=d;
          bestQuality=mid;
          lo=mid+1;
        }
      else hi=mid-1;
    }
  if(best.isEmpty())
    {
      best=avifAtQuality(image,AVIFMINQUALITY);
      bestQuality=AVIFMINQUALITY;
    }
  if(decoded) decoded->loadFromData(best,"avif");
  if(qualityUsed) *qualityUsed=bestQuality;
  return best;
}
