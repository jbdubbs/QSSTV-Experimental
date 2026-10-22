/***************************************************************************
 *   See templaterender.h                                                  *
 *                                                                         *
 *   The painting code is a behaviour-for-behaviour port of the legacy     *
 *   graphItemBase::paint()/drawItem() so that existing templates render   *
 *   the same (tests/template compares against the old renderer's output). *
 *   Where the legacy code did something odd on purpose-built numbers      *
 *   (lines and images truncate to int, the border rectangle of an image   *
 *   is inset by the pen width) it is kept.                                *
 ***************************************************************************/
#include "templaterender.h"

#include <QFontInfo>
#include <QPainterPath>
#include "macroexpansion.h"

namespace tpl
{

QString expandMacros(const QString &text, const QHash<QChar, QString> &vars)
{
  macroExpansion mexp;
  for (auto it = vars.constBegin(); it != vars.constEnd(); ++it) mexp.addConversion(it.key(), it.value());
  return mexp.convert(text);
}

namespace
{

/*! Qt converts a font's point size to pixels with the screen's logical dpi (96 on Linux/Windows, 72 on macOS, or
    whatever Xft.dpi says), so the same template drew different sized text on different machines. Templates are
    designed at 96 dpi: scale the text outline when the screen is not. */
double textDpiScale()
{
  static const double scale = []() {
    QFont f;
    f.setPointSizeF(72);
    const int px = QFontInfo(f).pixelSize();
    return px > 0 ? 96.0 / px : 1.0;
  }();
  return scale;
}

QPainterPath textPath(const FontSpec &spec, const QString &text)
{
  const QFont font = spec.toFont();
  QPainterPath testPath;
  testPath.addText(0, 0, font, QStringLiteral("AQag"));
  const qreal height = testPath.boundingRect().height() * 1.2;
  QPainterPath path;
  const QStringList lines = text.split(QLatin1Char('\n'));
  for (int i = 0; i < lines.count(); i++) path.addText(0, i * height, font, lines.at(i));
  const double s = textDpiScale();
  if (s != 1.0) path = QTransform::fromScale(s, s).map(path);
  return path;
}

void drawBorder(QPainter &p, const Element &e, const QPen &pen)
{
  QPen lpen(pen);
  const qreal pad = lpen.widthF() / 2;
  p.setBrush(Qt::NoBrush);
  lpen.setJoinStyle(Qt::MiterJoin);
  p.setPen(lpen);
  p.drawRect(e.rect.adjusted(pad, pad, -pad, -pad));
}

// an image is scaled to its rectangle first, then drawn inside the pen width; refuse absurd sizes (a hostile template
// could ask for a 20000 x 20000 scaled copy) and let the painter scale instead
void drawScaled(QPainter &p, const Element &e, const QImage &img, qreal pad)
{
  const QRectF target = e.rect.adjusted(2 * pad, 2 * pad, -2 * pad, -2 * pad);
  const int w = static_cast<int>(e.rect.width());
  const int h = static_cast<int>(e.rect.height());
  if (img.isNull() || w <= 0 || h <= 0) return;
  if (qint64(w) * h > 16LL * 1024 * 1024)
    {
      p.drawImage(target, img);
      return;
    }
  p.drawImage(target, img.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}

}  // namespace

void paintElement(QPainter &p, const Element &e, const RenderOptions &opt)
{
  p.save();
  p.translate(e.pos);
  p.setTransform(e.itemTransform(), true);
  if (e.opacity < 1.0) p.setOpacity(p.opacity() * e.opacity);

  const QPen pen = e.outline.toPen();
  const bool shaped = e.type == ItemType::Rect || e.type == ItemType::Ellipse;
  if (shaped && e.gradient.isActive()) p.setBrush(QBrush(e.gradient.build(e.rect)));
  else p.setBrush(QBrush(e.fill));
  p.setPen(pen);

  switch (e.type)
    {
    case ItemType::Rect:
      {
        QPen lpen(pen);
        lpen.setJoinStyle(Qt::MiterJoin);
        p.setPen(lpen);
        p.drawRect(e.rect);
        break;
      }
    case ItemType::Ellipse:
      p.drawEllipse(e.rect);
      break;
    case ItemType::Line:
      {
        QPen lpen(pen);
        lpen.setCapStyle(Qt::FlatCap);
        p.setPen(lpen);
        const qreal ym = e.rect.y() + e.rect.height() / 2;
        p.drawLine(static_cast<int>(e.rect.x()), static_cast<int>(ym), static_cast<int>(e.rect.x() + e.rect.width()), static_cast<int>(ym));
        break;
      }
    case ItemType::Image:
      drawScaled(p, e, e.image, pen.widthF() / 2);
      drawBorder(p, e, pen);
      break;
    case ItemType::Replay:
      {
        const qreal pad = pen.widthF() / 2;
        if (opt.replay.isNull())
          {
            p.setPen(pen);
            p.setBrush(QBrush(Qt::black, Qt::Dense7Pattern));
            p.drawRect(e.rect.adjusted(2 * pad, 2 * pad, -2 * pad, -2 * pad));
          }
        else
          {
            drawScaled(p, e, opt.replay, pad);
          }
        drawBorder(p, e, pen);
        break;
      }
    case ItemType::Text:
      {
        const QString text = opt.expandMacros ? expandMacros(e.text, opt.vars) : e.text;
        const QPainterPath path = textPath(e.font, text);
        if (e.gradient.isActive()) p.setBrush(QBrush(e.gradient.build(path.boundingRect())));
        else p.setBrush(QBrush(e.fill));
        p.setPen(pen);
        p.drawPath(path);
        break;
      }
    }
  p.restore();
}

void paintDocument(QPainter &p, const TemplateDocument &doc, const QRectF &target, const RenderOptions &opt, bool keepAspect)
{
  const QRectF src = doc.canvas;
  if (src.width() <= 0 || src.height() <= 0 || target.isEmpty()) return;
  qreal xr = target.width() / src.width();
  qreal yr = target.height() / src.height();
  if (keepAspect) xr = yr = qMin(xr, yr);
  p.save();
  p.setClipRect(target, Qt::IntersectClip);
  QTransform t;
  t.translate(target.left(), target.top());
  t.scale(xr, yr);
  t.translate(-src.left(), -src.top());
  p.setWorldTransform(t, true);
  for (const Element &e : doc.items) paintElement(p, e, opt);
  p.restore();
}

QImage overlay(const TemplateDocument &doc, const QImage &base, const RenderOptions &opt)
{
  if (base.isNull()) return QImage();
  QImage out = base.copy();
  switch (out.format())
    {
    case QImage::Format_Mono:
    case QImage::Format_MonoLSB:
    case QImage::Format_Indexed8:
    case QImage::Format_Alpha8:
    case QImage::Format_Grayscale8:
    case QImage::Format_Grayscale16:
      out = out.convertToFormat(QImage::Format_ARGB32_Premultiplied);   // QPainter cannot draw on these
      break;
    default:
      break;
    }
  QPainter p(&out);
  if (!p.isActive()) return base;
  p.setRenderHint(QPainter::Antialiasing);
  paintDocument(p, doc, QRectF(QPointF(0, 0), QSizeF(out.size())), opt, false);
  p.end();
  return out;
}

QImage renderThumbnail(const TemplateDocument &doc)
{
  const int maxSide = 4096;
  int w = qMax(1, static_cast<int>(doc.canvas.width()));
  int h = qMax(1, static_cast<int>(doc.canvas.height()));
  if (w > maxSide || h > maxSide)
    {
      const double f = double(maxSide) / qMax(w, h);
      w = qMax(1, int(w * f));
      h = qMax(1, int(h * f));
    }
  QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
  img.fill(0);
  QPainter p(&img);
  p.setRenderHint(QPainter::Antialiasing);
  RenderOptions opt;
  opt.expandMacros = false;
  paintDocument(p, doc, QRectF(0, 0, w, h), opt, true);
  p.end();
  return img;
}

}  // namespace tpl
