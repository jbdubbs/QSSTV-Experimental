#ifndef TEMPLATECANVAS_H
#define TEMPLATECANVAS_H

#include <QImage>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

/*!
  The template being edited, drawn over a picture (the one it is for, or a sample one): what the editor shows. The size of the item is the
  size of the picture; the template's canvas is stretched over it, as the transmit path does. The selection and its handles are Qt Quick
  items over this one.
*/
class TemplateCanvas : public QQuickPaintedItem
{
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(int revision READ revision WRITE setRevision NOTIFY revisionChanged)   //!< bind it to TouchTemplateEditor.revision: a change repaints
public:
  explicit TemplateCanvas(QQuickItem *parent=nullptr);
  void paint(QPainter *painter) override;
  int revision() const {return m_revision;}
  void setRevision(int r);
signals:
  void revisionChanged();
private:
  int m_revision=0;
  QImage m_sample;
};

#endif // TEMPLATECANVAS_H
