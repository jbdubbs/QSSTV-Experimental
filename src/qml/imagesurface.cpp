#include "imagesurface.h"
#include "rxcontroller.h"

#include <QPainter>

ImageSurface::ImageSurface(QQuickItem *parent) : QQuickPaintedItem(parent)
{
  setOpaquePainting(false);
  setRenderTarget(QQuickPaintedItem::Image);   // painted with the CPU and uploaded as a texture: no framebuffer objects needed
}

const QImage &ImageSurface::image() const
{
  return m_source==1 ? RxController::instance()->waterfall() : RxController::instance()->picture();
}

bool ImageSurface::empty() const
{
  return image().isNull();
}

void ImageSurface::setSource(int s)
{
  if(s==m_source) return;
  m_source=s;
  emit sourceChanged();
  emit revisionChanged();
  update();
}

void ImageSurface::setRevision(int r)
{
  if(r==m_revision) return;
  m_revision=r;
  emit revisionChanged();
  update();
}

void ImageSurface::setFill(bool f)
{
  if(f==m_fill) return;
  m_fill=f;
  emit fillChanged();
  update();
}

void ImageSurface::paint(QPainter *painter)
{
  const QImage &im=image();
  if(im.isNull() || width()<1 || height()<1) return;
  painter->setRenderHint(QPainter::SmoothPixmapTransform,true);
  const QSizeF target(width(),height());
  QSizeF drawn=QSizeF(im.size()).scaled(target,m_fill ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio);
  const QRectF r((target.width()-drawn.width())/2,(target.height()-drawn.height())/2,drawn.width(),drawn.height());
  painter->setClipRect(QRectF(QPointF(0,0),target));
  painter->drawImage(r,im);
}
