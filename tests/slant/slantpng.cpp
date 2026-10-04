// Helper for the loopback check of the calibration slant measurement:
//   slantpng make out.png W H x      a black picture with a 3 pixel white vertical line at column x
//   slantpng fit in.png              fit the line in a decoded picture, print slope (px/row), rms, used rows
#include "slantfit.h"
#include <QImage>
#include <QGuiApplication>
#include <cstdio>
#include <cstring>
#include <cstdlib>

int main(int argc,char **argv)
{
  QGuiApplication app(argc,argv);
  if(argc>=6 && !strcmp(argv[1],"make"))
    {
      int w=atoi(argv[3]),h=atoi(argv[4]),x=atoi(argv[5]);
      QImage im(w,h,QImage::Format_RGB32);
      im.fill(Qt::black);
      for(int y=0;y<h;y++) for(int i=x-1;i<=x+1;i++) im.setPixel(i,y,qRgb(255,255,255));
      return im.save(argv[2])? 0:1;
    }
  if(argc>=3 && !strcmp(argv[1],"fit"))
    {
      QImage g=QImage(argv[2]).convertToFormat(QImage::Format_Grayscale8);
      if(g.isNull()) {fprintf(stderr,"cannot read %s\n",argv[2]);return 1;}
      slantFitResult r=fitSlant(g.constBits(),g.width(),g.height(),g.bytesPerLine());
      printf("valid=%d slope=%.6f rms=%.3f used=%d found=%d total=%d\n",r.valid,r.slope,r.rms,r.used,r.found,r.total);
      return r.valid? 0:1;
    }
  fprintf(stderr,"usage: slantpng make out.png W H x | slantpng fit in.png\n");
  return 2;
}
