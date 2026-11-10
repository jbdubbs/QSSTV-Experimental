#include "thumbnailprovider.h"
#include "templatedoc.h"
#include "templaterender.h"
#include "supportfunctions.h"

#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <QUrl>

QString ThumbnailProvider::idFor(const QString &file)
{
  const QFileInfo fi(file);
  return QString::fromLatin1(QUrl::toPercentEncoding(file))+QLatin1Char('?')+QString::number(fi.lastModified().toSecsSinceEpoch());
}

static QImage templatePicture(const QString &file,int longSide)
{
  const tpl::LoadResult r=tpl::loadFile(file);
  const QSize size(longSide,longSide*4/5);
  QImage base(size,QImage::Format_RGB32);
  base.fill(QColor(120,128,136));
  if(!r.ok) return base;
  tpl::RenderOptions options;
  options.expandMacros=false;   // a thumbnail shows the template as written
  return tpl::overlay(r.doc,base,options);
}

QImage ThumbnailProvider::requestImage(const QString &id,QSize *size,const QSize &requestedSize)
{
  Q_UNUSED(requestedSize);
  const int q=id.indexOf(QLatin1Char('?'));
  const QString file=QUrl::fromPercentEncoding((q<0 ? id : id.left(q)).toLatin1());
  QImage image;
  const QString suffix=QFileInfo(file).suffix().toLower();
  if(suffix==QLatin1String("qtempl") || suffix==QLatin1String("templ"))
    {
      image=templatePicture(file,m_longSide);
    }
  else
    {
      QImageReader reader(file);
      reader.setAutoTransform(true);
      const QSize original=reader.size();
      if(original.isValid() && qMax(original.width(),original.height())>m_longSide)
        {
          QSize scaled=original;
          scaled.scale(m_longSide,m_longSide,Qt::KeepAspectRatio);
          reader.setScaledSize(scaled);
        }
      image=reader.read();
      if(image.isNull() && loadImageAutoRotate(image,file))
        {
          // no plugin for the format (Android): libavif reads AVIF, OpenJPEG a DRM picture in JPEG2000 (#197). Both decode the whole
          // picture, a DRM one is small.
          if(qMax(image.width(),image.height())>m_longSide)
            image=image.scaled(m_longSide,m_longSide,Qt::KeepAspectRatio,Qt::SmoothTransformation);
        }
    }
  if(size) *size=image.size();
  return image;
}
