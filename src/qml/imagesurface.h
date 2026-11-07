#ifndef IMAGESURFACE_H
#define IMAGESURFACE_H

#include <QImage>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

/*!
  Paints one of the pictures the controllers keep (the received picture, the waterfall) fitted into the item, keeping the aspect ratio.
  "ImageSurface" in QML; set `source`, and `revision` to the controller's revision so that a new picture is painted.
*/
class ImageSurface : public QQuickPaintedItem
{
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(int source READ source WRITE setSource NOTIFY sourceChanged)        //!< 0 the received picture, 1 the waterfall, 2 the picture to transmit
  Q_PROPERTY(int revision READ revision WRITE setRevision NOTIFY revisionChanged)
  Q_PROPERTY(bool fill READ fill WRITE setFill NOTIFY fillChanged)               //!< cover the item (crop) instead of fitting inside it
  Q_PROPERTY(bool empty READ empty NOTIFY revisionChanged)                       //!< there is nothing to paint
  Q_PROPERTY(bool tint READ tint WRITE setTint NOTIFY tintChanged)               //!< painted in red only (the Night look): the light of each pixel, no blue or green

public:
  explicit ImageSurface(QQuickItem *parent=nullptr);
  void paint(QPainter *painter) override;
  int source() const {return m_source;}
  void setSource(int s);
  int revision() const {return m_revision;}
  void setRevision(int r);
  bool fill() const {return m_fill;}
  void setFill(bool f);
  bool empty() const;
  bool tint() const {return m_tint;}
  void setTint(bool t);
  //! the picture with only its light left, in the red of the Night look (also for the tests)
  static QImage tinted(const QImage &image);

signals:
  void tintChanged();
  void sourceChanged();
  void revisionChanged();
  void fillChanged();

private:
  const QImage &image() const;
  int m_source=0;
  int m_revision=0;
  bool m_fill=false;
  bool m_tint=false;
  QImage m_tinted;          // the picture in red, kept until the next revision
  int m_tintedRevision=-1;
  qint64 m_tintedKey=0;
};

#endif // IMAGESURFACE_H
