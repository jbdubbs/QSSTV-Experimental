#include "templatecanvas.h"
#include "touchtemplateeditor.h"
#include "templaterender.h"
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

TemplateCanvas::TemplateCanvas(QQuickItem *parent):QQuickPaintedItem(parent)
{
  setAntialiasing(true);
}

void TemplateCanvas::setRevision(int r)
{
  if(r==m_revision) return;
  m_revision=r;
  emit revisionChanged();
  update();
}

// a picture to lay the template on: sky, a sun and two hills, like the test pictures of the mockups
static QImage samplePicture(const QSize &size)
{
  QImage im(size,QImage::Format_ARGB32_Premultiplied);
  QPainter p(&im);
  p.setRenderHint(QPainter::Antialiasing);
  QLinearGradient sky(0,0,0,size.height());
  sky.setColorAt(0,QColor("#6ec1ff"));
  sky.setColorAt(1,QColor("#d9f0ff"));
  p.fillRect(im.rect(),sky);
  p.setPen(Qt::NoPen);
  p.setBrush(QColor("#fff3c4"));
  const double w=size.width(),h=size.height();
  p.drawEllipse(QPointF(w*0.3,h*0.28),h*0.1,h*0.1);
  QPainterPath hill;
  hill.moveTo(0,h*0.72);
  hill.cubicTo(w*0.25,h*0.52,w*0.6,h*0.62,w,h*0.5);
  hill.lineTo(w,h);
  hill.lineTo(0,h);
  p.setBrush(QColor("#2f7d3a"));
  p.drawPath(hill);
  QPainterPath hill2;
  hill2.moveTo(0,h*0.86);
  hill2.cubicTo(w*0.3,h*0.7,w*0.7,h*0.82,w,h*0.74);
  hill2.lineTo(w,h);
  hill2.lineTo(0,h);
  p.setBrush(QColor("#1d5a2a"));
  p.drawPath(hill2);
  return im;
}

void TemplateCanvas::paint(QPainter *painter)
{
  TouchTemplateEditor *ed=TouchTemplateEditor::instance();
  const QSize px(qMax(1,int(width())),qMax(1,int(height())));
  if(m_sample.size()!=px) m_sample=samplePicture(px);
  painter->setRenderHint(QPainter::Antialiasing);
  painter->setRenderHint(QPainter::TextAntialiasing);
  painter->drawImage(0,0,m_sample);
  tpl::paintDocument(*painter,ed->document(),QRectF(0,0,width(),height()),ed->renderOptions());
}
