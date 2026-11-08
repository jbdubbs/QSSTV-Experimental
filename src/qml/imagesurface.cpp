#include "imagesurface.h"
#include "rxcontroller.h"
#include "txcontroller.h"

#include <QPainter>

ImageSurface::ImageSurface(QQuickItem *parent) : QQuickPaintedItem(parent)
{
  setOpaquePainting(false);
  setRenderTarget(QQuickPaintedItem::Image);   // painted with the CPU and uploaded as a texture: no framebuffer objects needed
}

const QImage &ImageSurface::image() const
{
  if(m_source==2) return TxController::instance()->picture();
  return m_source==1 ? RxController::instance()->waterfall() : RxController::instance()->picture();
}

bool ImageSurface::empty() const
{
  return image().isNull();
}

QSizeF ImageSurface::painted() const
{
  const QImage &im=image();
  if(im.isNull() || width()<1 || height()<1) return QSizeF(0,0);
  return QSizeF(im.size()).scaled(QSizeF(width(),height()),m_fill ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio);
}

void ImageSurface::geometryChange(const QRectF &newGeometry,const QRectF &oldGeometry)
{
  QQuickPaintedItem::geometryChange(newGeometry,oldGeometry);
  if(newGeometry.size()!=oldGeometry.size()) emit paintedChanged();
}

void ImageSurface::setSource(int s)
{
  if(s==m_source) return;
  m_source=s;
  m_tinted=QImage();
  emit sourceChanged();
  emit revisionChanged();
  emit paintedChanged();
  update();
}

void ImageSurface::setRevision(int r)
{
  if(r==m_revision) return;
  m_revision=r;
  emit revisionChanged();
  emit paintedChanged();
  update();
}

void ImageSurface::setFill(bool f)
{
  if(f==m_fill) return;
  m_fill=f;
  emit fillChanged();
  emit paintedChanged();
  update();
}

void ImageSurface::setTint(bool t)
{
  if(t==m_tint) return;
  m_tint=t;
  m_tinted=QImage();
  emit tintChanged();
  update();
}

QImage ImageSurface::tinted(const QImage &image)
{
  QImage in=image.convertToFormat(QImage::Format_RGB32);
  QImage out(in.size(),QImage::Format_RGB32);
  for(int y=0;y<in.height();y++)
    {
      const QRgb *src=reinterpret_cast<const QRgb *>(in.constScanLine(y));
      QRgb *dst=reinterpret_cast<QRgb *>(out.scanLine(y));
      for(int x=0;x<in.width();x++)
        {
          const int l=(77*qRed(src[x])+151*qGreen(src[x])+28*qBlue(src[x]))>>8;   // the light of the pixel, 0 to 255
          dst[x]=qRgb(l*9/10,l*2/9,l*1/7);                                         // a deep red, a little dimmer than the picture was
        }
    }
  return out;
}

void ImageSurface::paint(QPainter *painter)
{
  const QImage &plain=image();
  const QImage *shown=&plain;
  if(m_tint && !plain.isNull())
    {
      // a picture that grows line by line is made red again for each revision, not for each repaint
      if(m_tinted.isNull() || m_tintedRevision!=m_revision || m_tinted.size()!=plain.size() || m_tintedKey!=plain.cacheKey())
        {
          m_tinted=tinted(plain);
          m_tintedRevision=m_revision;
          m_tintedKey=plain.cacheKey();
        }
      shown=&m_tinted;
    }
  const QImage &im=*shown;
  if(im.isNull() || width()<1 || height()<1) return;
  painter->setRenderHint(QPainter::SmoothPixmapTransform,true);
  const QSizeF target(width(),height());
  QSizeF drawn=QSizeF(im.size()).scaled(target,m_fill ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio);
  const QRectF r((target.width()-drawn.width())/2,(target.height()-drawn.height())/2,drawn.width(),drawn.height());
  painter->setClipRect(QRectF(QPointF(0,0),target));
  painter->drawImage(r,im);
}
