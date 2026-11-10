// Camera frames: a phone's frame carries a rotation and a mirror flag that QVideoFrame::toImage() does not apply, and
// videoFrameImage() does. Pictures are checked pixel by pixel against the rotation worked out by hand (clockwise, then mirrored
// left to right, the order the Qt documentation gives), and a frame with the transformation in its surface format (which
// toImage() does apply) must not be turned a second time.
#include "frameimage.h"
#include <QGuiApplication>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <cstdio>

static int fails=0;
#define CHECK(c) do{ if(!(c)){ fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); fails++; } }while(0)

static const int W=6,H=4;

// every pixel names its own place: red = x, green = y
static QImage card()
{
  QImage img(W,H,QImage::Format_RGB32);
  for(int y=0;y<H;y++) for(int x=0;x<W;x++) img.setPixel(x,y,qRgb(x*40,y*60,99));
  return img;
}

// where pixel (x,y) of the card ends up: turned clockwise by deg, then mirrored left to right
static QPoint place(int x,int y,int deg,bool mir,QSize &outSize)
{
  int nx=x,ny=y,nw=W,nh=H;
  switch(deg)
    {
    case 90:  nx=H-1-y; ny=x; nw=H; nh=W; break;
    case 180: nx=W-1-x; ny=H-1-y; break;
    case 270: nx=y; ny=W-1-x; nw=H; nh=W; break;
    default: break;
    }
  if(mir) nx=nw-1-nx;
  outSize=QSize(nw,nh);
  return QPoint(nx,ny);
}

static bool matches(const QImage &got,const QImage &src,int deg,bool mir,bool say=true)
{
  QSize sz;
  place(0,0,deg,mir,sz);
  if(got.size()!=sz) { if(say) fprintf(stderr,"  size %dx%d, wanted %dx%d (rotation %d mirrored %d)\n",got.width(),got.height(),sz.width(),sz.height(),deg,mir); return false; }
  for(int y=0;y<H;y++) for(int x=0;x<W;x++)
    {
      QPoint p=place(x,y,deg,mir,sz);
      if((got.pixel(p)&0xffffff)!=(src.pixel(x,y)&0xffffff))
        { if(say) fprintf(stderr,"  rotation %d mirrored %d: pixel (%d,%d) is not at (%d,%d)\n",deg,mir,x,y,p.x(),p.y()); return false; }
    }
  return true;
}

int main(int argc,char **argv)
{
  QGuiApplication app(argc,argv);
  const QImage src=card();

  // the helper alone, all eight orientations
  for(int deg:{0,90,180,270}) for(bool mir:{false,true})
    CHECK(matches(orientedImage(src,deg,mir),src,deg,mir));
  CHECK(matches(orientedImage(src,450,false),src,90,false));    // more than a turn
  CHECK(matches(orientedImage(src,-90,false),src,270,false));   // and the other way
  CHECK(orientedImage(QImage(),90,true).isNull());

  // through a real frame, as the camera hands it over
  for(int deg:{0,90,180,270}) for(bool mir:{false,true})
    {
      QVideoFrame frame(src);
      CHECK(frame.isValid());
      frame.setRotation(QtVideo::Rotation(deg));
      frame.setMirrored(mir);
      CHECK(matches(videoFrameImage(frame),src,deg,mir));
      // what we are fixing: toImage() leaves the frame as it came
      if(deg!=0 || mir) CHECK(!matches(frame.toImage(),src,deg,mir,false));
    }

  // the transformation of the surface format is applied by toImage() already: once is enough
  {
    QVideoFrameFormat fmt=QVideoFrame(src).surfaceFormat();
    fmt.setRotation(QtVideo::Rotation::Clockwise90);
    QVideoFrame frame(fmt);
    CHECK(frame.map(QVideoFrame::WriteOnly));
    memset(frame.bits(0),0x55,size_t(frame.mappedBytes(0)));
    frame.unmap();
    CHECK(frame.toImage().size()==QSize(H,W));          // toImage() turned it
    CHECK(videoFrameImage(frame).size()==QSize(H,W));   // and we did not turn it again
  }

  if(fails) { fprintf(stderr,"camorient: %d checks failed\n",fails); return 1; }
  printf("camorient: all checks passed\n");
  return 0;
}
