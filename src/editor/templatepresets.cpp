#include "templatepresets.h"

#include <QCoreApplication>
#include <QFontDatabase>

namespace templatePresets
{

namespace
{

tpl::Element textBox(const QString &text, const QRectF &box, double pointSize, bool bold)
{
  tpl::Element e;
  e.type = tpl::ItemType::Text;
  e.text = text;
  e.pos = box.topLeft();
  e.rect = QRectF(0, 0, box.width(), box.height());
  e.textBox = true;
  e.shrinkToFit = true;
  e.valign = tpl::TextVAlign::Middle;
  e.font.family = QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
  e.font.pointSize = pointSize;
  e.font.weight = bold ? 700 : 400;
  e.fill = Qt::white;
  e.outline.color = Qt::black;
  e.outline.width = qMax(1.0, pointSize / 12);
  e.outline.join = Qt::RoundJoin;
  return e;
}

void gradient(tpl::Element &e, int angle, const QVector<tpl::GradientStop> &stops)
{
  e.gradient.kind = tpl::Gradient::Linear;
  e.gradient.angle = angle;
  e.gradient.stops = stops;
  e.fill = stops.isEmpty() ? e.fill : stops.last().color;
}

tpl::Element darkBar(const QSize &c)
{
  tpl::Element bar;
  bar.type = tpl::ItemType::Rect;
  bar.pos = QPointF(0, c.height() * 0.84);
  bar.rect = QRectF(0, 0, c.width(), c.height() * 0.16);
  bar.outline.style = Qt::NoPen;
  gradient(bar, 270, {{0, QColor(8, 16, 40, 0)}, {0.35, QColor(8, 16, 40, 150)}, {1, QColor(8, 16, 40, 235)}});
  bar.anchorH = tpl::AnchorH::Span;
  bar.anchorV = tpl::AnchorV::Bottom;
  return bar;
}

tpl::Element callAndReport(const QSize &c)
{
  const double k = c.height() / 496.0;
  tpl::Element call = textBox(QStringLiteral("%c de %m"), QRectF(c.width() * 0.03, c.height() * 0.855, c.width() * 0.6, c.height() * 0.13), 26 * k, true);
  call.align = tpl::TextAlign::Left;
  call.shadow.on = true;
  call.shadow.dx = 2 * k;
  call.shadow.dy = 2 * k;
  call.shadow.blur = 3 * k;
  call.shadow.color = QColor(0, 0, 0, 190);
  call.anchorH = tpl::AnchorH::Left;
  call.anchorV = tpl::AnchorV::Bottom;
  return call;
}

tpl::Element reportBox(const QSize &c)
{
  const double k = c.height() / 496.0;
  tpl::Element rsv = textBox(QStringLiteral("RSV %r\n%d %t UTC"), QRectF(c.width() * 0.64, c.height() * 0.855, c.width() * 0.33, c.height() * 0.13), 13 * k, false);
  rsv.align = tpl::TextAlign::Right;
  rsv.wrap = true;
  rsv.outline.width = qMax(0.5, k);
  rsv.anchorH = tpl::AnchorH::Right;
  rsv.anchorV = tpl::AnchorV::Bottom;
  return rsv;
}

tpl::Element replayWindow(const QSize &c)
{
  const double k = c.height() / 496.0;
  tpl::Element r;
  r.type = tpl::ItemType::Replay;
  const double w = c.width() * 0.34, h = w * 0.75;
  r.pos = QPointF(c.width() * 0.63, c.height() * 0.04);
  r.rect = QRectF(0, 0, w, h);
  r.outline.color = Qt::white;
  r.outline.width = 3 * k;
  r.shadow.on = true;
  r.shadow.dx = 3 * k;
  r.shadow.dy = 4 * k;
  r.shadow.blur = 6 * k;
  r.shadow.color = QColor(0, 0, 0, 150);
  r.anchorH = tpl::AnchorH::Right;
  r.anchorV = tpl::AnchorV::Top;
  return r;
}

}  // namespace

QStringList starterNames()
{
  return {QCoreApplication::translate("templatePresets", "Blank"),
          QCoreApplication::translate("templatePresets", "Call sign bar"),
          QCoreApplication::translate("templatePresets", "Call sign in the corner"),
          QCoreApplication::translate("templatePresets", "Replay window and call sign bar")};
}

tpl::TemplateDocument makeStarter(int index, const QSize &canvas)
{
  tpl::TemplateDocument d;
  d.canvas = QRectF(QPointF(0, 0), QSizeF(canvas));
  d.layout = tpl::TemplateDocument::Anchored;
  const double k = canvas.height() / 496.0;
  switch (index)
    {
    case 1:
      d.items << darkBar(canvas) << callAndReport(canvas) << reportBox(canvas);
      break;
    case 2:
      {
        tpl::Element call = textBox(QStringLiteral("%c de %m"), QRectF(canvas.width() * 0.03, canvas.height() * 0.03, canvas.width() * 0.5, canvas.height() * 0.1), 24 * k, true);
        call.align = tpl::TextAlign::Left;
        call.shadow.on = true;
        call.shadow.dx = 2 * k;
        call.shadow.dy = 2 * k;
        call.shadow.blur = 3 * k;
        call.shadow.color = QColor(0, 0, 0, 190);
        tpl::Element rsv = textBox(QStringLiteral("RSV %r"), QRectF(canvas.width() * 0.03, canvas.height() * 0.13, canvas.width() * 0.3, canvas.height() * 0.07), 14 * k, false);
        rsv.align = tpl::TextAlign::Left;
        rsv.outline.width = qMax(0.5, k);
        d.items << call << rsv;
        break;
      }
    case 3:
      d.items << darkBar(canvas) << callAndReport(canvas) << reportBox(canvas) << replayWindow(canvas);
      break;
    default:
      break;
    }
  for (int i = 0; i < d.items.size(); i++) d.items[i].id = QStringLiteral("item-%1").arg(i + 1);
  tpl::sanitize(d);
  return d;
}

QStringList barNames()
{
  return {QCoreApplication::translate("templatePresets", "Dark fade"),
          QCoreApplication::translate("templatePresets", "Blue"),
          QCoreApplication::translate("templatePresets", "Red"),
          QCoreApplication::translate("templatePresets", "Green"),
          QCoreApplication::translate("templatePresets", "Gold"),
          QCoreApplication::translate("templatePresets", "Rainbow")};
}

tpl::Element makeBar(int style, bool atBottom, const QRectF &canvas)
{
  const double h = canvas.height() * 0.14;
  tpl::Element bar;
  bar.type = tpl::ItemType::Rect;
  bar.pos = QPointF(canvas.left(), atBottom ? canvas.bottom() - h : canvas.top());
  bar.rect = QRectF(0, 0, canvas.width(), h);
  bar.outline.style = Qt::NoPen;
  bar.anchorH = tpl::AnchorH::Span;
  bar.anchorV = atBottom ? tpl::AnchorV::Bottom : tpl::AnchorV::Top;
  // the fade runs towards the edge of the picture: darkest at the edge for the bottom bar, at the top edge for the top bar
  auto vertical = [&](QColor edge, QColor inner) {
    gradient(bar, 270, atBottom ? QVector<tpl::GradientStop>{{0, inner}, {1, edge}} : QVector<tpl::GradientStop>{{0, edge}, {1, inner}});
  };
  switch (qBound(0, style, 5))
    {
    case 0:
      vertical(QColor(8, 16, 40, 235), QColor(8, 16, 40, 0));
      break;
    case 1:
      vertical(QColor(10, 40, 130, 240), QColor(90, 160, 235, 200));
      break;
    case 2:
      vertical(QColor(120, 10, 10, 240), QColor(240, 120, 40, 200));
      break;
    case 3:
      vertical(QColor(10, 80, 30, 240), QColor(130, 215, 80, 200));
      break;
    case 4:
      vertical(QColor(120, 70, 0, 240), QColor(255, 220, 80, 210));
      break;
    default:
      gradient(bar, 0, {{0, QColor(230, 40, 40, 230)}, {0.2, QColor(240, 200, 30, 230)}, {0.4, QColor(60, 190, 60, 230)}, {0.6, QColor(40, 190, 210, 230)}, {0.8, QColor(50, 80, 220, 230)}, {1, QColor(190, 50, 200, 230)}});
      break;
    }
  tpl::sanitize(bar);
  return bar;
}

}  // namespace templatePresets
