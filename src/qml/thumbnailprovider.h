#ifndef THUMBNAILPROVIDER_H
#define THUMBNAILPROVIDER_H

#include <QQuickImageProvider>

/*!
  The pictures of the gallery for QML: image://thumb/<file> (a small one, for the grid) and image://full/<file> (large enough to zoom into).
  The file name is percent-encoded, followed by an optional "?" and the file's modification time, which only makes the cache look again
  when the file changed. A template file (.qtempl, .templ) is drawn over a grey picture, as the transmitter does with its pictures.
  Runs in the image loading thread of Qt Quick: it keeps no state.
*/
class ThumbnailProvider : public QQuickImageProvider
{
public:
  explicit ThumbnailProvider(int longSide) : QQuickImageProvider(QQuickImageProvider::Image), m_longSide(longSide) {}
  QImage requestImage(const QString &id,QSize *size,const QSize &requestedSize) override;
  //! the id to put after image://thumb/ or image://full/ for a file
  static QString idFor(const QString &file);

private:
  int m_longSide;
};

#endif // THUMBNAILPROVIDER_H
