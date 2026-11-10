#include "frameimage.h"
#include <QVideoFrame>
#include <QTransform>

QImage orientedImage(const QImage &img,int clockwiseDegrees,bool mirrored)
{
  if(img.isNull()) return img;
  int quarter=(clockwiseDegrees/90)%4;
  if(quarter<0) quarter+=4;
  QImage out=img;
  if(quarter!=0)
    {
      QTransform t;
      t.rotate(quarter*90);   // clockwise on a screen (the y axis points down)
      out=img.transformed(t);
    }
#if QT_VERSION >= QT_VERSION_CHECK(6,9,0)
  if(mirrored) out=out.flipped(Qt::Horizontal);
#else
  if(mirrored) out=out.mirrored(true,false);
#endif
  return out;
}

QImage videoFrameImage(const QVideoFrame &frame)
{
  return orientedImage(frame.toImage(),int(frame.rotation()),frame.mirrored());
}
