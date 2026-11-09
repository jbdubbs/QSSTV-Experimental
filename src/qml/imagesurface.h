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
  Q_PROPERTY(bool stretch READ stretch WRITE setStretch NOTIFY stretchChanged)   //!< fill the whole item, the aspect ratio not kept (the waterfall: all of its frequencies, whatever the height)
  Q_PROPERTY(bool empty READ empty NOTIFY revisionChanged)                       //!< there is nothing to paint
  Q_PROPERTY(qreal paintedWidth READ paintedWidth NOTIFY paintedChanged)         //!< the size the picture is drawn at inside the item (as Image has), 0 without a picture
  Q_PROPERTY(qreal paintedHeight READ paintedHeight NOTIFY paintedChanged)
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
  bool stretch() const {return m_stretch;}
  void setStretch(bool s);
  bool empty() const;
  qreal paintedWidth() const {return painted().width();}
  qreal paintedHeight() const {return painted().height();}
  bool tint() const {return m_tint;}
  void setTint(bool t);
  //! the picture with only its light left, in the red of the Night look (also for the tests)
  static QImage tinted(const QImage &image);

signals:
  void paintedChanged();
  void tintChanged();
  void sourceChanged();
  void revisionChanged();
  void fillChanged();
  void stretchChanged();

protected:
  void geometryChange(const QRectF &newGeometry,const QRectF &oldGeometry) override;

private:
  const QImage &image() const;
  QSizeF painted() const;
  int m_source=0;
  int m_revision=0;
  bool m_fill=false;
  bool m_stretch=false;
  bool m_tint=false;
  QImage m_tinted;          // the picture in red, kept until the next revision
  int m_tintedRevision=-1;
  qint64 m_tintedKey=0;
};

#endif // IMAGESURFACE_H
