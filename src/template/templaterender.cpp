/***************************************************************************
 *   See templaterender.h                                                  *
 *                                                                         *
 *   The painting code is a behavior-for-behavior port of the legacy     *
 *   graphItemBase::paint()/drawItem() so that existing templates render   *
 *   the same (tests/template compares against the old renderer's output). *
 *   Where the legacy code did something odd on purpose-built numbers      *
 *   (lines and images truncate to int, the border rectangle of an image   *
 *   is inset by the pen width) it is kept.                                *
 ***************************************************************************/
#include "templaterender.h"

#include <QFontInfo>
#include <QFontMetricsF>
#include <QMutex>
#include <QMutexLocker>
#include <QPainterPath>
#include <cmath>
#include <QtMath>
#include <vector>
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

// --- text in a box --------------------------------------------------------------------------------------------------
// Laid out in "font space" (the font's own pixels, before the 96 dpi normalization of textDpiScale()), then scaled.

struct BlockLayout
{
  QStringList lines;
  QVector<double> widths;     //!< advance width of every line
  double pitch = 0;           //!< distance between baselines (the legacy "AQag" height x 1.2)
  double ascent = 0;
  double descent = 0;
  double width = 0;           //!< widest line
  double height = 0;          //!< ascent + descent + the extra lines
};

FontSpec scaledSpec(const FontSpec &spec, double factor)
{
  FontSpec f = spec;
  f.pointSize = qMax(0.5, spec.pointSize * factor);
  f.letterSpacing = spec.letterSpacing * factor;
  return f;
}

/*! the words of a paragraph on lines no wider than `maxWidth`; a word wider than that is broken between characters */
QStringList wrapParagraph(const QString &paragraph, const QFontMetricsF &fm, double maxWidth)
{
  QStringList lines;
  QString line;
  auto flush = [&]() {
    lines << line;
    line.clear();
  };
  const QStringList words = paragraph.split(QLatin1Char(' '), Qt::SkipEmptyParts);
  for (const QString &word : words)
    {
      const QString trial = line.isEmpty() ? word : line + QLatin1Char(' ') + word;
      if (fm.horizontalAdvance(trial) <= maxWidth || (line.isEmpty() && word.size() <= 1))
        {
          line = trial;
          continue;
        }
      if (!line.isEmpty()) flush();
      if (fm.horizontalAdvance(word) <= maxWidth)
        {
          line = word;
          continue;
        }
      for (int i = 0; i < word.size();)   // longer than the box: break it where it no longer fits, never inside a letter
        {
          int n = 1;
          if (word.at(i).isHighSurrogate() && i + 1 < word.size() && word.at(i + 1).isLowSurrogate()) n = 2;
          while (i + n < word.size() && (word.at(i + n).category() == QChar::Mark_NonSpacing || word.at(i + n).category() == QChar::Mark_SpacingCombining)) n++;   // accents stay with their letter
          const QString piece = word.mid(i, n);
          if (!line.isEmpty() && fm.horizontalAdvance(line + piece) > maxWidth) flush();
          line += piece;
          i += n;
        }
    }
  if (!line.isEmpty() || lines.isEmpty()) flush();
  return lines;
}

BlockLayout layoutBlock(const FontSpec &spec, double factor, const QString &text, double boxWidth, bool wrap)
{
  const FontSpec fs = scaledSpec(spec, factor);
  const QFont font = fs.toFont();
  const QFontMetricsF fm(font);
  BlockLayout b;
  const QStringList paragraphs = text.split(QLatin1Char('\n'));
  for (const QString &para : paragraphs)
    {
      if (wrap) b.lines << wrapParagraph(para, fm, boxWidth);
      else b.lines << para;
    }
  QPainterPath testPath;
  testPath.addText(0, 0, font, QStringLiteral("AQag"));
  b.pitch = testPath.boundingRect().height() * 1.2;
  b.ascent = fm.ascent();
  b.descent = fm.descent();
  for (const QString &l : b.lines)
    {
      const double w = fm.horizontalAdvance(l);
      b.widths << w;
      b.width = qMax(b.width, w);
    }
  b.height = b.ascent + b.descent + qMax(0, int(b.lines.size()) - 1) * b.pitch;
  return b;
}

/*! The size factor (0.1 .. 1) at which the text fits the box. Finding it takes a dozen layouts and the editor asks on every
    repaint, so the answer is remembered for the same text, font and box. */
double shrinkFactor(const FontSpec &font, const QString &text, double boxW, double boxH, bool wrap)
{
  static QMutex mutex;
  static QHash<QString, double> cache;
  const QString key = QStringList({font.family, QString::number(font.pointSize, 'g', 10), QString::number(font.weight), QString::number(font.stretch),
                                   QString::number(font.letterSpacing, 'g', 10), QString::number(int(font.italic) | int(font.underline) << 1 | int(font.strikeOut) << 2),
                                   QString::number(boxW, 'g', 10), QString::number(boxH, 'g', 10), QString::number(int(wrap)), text}).join(QLatin1Char('\x1f'));
  {
    QMutexLocker lock(&mutex);
    const auto it = cache.constFind(key);
    if (it != cache.constEnd()) return it.value();
  }
  auto fits = [&](double f) {
    const BlockLayout l = layoutBlock(font, f, text, boxW, wrap);
    return l.width <= boxW + 0.01 && l.height <= boxH + 0.01;
  };
  double factor = 1.0;
  if (!fits(1.0))
    {
      double lo = 0.1, hi = 1.0;
      if (!fits(lo)) factor = lo;   // does not fit even at a tenth: draw it that small
      else
        {
          for (int i = 0; i < 14; i++)
            {
              const double mid = (lo + hi) / 2;
              if (fits(mid)) lo = mid;
              else hi = mid;
            }
          factor = lo;
        }
    }
  QMutexLocker lock(&mutex);
  if (cache.size() > 2000) cache.clear();
  cache.insert(key, factor);
  return factor;
}

/*! The text of a box item: wrapped, shrunk to fit and aligned inside e.rect, as a path in item coordinates. */
QPainterPath boxedTextPath(const Element &e, const QString &text)
{
  const double dpi = textDpiScale();
  const double boxW = qMax(1.0, e.rect.width()) / dpi;
  const double boxH = qMax(1.0, e.rect.height()) / dpi;
  const double factor = e.shrinkToFit ? shrinkFactor(e.font, text, boxW, boxH, e.wrap) : 1.0;
  const BlockLayout b = layoutBlock(e.font, factor, text, boxW, e.wrap);
  const QFont font = scaledSpec(e.font, factor).toFont();
  double y = b.ascent;
  if (e.valign == TextVAlign::Middle) y += (boxH - b.height) / 2;
  else if (e.valign == TextVAlign::Bottom) y += boxH - b.height;
  QPainterPath path;
  for (int i = 0; i < b.lines.size(); i++)
    {
      double x = 0;
      if (e.align == TextAlign::Center) x = (boxW - b.widths[i]) / 2;
      else if (e.align == TextAlign::Right) x = boxW - b.widths[i];
      path.addText(x, y + i * b.pitch, font, b.lines.at(i));
    }
  QTransform t;
  t.translate(e.rect.x(), e.rect.y());
  t.scale(dpi, dpi);
  return t.map(path);
}

/*! the outline of an item's text, wherever it is drawn */
QPainterPath textOf(const Element &e, const QString &text)
{
  return e.textBox ? boxedTextPath(e, text) : textPath(e.font, text);
}

void drawBorder(QPainter &p, const Element &e, const QPen &pen)
{
  const qreal pad = pen.widthF() / 2;
  p.setBrush(Qt::NoBrush);
  p.setPen(pen);
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

namespace
{

/*! Draws the item in the coordinate system the caller has set up (origin, item transform, opacity). With `flat` the item
    is drawn as a one-color silhouette instead (a shadow): no gradient, no borders, pictures as their rectangle. */
void paintBody(QPainter &p, const Element &e, const RenderOptions &opt, const QColor *flat)
{
  QPen pen = e.outline.toPen();
  if (flat) pen.setColor(*flat);
  const qreal inset = pen.style() == Qt::NoPen ? 0 : pen.widthF() / 2;   // a picture sits inside its border, and fills its rectangle without one
  const bool shaped = e.type == ItemType::Rect || e.type == ItemType::Ellipse;
  if (flat) p.setBrush(QBrush(*flat));
  else if (shaped && e.gradient.isActive()) p.setBrush(QBrush(e.gradient.build(e.rect)));
  else p.setBrush(QBrush(e.fill));
  p.setPen(pen);

  switch (e.type)
    {
    case ItemType::Rect:
      if (e.radius > 0)
        {
          const qreal r = qMin<qreal>(e.radius, qMin(e.rect.width(), e.rect.height()) / 2);
          p.drawRoundedRect(e.rect, r, r);
        }
      else p.drawRect(e.rect);
      break;
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
      if (flat)
        {
          p.setPen(Qt::NoPen);
          p.drawRect(e.rect);
          break;
        }
      drawScaled(p, e, e.image, inset);
      drawBorder(p, e, pen);
      break;
    case ItemType::Replay:
      {
        if (flat)
          {
            p.setPen(Qt::NoPen);
            p.drawRect(e.rect);
            break;
          }
        if (opt.replay.isNull())
          {
            p.setPen(pen);
            p.setBrush(QBrush(Qt::black, Qt::Dense7Pattern));
            p.drawRect(e.rect.adjusted(2 * inset, 2 * inset, -2 * inset, -2 * inset));
          }
        else
          {
            drawScaled(p, e, opt.replay, inset);
          }
        drawBorder(p, e, pen);
        break;
      }
    case ItemType::Text:
      {
        const QString text = opt.expandMacros ? expandMacros(e.text, opt.vars) : e.text;
        const QPainterPath path = textOf(e, text);
        if (!flat && !e.image.isNull()) p.setBrush(QBrush(e.image));
        else if (!flat && e.gradient.isActive()) p.setBrush(QBrush(e.gradient.build(e.textBox ? e.rect : path.boundingRect())));
        p.setPen(pen);
        p.drawPath(path);
        break;
      }
    }
}

/*! One pass of a box blur of radius r over the rows (horizontal) or the columns of a premultiplied ARGB32 image. */
void boxBlurPass(QImage &img, int r, bool horizontal)
{
  const int w = img.width(), h = img.height();
  const int lines = horizontal ? h : w;
  const int len = horizontal ? w : h;
  const int window = 2 * r + 1;
  std::vector<quint32> in(len);
  for (int l = 0; l < lines; l++)
    {
      for (int i = 0; i < len; i++) in[i] = horizontal ? reinterpret_cast<const quint32 *>(img.constScanLine(l))[i] : reinterpret_cast<const quint32 *>(img.constScanLine(i))[l];
      quint32 sum[4] = {0, 0, 0, 0};
      auto add = [&](int i, int sign) {
        if (i < 0 || i >= len) return;
        const quint32 v = in[i];
        sum[0] += sign * (v & 0xff);
        sum[1] += sign * ((v >> 8) & 0xff);
        sum[2] += sign * ((v >> 16) & 0xff);
        sum[3] += sign * (v >> 24);
      };
      for (int i = -r; i <= r; i++) add(i, 1);
      for (int i = 0; i < len; i++)
        {
          const quint32 out = (sum[0] / window) | ((sum[1] / window) << 8) | ((sum[2] / window) << 16) | ((sum[3] / window) << 24);
          if (horizontal) reinterpret_cast<quint32 *>(img.scanLine(l))[i] = out;
          else reinterpret_cast<quint32 *>(img.scanLine(i))[l] = out;
          add(i - r, -1);
          add(i + r + 1, 1);
        }
    }
}

/*! A close relative of a Gaussian blur of the given sigma: three box blurs, rows then columns. */
void blurImage(QImage &img, double sigma)
{
  int box = static_cast<int>(std::floor(std::sqrt(4 * sigma * sigma + 1)));
  if (box % 2 == 0) box++;
  const int r = qMax(1, box / 2);
  for (int pass = 0; pass < 3; pass++)
    {
      boxBlurPass(img, r, true);
      boxBlurPass(img, r, false);
    }
}

void paintShadow(QPainter &p, const Element &e, const RenderOptions &opt)
{
  const Shadow &sh = e.shadow;
  if (sh.color.alpha() == 0) return;
  const QColor flat = sh.color;
  const QTransform world = p.worldTransform();
  const QTransform full = e.itemTransform() * QTransform::fromTranslate(e.pos.x() + sh.dx, e.pos.y() + sh.dy) * world;
  auto hard = [&]() {
    p.save();
    p.translate(sh.dx, sh.dy);
    p.translate(e.pos);
    p.setTransform(e.itemTransform(), true);
    if (e.opacity < 1.0) p.setOpacity(p.opacity() * e.opacity);
    paintBody(p, e, opt, &flat);
    p.restore();
  };
  const double scale = std::sqrt(std::abs(world.m11() * world.m22() - world.m12() * world.m21()));
  const double sigma = sh.blur * scale / 2;
  if (sigma < 0.3)
    {
      hard();
      return;
    }
  // the blurred silhouette is drawn into a layer of device pixels around the item, blurred, and put back
  const double pad = e.outline.width + 2;
  const QRectF itemBounds = localBounds(e, opt).adjusted(-pad, -pad, pad, pad);
  int box = static_cast<int>(std::floor(std::sqrt(4 * sigma * sigma + 1)));
  if (box % 2 == 0) box++;
  const double margin = 3 * qMax(1, box / 2) + 2;
  QRectF dev = full.mapRect(itemBounds).adjusted(-margin, -margin, margin, margin);
  const QRectF clip = world.mapRect(p.clipBoundingRect());
  if (clip.isValid() && std::isfinite(clip.width()) && std::isfinite(clip.height())) dev = dev.intersected(clip.adjusted(-margin, -margin, margin, margin));
  const QRect ir = dev.toAlignedRect();
  if (ir.isEmpty()) return;
  if (qint64(ir.width()) * ir.height() > 12LL * 1024 * 1024)
    {
      hard();
      return;
    }
  QImage layer(ir.size(), QImage::Format_ARGB32_Premultiplied);
  layer.fill(0);
  {
    QPainter lp(&layer);
    lp.setRenderHints(p.renderHints());
    lp.translate(-ir.topLeft());
    lp.setWorldTransform(full, true);
    paintBody(lp, e, opt, &flat);
  }
  blurImage(layer, sigma);
  p.save();
  p.setWorldTransform(QTransform());
  if (e.opacity < 1.0) p.setOpacity(p.opacity() * e.opacity);
  p.drawImage(ir.topLeft(), layer);
  p.restore();
}

}  // namespace

void paintElement(QPainter &p, const Element &e, const RenderOptions &opt)
{
  p.save();
  if (e.shadow.on) paintShadow(p, e, opt);
  p.translate(e.pos);
  p.setTransform(e.itemTransform(), true);
  if (e.opacity < 1.0) p.setOpacity(p.opacity() * e.opacity);
  paintBody(p, e, opt, nullptr);
  p.restore();
}

QRectF textBounds(const Element &e, const RenderOptions &opt)
{
  if (e.textBox) return QRectF(e.rect.x(), e.rect.y(), qMax(1.0, e.rect.width()), qMax(1.0, e.rect.height()));
  const QString text = opt.expandMacros ? expandMacros(e.text, opt.vars) : e.text;
  const QRectF r = textPath(e.font, text).boundingRect();
  if (r.width() >= 1 && r.height() >= 1) return r;
  const double h = qMax(4.0, e.font.pointSize * 96.0 / 72.0);   // px at the 96 dpi reference
  return QRectF(0, -h, qMax(8.0, r.width()), h);
}

QStringList textLines(const Element &e, const RenderOptions &opt)
{
  const QString text = opt.expandMacros ? expandMacros(e.text, opt.vars) : e.text;
  if (!e.textBox) return text.split(QLatin1Char('\n'));
  const double dpi = textDpiScale();
  const double boxW = qMax(1.0, e.rect.width()) / dpi;
  const double boxH = qMax(1.0, e.rect.height()) / dpi;
  const double factor = e.shrinkToFit ? shrinkFactor(e.font, text, boxW, boxH, e.wrap) : 1.0;
  return layoutBlock(e.font, factor, text, boxW, e.wrap).lines;
}

QRectF naturalTextBox(const Element &e, const RenderOptions &opt)
{
  const QString text = opt.expandMacros ? expandMacros(e.text, opt.vars) : e.text;
  const BlockLayout b = layoutBlock(e.font, 1.0, text, 1e9, false);
  const double dpi = textDpiScale();
  return QRectF(0, -b.ascent * dpi, qMax(1.0, b.width * dpi), qMax(1.0, b.height * dpi));
}

QRectF localBounds(const Element &e, const RenderOptions &opt)
{
  switch (e.type)
    {
    case ItemType::Text:
      return textBounds(e, opt);
    case ItemType::Line:
      {
        const qreal h = qMax<qreal>(e.outline.width, 2);
        return QRectF(e.rect.x(), e.rect.y() + e.rect.height() / 2 - h / 2, e.rect.width(), h);
      }
    default:
      return e.rect;
    }
}

QTransform sceneTransform(const Element &e)
{
  return e.itemTransform() * QTransform::fromTranslate(e.pos.x(), e.pos.y());
}

QPolygonF sceneOutline(const Element &e, const RenderOptions &opt)
{
  return sceneTransform(e).map(QPolygonF(localBounds(e, opt)));
}

double anchoredScale(const QRectF &canvas, const QSizeF &target)
{
  if (canvas.width() <= 0 || canvas.height() <= 0) return 1.0;
  return qMin(target.width() / canvas.width(), target.height() / canvas.height());
}

QSizeF anchoredCanvasSize(const QRectF &canvas, const QSizeF &target)
{
  const double s = anchoredScale(canvas, target);
  if (s <= 0) return canvas.size();
  QSizeF eff(target.width() / s, target.height() / s);
  if (std::abs(eff.width() - canvas.width()) < 1e-9) eff.setWidth(canvas.width());   // the tighter axis: no rounding noise
  if (std::abs(eff.height() - canvas.height()) < 1e-9) eff.setHeight(canvas.height());
  return eff;
}

Element anchoredElement(const Element &e, const QRectF &canvas, const QSizeF &effective)
{
  Element a = e;
  if (canvas.width() <= 0 || canvas.height() <= 0) return a;
  const double dx = effective.width() - canvas.width();
  const double dy = effective.height() - canvas.height();
  const double fx = effective.width() / canvas.width();
  const double fy = effective.height() / canvas.height();
  const bool turned = e.rotation != 0 || e.skewH != 0 || e.skewV != 0;   // a turned item is only moved: growing it would shear it
  const bool freeText = e.type == ItemType::Text && !e.textBox;          // anchored by its origin
  const bool point = turned || freeText;
  // x
  switch (e.anchorH)
    {
    case AnchorH::Left:
      break;
    case AnchorH::Center:
      a.pos.rx() += dx / 2;
      break;
    case AnchorH::Right:
      a.pos.rx() += dx;
      break;
    case AnchorH::Span:
      if (!point) a.rect.setWidth(e.rect.width() + dx);
      break;
    case AnchorH::Scale:
      if (point)
        {
          const double ref = freeText ? e.pos.x() : e.pos.x() + e.rect.center().x();
          a.pos.rx() += (canvas.x() + (ref - canvas.x()) * fx) - ref;
        }
      else
        {
          const double left = e.pos.x() + e.rect.x();
          a.pos.rx() += (canvas.x() + (left - canvas.x()) * fx) - left;
          a.rect.setWidth(e.rect.width() * fx);
        }
      break;
    }
  // y
  switch (e.anchorV)
    {
    case AnchorV::Top:
      break;
    case AnchorV::Middle:
      a.pos.ry() += dy / 2;
      break;
    case AnchorV::Bottom:
      a.pos.ry() += dy;
      break;
    case AnchorV::Span:
      if (!point) a.rect.setHeight(e.rect.height() + dy);
      break;
    case AnchorV::Scale:
      if (point)
        {
          const double ref = freeText ? e.pos.y() : e.pos.y() + e.rect.center().y();
          a.pos.ry() += (canvas.y() + (ref - canvas.y()) * fy) - ref;
        }
      else
        {
          const double top = e.pos.y() + e.rect.y();
          a.pos.ry() += (canvas.y() + (top - canvas.y()) * fy) - top;
          a.rect.setHeight(e.rect.height() * fy);
        }
      break;
    }
  return a;
}

void paintDocument(QPainter &p, const TemplateDocument &doc, const QRectF &target, const RenderOptions &opt, bool keepAspect)
{
  const QRectF src = doc.canvas;
  if (src.width() <= 0 || src.height() <= 0 || target.isEmpty()) return;
  if (doc.sampledTransparency)
    {
      // Compose before removing the key: per-object removal would reveal lower objects.
      QSize size(qMax(1, qRound(target.width())), qMax(1, qRound(target.height())));
      if (size.width() > 4096 || size.height() > 4096) size.scale(4096, 4096, Qt::KeepAspectRatio);
      QImage layer(size, QImage::Format_ARGB32);
      if (layer.isNull()) return;
      layer.fill(doc.background);
      TemplateDocument plain = doc;
      plain.sampledTransparency = false;
      { QPainter lp(&layer); lp.setRenderHints(p.renderHints());
        paintDocument(lp, plain, QRectF(QPointF(), QSizeF(size)), opt, keepAspect); }
      double sx = size.width() / src.width(), sy = size.height() / src.height();
      if (keepAspect) sx = sy = qMin(sx, sy);
      if (doc.layout == TemplateDocument::Anchored && !keepAspect)
        sx = sy = anchoredScale(src, size);
      const int x = qBound(0, qFloor((doc.transparencyPoint.x() - src.x()) * sx), size.width() - 1);
      const int y = qBound(0, qFloor((doc.transparencyPoint.y() - src.y()) * sy), size.height() - 1);
      const QRgb key = layer.pixel(x, y) & 0xffffff;
      for (int row = 0; row < layer.height(); ++row)
        {
          QRgb *line = reinterpret_cast<QRgb *>(layer.scanLine(row));
          for (int col = 0; col < layer.width(); ++col)
            if ((line[col] & 0xffffff) == key) line[col] = 0;
        }
      p.save(); p.setClipRect(target, Qt::IntersectClip); p.drawImage(target, layer); p.restore();
      return;
    }
  const bool anchored = doc.layout == TemplateDocument::Anchored && !keepAspect;
  qreal xr = target.width() / src.width();
  qreal yr = target.height() / src.height();
  if (keepAspect) xr = yr = qMin(xr, yr);
  if (anchored) xr = yr = anchoredScale(src, target.size());
  p.save();
  p.setClipRect(target, Qt::IntersectClip);
  QTransform t;
  t.translate(target.left(), target.top());
  t.scale(xr, yr);
  t.translate(-src.left(), -src.top());
  p.setWorldTransform(t, true);
  if (anchored)
    {
      const QSizeF eff = anchoredCanvasSize(src, target.size());
      for (const Element &e : doc.items) paintElement(p, anchoredElement(e, src, eff), opt);
    }
  else
    {
      for (const Element &e : doc.items) paintElement(p, e, opt);
    }
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
