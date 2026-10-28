/***************************************************************************
 *   See templatedoc.h                                                     *
 ***************************************************************************/
#include "templatedoc.h"

#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QSaveFile>
#include <QtMath>
#include <cmath>

namespace tpl
{

const char *const nativeExtension = "qtempl";

static const char formatName[] = "qsstv-template";
static const int currentVersion = 2;

// ---------------------------------------------------------------------------------------------------------------
// value types
// ---------------------------------------------------------------------------------------------------------------

/*! Port of the legacy buildGradient() (editor/gradientdialog.cpp); the geometry is kept verbatim so that imported
    templates render the same. `angle` is the legacy "direction": degrees, 0 = east. */
QGradient Gradient::build(const QRectF &f) const
{
  const qreal w = f.width();
  const qreal h = f.height();
  const qreal d = static_cast<qreal>(angle);
  qreal x1, y1, x2, y2, temp;
  QGradient result;
  if (kind == Linear || kind == None)
    {
      if (std::fabs(w / 2 * std::tan(M_PI / 2 - d * M_PI / 180)) <= (w / 2))
        {
          x1 = f.x() + w / 2 - (w / 2 * std::tan(M_PI / 2 - d * M_PI / 180.));
          y1 = f.y() + h;
          x2 = f.x() + w / 2 + (w / 2 * std::tan(M_PI / 2 - d * M_PI / 180.));
          y2 = f.y();
          if ((angle > 180) && (angle <= 359))
            {
              temp = x1; x1 = x2; x2 = temp;
              temp = y1; y1 = y2; y2 = temp;
            }
        }
      else
        {
          x1 = f.x();
          y1 = f.y() + h / 2 + (h / 2 * std::tan(d * M_PI / 180.));
          x2 = f.x() + w;
          y2 = f.y() + h / 2 - (h / 2 * std::tan(d * M_PI / 180.));
          if ((angle > 90) && (angle <= 270))
            {
              temp = x1; x1 = x2; x2 = temp;
              temp = y1; y1 = y2; y2 = temp;
            }
        }
      result = QLinearGradient(x1, y1, x2, y2);
    }
  else if (kind == Radial)
    {
      result = QRadialGradient(QPointF(f.x() + f.width() / 2, f.y() + f.height() / 2), f.width() / 2);
    }
  else
    {
      result = QConicalGradient(QPointF(f.x() + f.width() / 2, f.y() + f.height() / 2), angle);
    }
  for (const GradientStop &s : stops)
    {
      if (s.pos >= 0 && s.pos <= 1) result.setColorAt(s.pos, s.color);
    }
  return result;
}

QPen Outline::toPen() const
{
  // a pen of width 0 is cosmetic in Qt: it always draws one device pixel, so "no width" must mean "no pen"
  if (width <= 0) return QPen(Qt::NoPen);
  QPen p(color);
  p.setWidthF(width);
  p.setJoinStyle(join);
  p.setCapStyle(cap);
  p.setStyle(style);
  p.setMiterLimit(miterLimit);
  return p;
}

QFont FontSpec::toFont() const
{
  QFont f(family);
  f.setPointSizeF(pointSize);
  f.setWeight(static_cast<QFont::Weight>(weight));
  f.setItalic(italic);
  f.setUnderline(underline);
  f.setStrikeOut(strikeOut);
  if (stretch > 0) f.setStretch(stretch);
  if (letterSpacing != 0) f.setLetterSpacing(QFont::AbsoluteSpacing, letterSpacing);
  f.setStyleStrategy(QFont::ForceOutline);
  return f;
}

FontSpec FontSpec::fromFont(const QFont &f)
{
  FontSpec s;
  s.family = f.family();
  qreal pt = f.pointSizeF();
  if (pt <= 0 && f.pixelSize() > 0) pt = f.pixelSize() * 72.0 / 96.0;
  s.pointSize = pt > 0 ? pt : 12;
  s.weight = f.weight();
  s.italic = f.italic();
  s.underline = f.underline();
  s.strikeOut = f.strikeOut();
  s.stretch = f.stretch();
  if (f.letterSpacingType() == QFont::AbsoluteSpacing) s.letterSpacing = f.letterSpacing();
  return s;
}

QTransform Element::itemTransform() const
{
  QTransform tx;
  tx.translate(rect.x() + rect.width() / 2, rect.y() + rect.height() / 2);
  tx.shear(skewH, skewV);
  tx.rotate(rotation);
  tx.translate(-rect.x() - rect.width() / 2, -rect.y() - rect.height() / 2);
  return tx;
}

bool TemplateDocument::hasReplay() const
{
  for (const Element &e : items)
    if (e.type == ItemType::Replay) return true;
  return false;
}

// ---------------------------------------------------------------------------------------------------------------
// sanitizing
// ---------------------------------------------------------------------------------------------------------------

static double clampd(double v, double lo, double hi, double fallback = 0)
{
  if (!std::isfinite(v)) return fallback;
  return qBound(lo, v, hi);
}

static QPointF clampPoint(const QPointF &p)
{
  return QPointF(clampd(p.x(), -limits::maxCoord, limits::maxCoord), clampd(p.y(), -limits::maxCoord, limits::maxCoord));
}

static QRectF clampRect(const QRectF &r)
{
  QRectF n = r.normalized();
  return QRectF(clampd(n.x(), -limits::maxCoord, limits::maxCoord), clampd(n.y(), -limits::maxCoord, limits::maxCoord),
                clampd(n.width(), 0, limits::maxCoord), clampd(n.height(), 0, limits::maxCoord));
}

static QColor validColor(const QColor &c, const QColor &fallback)
{
  return c.isValid() ? c : fallback;
}

static QImage limitImage(const QImage &img)
{
  if (img.isNull()) return img;
  if (img.width() > limits::maxImageSide || img.height() > limits::maxImageSide)
    return img.scaled(limits::maxImageSide, limits::maxImageSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  return img;
}

void sanitize(Element &e)
{
  e.id = e.id.left(limits::maxStringLength);
  e.pos = clampPoint(e.pos);
  e.rect = clampRect(e.rect);
  e.rotation = clampd(e.rotation, -3600, 3600);
  e.skewH = clampd(e.skewH, -10, 10);
  e.skewV = clampd(e.skewV, -10, 10);
  e.opacity = clampd(e.opacity, 0, 1, 1);
  e.fill = validColor(e.fill, QColor(Qt::black));
  e.text = e.text.left(limits::maxTextLength);

  Outline &o = e.outline;
  o.color = validColor(o.color, QColor(Qt::black));
  o.width = clampd(o.width, 0, 200, 1);
  if (o.width <= 0)   // an outline of no width is no outline; the width stays usable for when it is switched on again
    {
      o.style = Qt::NoPen;
      o.width = 1;
    }
  o.miterLimit = clampd(o.miterLimit, 1, 100, 2);

  FontSpec &f = e.font;
  f.family = f.family.left(limits::maxStringLength);
  f.pointSize = clampd(f.pointSize, 1, 2000, 12);
  f.weight = qBound(1, f.weight, 1000);
  f.stretch = qBound(0, f.stretch, 4000);
  f.letterSpacing = clampd(f.letterSpacing, -100, 1000);

  Gradient &g = e.gradient;
  g.angle = qBound(-3600, g.angle, 3600);
  if (g.stops.size() > limits::maxGradientStops) g.stops.resize(limits::maxGradientStops);
  for (GradientStop &s : g.stops)
    {
      s.pos = clampd(s.pos, 0, 1);
      s.color = validColor(s.color, QColor(Qt::black));
    }
  e.image = limitImage(e.image);
  e.radius = clampd(e.radius, 0, limits::maxCoord);
  Shadow &sh = e.shadow;
  sh.color = validColor(sh.color, QColor(0, 0, 0, 160));
  sh.dx = clampd(sh.dx, -limits::maxShadowOffset, limits::maxShadowOffset);
  sh.dy = clampd(sh.dy, -limits::maxShadowOffset, limits::maxShadowOffset);
  sh.blur = clampd(sh.blur, 0, limits::maxShadowBlur);
}

void autoAnchor(Element &e, const QRectF &canvas)
{
  e.anchorH = AnchorH::Left;
  e.anchorV = AnchorV::Top;
  if (canvas.width() <= 0 || canvas.height() <= 0) return;
  const bool freeText = e.type == ItemType::Text && !e.textBox;
  const bool turned = e.rotation != 0 || e.skewH != 0 || e.skewV != 0;
  QRectF frame = freeText ? QRectF(e.pos, QSizeF(0, 0)) : e.rect.translated(e.pos);
  // a turned item is placed by where it really is, and (not being stretched when anchored) never spans
  if (turned && !freeText) frame = (e.itemTransform() * QTransform::fromTranslate(e.pos.x(), e.pos.y())).mapRect(e.rect);
  const double cx = (frame.center().x() - canvas.left()) / canvas.width();
  const double cy = (frame.center().y() - canvas.top()) / canvas.height();
  e.anchorH = cx < 1.0 / 3 ? AnchorH::Left : (cx > 2.0 / 3 ? AnchorH::Right : AnchorH::Center);
  e.anchorV = cy < 1.0 / 3 ? AnchorV::Top : (cy > 2.0 / 3 ? AnchorV::Bottom : AnchorV::Middle);
  if (freeText || turned) return;
  // an item that touches one side follows that side, whichever third its center is in (a wide picture against the right edge)
  const double nearX = 0.05 * canvas.width(), nearY = 0.05 * canvas.height();
  const double gapL = frame.left() - canvas.left(), gapR = canvas.right() - frame.right();
  const double gapT = frame.top() - canvas.top(), gapB = canvas.bottom() - frame.bottom();
  if (gapL < nearX && gapR >= nearX) e.anchorH = AnchorH::Left;
  else if (gapR < nearX && gapL >= nearX) e.anchorH = AnchorH::Right;
  if (gapT < nearY && gapB >= nearY) e.anchorV = AnchorV::Top;
  else if (gapB < nearY && gapT >= nearY) e.anchorV = AnchorV::Bottom;
  if (frame.width() >= 0.8 * canvas.width()) e.anchorH = AnchorH::Span;
  if (frame.height() >= 0.8 * canvas.height()) e.anchorV = AnchorV::Span;
}

void reanchor(const Element &before, Element &after, const QRectF &canvas)
{
  if (before.pos == after.pos && before.rect == after.rect && before.rotation == after.rotation && before.skewH == after.skewH && before.skewV == after.skewV)
    return;
  Element was = before;
  autoAnchor(was, canvas);
  Element now = after;
  autoAnchor(now, canvas);
  if (after.anchorH == before.anchorH && before.anchorH == was.anchorH) after.anchorH = now.anchorH;
  if (after.anchorV == before.anchorV && before.anchorV == was.anchorV) after.anchorV = now.anchorV;
}

void sanitize(TemplateDocument &d)
{
  d.canvas = clampRect(d.canvas);
  if (d.canvas.width() < 1 || d.canvas.height() < 1) d.canvas = QRectF(0, 0, 320, 256);
  if (d.items.size() > limits::maxItems) d.items.resize(limits::maxItems);
  if (!d.background.isValid()) d.background = QColor(248, 248, 248);
  d.background.setAlpha(255);
  if (!std::isfinite(d.transparencyPoint.x()) || !std::isfinite(d.transparencyPoint.y()))
    d.transparencyPoint = d.canvas.topLeft();
  d.transparencyPoint.setX(qBound(d.canvas.left(), d.transparencyPoint.x(), std::nextafter(d.canvas.right(), d.canvas.left())));
  d.transparencyPoint.setY(qBound(d.canvas.top(), d.transparencyPoint.y(), std::nextafter(d.canvas.bottom(), d.canvas.top())));
  for (Element &e : d.items) sanitize(e);
}

// ---------------------------------------------------------------------------------------------------------------
// colors
// ---------------------------------------------------------------------------------------------------------------

QString colorToString(const QColor &c)
{
  QString s = QStringLiteral("#%1%2%3").arg(c.red(), 2, 16, QChar('0')).arg(c.green(), 2, 16, QChar('0')).arg(c.blue(), 2, 16, QChar('0'));
  if (c.alpha() != 255) s += QStringLiteral("%1").arg(c.alpha(), 2, 16, QChar('0'));
  return s;
}

QColor colorFromString(const QString &str, const QColor &fallback)
{
  QString s = str.trimmed();
  if (!s.startsWith('#')) return fallback;
  s.remove(0, 1);
  bool ok = false;
  auto h = [&](int pos, int len) { return s.mid(pos, len).toInt(&ok, 16); };
  int r, g, b, a = 255;
  if (s.size() == 3)
    {
      r = h(0, 1) * 17; if (!ok) return fallback;
      g = h(1, 1) * 17; if (!ok) return fallback;
      b = h(2, 1) * 17; if (!ok) return fallback;
    }
  else if (s.size() == 6 || s.size() == 8)
    {
      r = h(0, 2); if (!ok) return fallback;
      g = h(2, 2); if (!ok) return fallback;
      b = h(4, 2); if (!ok) return fallback;
      if (s.size() == 8) { a = h(6, 2); if (!ok) return fallback; }
    }
  else
    {
      return fallback;
    }
  return QColor(r, g, b, a);
}

// ---------------------------------------------------------------------------------------------------------------
// JSON
// ---------------------------------------------------------------------------------------------------------------

static const struct { ItemType v; const char *name; } typeNames[] = {
  {ItemType::Text, "text"}, {ItemType::Rect, "rect"}, {ItemType::Ellipse, "ellipse"},
  {ItemType::Line, "line"}, {ItemType::Image, "image"}, {ItemType::Replay, "replay"}};

static const struct { Qt::PenJoinStyle v; const char *name; } joinNames[] = {
  {Qt::MiterJoin, "miter"}, {Qt::BevelJoin, "bevel"}, {Qt::RoundJoin, "round"}};
static const struct { Qt::PenCapStyle v; const char *name; } capNames[] = {
  {Qt::FlatCap, "flat"}, {Qt::SquareCap, "square"}, {Qt::RoundCap, "round"}};
static const struct { Qt::PenStyle v; const char *name; } styleNames[] = {
  {Qt::SolidLine, "solid"}, {Qt::DashLine, "dash"}, {Qt::DotLine, "dot"}, {Qt::DashDotLine, "dashdot"},
  {Qt::DashDotDotLine, "dashdotdot"}, {Qt::NoPen, "none"}};
static const struct { Gradient::Kind v; const char *name; } kindNames[] = {
  {Gradient::Linear, "linear"}, {Gradient::Radial, "radial"}, {Gradient::Conical, "conical"}};
static const struct { AnchorH v; const char *name; } anchorHNames[] = {
  {AnchorH::Left, "left"}, {AnchorH::Center, "center"}, {AnchorH::Right, "right"}, {AnchorH::Span, "span"}, {AnchorH::Scale, "scale"}};
static const struct { AnchorV v; const char *name; } anchorVNames[] = {
  {AnchorV::Top, "top"}, {AnchorV::Middle, "middle"}, {AnchorV::Bottom, "bottom"}, {AnchorV::Span, "span"}, {AnchorV::Scale, "scale"}};
static const struct { TextAlign v; const char *name; } alignNames[] = {
  {TextAlign::Left, "left"}, {TextAlign::Center, "center"}, {TextAlign::Right, "right"}};
static const struct { TextVAlign v; const char *name; } valignNames[] = {
  {TextVAlign::Top, "top"}, {TextVAlign::Middle, "middle"}, {TextVAlign::Bottom, "bottom"}};

template <class E, size_t N, class V> static const char *nameOf(const E (&table)[N], V v, const char *fallback)
{
  for (const E &t : table)
    if (t.v == v) return t.name;
  return fallback;
}

template <class E, size_t N, class V> static V valueOf(const E (&table)[N], const QString &name, V fallback)
{
  for (const E &t : table)
    if (name == QLatin1String(t.name)) return t.v;
  return fallback;
}

static double jnum(const QJsonObject &o, const char *key, double def)
{
  const QJsonValue v = o.value(QLatin1String(key));
  return v.isDouble() ? v.toDouble() : def;
}

static QString jstr(const QJsonObject &o, const char *key, const QString &def = QString())
{
  const QJsonValue v = o.value(QLatin1String(key));
  return v.isString() ? v.toString() : def;
}

static bool jbool(const QJsonObject &o, const char *key, bool def)
{
  const QJsonValue v = o.value(QLatin1String(key));
  return v.isBool() ? v.toBool() : def;
}

static QString imageToDataUri(const QImage &img)
{
  QByteArray ba;
  QBuffer b(&ba);
  b.open(QIODevice::WriteOnly);
  img.save(&b, "PNG");
  return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(ba.toBase64());
}

static QImage imageFromDataUri(const QString &s)
{
  static const QLatin1String prefix("data:image/png;base64,");
  if (!s.startsWith(prefix)) return QImage();
  const QByteArray raw = QByteArray::fromBase64(s.mid(prefix.size()).toLatin1());
  QBuffer b;
  b.setData(raw);
  b.open(QIODevice::ReadOnly);
  QImageReader r(&b, "PNG");
  const QSize sz = r.size();
  if (!sz.isValid() || qint64(sz.width()) * sz.height() > 64LL * 1024 * 1024) return QImage();
  return limitImage(r.read());
}

static QJsonObject gradientToJson(const Gradient &g)
{
  QJsonArray stops;
  for (const GradientStop &s : g.stops) stops.append(QJsonArray{s.pos, colorToString(s.color)});
  return QJsonObject{{"type", nameOf(kindNames, g.kind, "linear")}, {"angle", g.angle}, {"stops", stops}};
}

static Gradient gradientFromJson(const QJsonObject &o)
{
  Gradient g;
  g.kind = valueOf(kindNames, jstr(o, "type"), Gradient::None);
  g.angle = qRound(jnum(o, "angle", 0));
  const QJsonArray stops = o.value(QLatin1String("stops")).toArray();
  for (const QJsonValue &sv : stops)
    {
      const QJsonArray a = sv.toArray();
      if (a.size() < 2 || !a.at(0).isDouble() || !a.at(1).isString()) continue;
      if (g.stops.size() >= limits::maxGradientStops) break;
      GradientStop s;
      s.pos = a.at(0).toDouble();
      s.color = colorFromString(a.at(1).toString(), QColor(Qt::black));
      g.stops.append(s);
    }
  return g;
}

static QJsonObject outlineToJson(const Outline &o)
{
  return QJsonObject{{"color", colorToString(o.color)}, {"w", o.width}, {"join", nameOf(joinNames, o.join, "bevel")},
                     {"cap", nameOf(capNames, o.cap, "square")}, {"style", nameOf(styleNames, o.style, "solid")},
                     {"miter", o.miterLimit}};
}

static Outline outlineFromJson(const QJsonObject &o)
{
  Outline r;
  r.color = colorFromString(jstr(o, "color"), r.color);
  r.width = jnum(o, "w", r.width);
  r.join = valueOf(joinNames, jstr(o, "join"), r.join);
  r.cap = valueOf(capNames, jstr(o, "cap"), r.cap);
  r.style = valueOf(styleNames, jstr(o, "style"), r.style);
  r.miterLimit = jnum(o, "miter", r.miterLimit);
  return r;
}

static QJsonObject fontToJson(const FontSpec &f)
{
  QJsonObject o{{"family", f.family}, {"size", f.pointSize}, {"weight", f.weight}};
  if (f.italic) o["italic"] = true;
  if (f.underline) o["underline"] = true;
  if (f.strikeOut) o["strikeout"] = true;
  if (f.stretch > 0) o["stretch"] = f.stretch;
  if (f.letterSpacing != 0) o["spacing"] = f.letterSpacing;
  return o;
}

static FontSpec fontFromJson(const QJsonObject &o)
{
  FontSpec f;
  f.family = jstr(o, "family");
  f.pointSize = jnum(o, "size", f.pointSize);
  f.weight = qRound(jnum(o, "weight", f.weight));
  f.italic = jbool(o, "italic", false);
  f.underline = jbool(o, "underline", false);
  f.strikeOut = jbool(o, "strikeout", false);
  f.stretch = qRound(jnum(o, "stretch", 0));
  f.letterSpacing = jnum(o, "spacing", 0);
  return f;
}

static QJsonObject elementToJson(const Element &e)
{
  QJsonObject o;
  o["type"] = nameOf(typeNames, e.type, "rect");
  if (!e.id.isEmpty()) o["id"] = e.id;
  o["x"] = e.pos.x();
  o["y"] = e.pos.y();
  if (e.rect.x() != 0) o["rx"] = e.rect.x();
  if (e.rect.y() != 0) o["ry"] = e.rect.y();
  if (e.type != ItemType::Text) { o["w"] = e.rect.width(); o["h"] = e.rect.height(); }
  else if (e.textBox || e.rect != QRectF(0, 0, 100, 100)) { o["w"] = e.rect.width(); o["h"] = e.rect.height(); }
  if (e.rotation != 0) o["rotation"] = e.rotation;
  if (e.skewH != 0) o["skewX"] = e.skewH;
  if (e.skewV != 0) o["skewY"] = e.skewV;
  if (e.opacity != 1.0) o["opacity"] = e.opacity;
  if (e.locked) o["locked"] = true;
  o["fill"] = colorToString(e.fill);
  if (e.gradient.kind != Gradient::None) o["gradient"] = gradientToJson(e.gradient);
  o["outline"] = outlineToJson(e.outline);
  if (e.type == ItemType::Text)
    {
      o["text"] = e.text;
      o["font"] = fontToJson(e.font);
      if (e.textBox)
        {
          o["box"] = true;
          o["align"] = nameOf(alignNames, e.align, "left");
          o["valign"] = nameOf(valignNames, e.valign, "top");
          if (e.wrap) o["wrap"] = true;
          if (e.shrinkToFit) o["fit"] = "shrink";
        }
    }
  if ((e.type == ItemType::Image || e.type == ItemType::Text) && !e.image.isNull()) o["image"] = imageToDataUri(e.image);
  if (e.radius > 0) o["radius"] = e.radius;
  if (e.shadow.on)
    o["shadow"] = QJsonObject{{"color", colorToString(e.shadow.color)}, {"dx", e.shadow.dx}, {"dy", e.shadow.dy}, {"blur", e.shadow.blur}};
  if (e.anchorH != AnchorH::Left || e.anchorV != AnchorV::Top)
    o["anchor"] = QJsonObject{{"h", nameOf(anchorHNames, e.anchorH, "left")}, {"v", nameOf(anchorVNames, e.anchorV, "top")}};
  return o;
}

static bool elementFromJson(const QJsonObject &o, Element &e)
{
  const QString t = jstr(o, "type");
  bool known = false;
  for (const auto &n : typeNames)
    if (t == QLatin1String(n.name)) { e.type = n.v; known = true; }
  if (!known) return false;
  e.id = jstr(o, "id");
  e.pos = QPointF(jnum(o, "x", 0), jnum(o, "y", 0));
  e.rect = QRectF(jnum(o, "rx", 0), jnum(o, "ry", 0), jnum(o, "w", 100), jnum(o, "h", 100));
  e.rotation = jnum(o, "rotation", 0);
  e.skewH = jnum(o, "skewX", 0);
  e.skewV = jnum(o, "skewY", 0);
  e.opacity = jnum(o, "opacity", 1.0);
  e.locked = jbool(o, "locked", false);
  e.fill = colorFromString(jstr(o, "fill"), QColor(Qt::white));
  if (o.value(QLatin1String("gradient")).isObject()) e.gradient = gradientFromJson(o.value(QLatin1String("gradient")).toObject());
  if (o.value(QLatin1String("outline")).isObject()) e.outline = outlineFromJson(o.value(QLatin1String("outline")).toObject());
  if (e.type == ItemType::Text)
    {
      e.text = jstr(o, "text");
      e.font = fontFromJson(o.value(QLatin1String("font")).toObject());
      e.textBox = jbool(o, "box", false);
      e.align = valueOf(alignNames, jstr(o, "align"), TextAlign::Left);
      e.valign = valueOf(valignNames, jstr(o, "valign"), TextVAlign::Top);
      e.wrap = jbool(o, "wrap", false);
      e.shrinkToFit = jstr(o, "fit") == QLatin1String("shrink");
    }
  if (e.type == ItemType::Image || e.type == ItemType::Text) e.image = imageFromDataUri(jstr(o, "image"));
  e.radius = jnum(o, "radius", 0);
  if (o.value(QLatin1String("shadow")).isObject())
    {
      const QJsonObject so = o.value(QLatin1String("shadow")).toObject();
      e.shadow.on = true;
      e.shadow.color = colorFromString(jstr(so, "color"), e.shadow.color);
      e.shadow.dx = jnum(so, "dx", e.shadow.dx);
      e.shadow.dy = jnum(so, "dy", e.shadow.dy);
      e.shadow.blur = jnum(so, "blur", 0);
    }
  if (o.value(QLatin1String("anchor")).isObject())
    {
      const QJsonObject ao = o.value(QLatin1String("anchor")).toObject();
      e.anchorH = valueOf(anchorHNames, jstr(ao, "h"), AnchorH::Left);
      e.anchorV = valueOf(anchorVNames, jstr(ao, "v"), AnchorV::Top);
    }
  return true;
}

QByteArray toJson(const TemplateDocument &doc)
{
  QJsonArray items;
  for (const Element &e : doc.items) items.append(elementToJson(e));
  QJsonObject canvas{{"w", doc.canvas.width()}, {"h", doc.canvas.height()}};
  if (doc.canvas.x() != 0) canvas["x"] = doc.canvas.x();
  if (doc.canvas.y() != 0) canvas["y"] = doc.canvas.y();
  bool needsV2 = doc.sampledTransparency;
  for (const Element &e : doc.items) needsV2 |= e.type == ItemType::Text && !e.image.isNull();
  QJsonObject root{{"format", formatName}, {"version", needsV2 ? currentVersion : 1}, {"canvas", canvas},
                   {"layout", doc.layout == TemplateDocument::Anchored ? "anchored" : "stretch"}, {"items", items}};
  if (doc.sampledTransparency)
    root["sampledTransparency"] = QJsonObject{{"background", colorToString(doc.background)},
      {"x", doc.transparencyPoint.x()}, {"y", doc.transparencyPoint.y()}};
  return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

LoadResult loadJson(const QByteArray &data)
{
  LoadResult r;
  if (data.size() > limits::maxFileBytes)
    {
      r.error = QStringLiteral("file too large");
      return r;
    }
  QJsonParseError pe;
  const QJsonDocument jd = QJsonDocument::fromJson(data, &pe);
  if (pe.error != QJsonParseError::NoError || !jd.isObject())
    {
      r.error = QStringLiteral("not a valid template file (%1)").arg(pe.errorString());
      return r;
    }
  const QJsonObject root = jd.object();
  if (jstr(root, "format") != QLatin1String(formatName))
    {
      r.error = QStringLiteral("not a QSSTV template");
      return r;
    }
  const int version = qRound(jnum(root, "version", 0));
  if (version < 1 || version > currentVersion)
    {
      r.error = QStringLiteral("template format version %1 is not supported by this program").arg(version);
      return r;
    }
  const QJsonObject c = root.value(QLatin1String("canvas")).toObject();
  r.doc.canvas = QRectF(jnum(c, "x", 0), jnum(c, "y", 0), jnum(c, "w", 320), jnum(c, "h", 256));
  r.doc.layout = jstr(root, "layout") == QLatin1String("anchored") ? TemplateDocument::Anchored : TemplateDocument::Stretch;
  if (root.contains("sampledTransparency"))
    {
      if (version < 2 || !root.value("sampledTransparency").isObject())
        { r.error = QStringLiteral("invalid transparency metadata"); return r; }
      const QJsonObject key = root.value("sampledTransparency").toObject();
      r.doc.sampledTransparency = true;
      r.doc.background = colorFromString(jstr(key, "background"), QColor(248, 248, 248));
      r.doc.transparencyPoint = QPointF(jnum(key, "x", 0), jnum(key, "y", 0));
      if (!r.doc.canvas.contains(r.doc.transparencyPoint))
        { r.error = QStringLiteral("transparency point is outside the canvas"); return r; }
    }
  const QJsonArray items = root.value(QLatin1String("items")).toArray();
  if (items.size() > limits::maxItems)
    {
      r.error = QStringLiteral("too many items");
      return r;
    }
  int skipped = 0;
  for (const QJsonValue &v : items)
    {
      Element e;
      if (!v.isObject() || !elementFromJson(v.toObject(), e))
        {
          skipped++;
          continue;
        }
      r.doc.items.append(e);
    }
  if (skipped) r.warnings << QStringLiteral("%1 item(s) of an unknown kind were ignored").arg(skipped);
  sanitize(r.doc);
  r.ok = true;
  return r;
}

// ---------------------------------------------------------------------------------------------------------------
// files
// ---------------------------------------------------------------------------------------------------------------

bool looksLikeTemplate(const QByteArray &head)
{
  if (head.size() >= 4 && head.startsWith("4QZS")) return true;     // legacy magic, big endian ('4'<<24)+('Q'<<16)+('Z'<<8)+'S'
  int i = 0;
  if (head.startsWith("\xEF\xBB\xBF")) i = 3;                      // UTF-8 byte order mark
  while (i < head.size() && (head.at(i) == ' ' || head.at(i) == '\t' || head.at(i) == '\r' || head.at(i) == '\n')) i++;
  return i < head.size() && head.at(i) == '{';
}

LoadResult loadFile(const QString &fileName)
{
  LoadResult r;
  QFile f(fileName);
  if (fileName.isEmpty() || !f.open(QIODevice::ReadOnly))
    {
      r.error = QStringLiteral("cannot open %1").arg(fileName);
      return r;
    }
  if (f.size() > limits::maxFileBytes)
    {
      r.error = QStringLiteral("file too large");
      return r;
    }
  const QByteArray head = f.peek(64);
  if (!looksLikeTemplate(head))
    {
      r.error = QStringLiteral("not a template file");
      return r;
    }
  const QByteArray data = f.readAll();
  return head.startsWith("4QZS") ? loadLegacy(data) : loadJson(data);
}

LoadResult loadFileCached(const QString &fileName)
{
  static QMutex mutex;
  static QString cPath;
  static qint64 cSize = -1;
  static qint64 cTime = 0;
  static LoadResult cResult;
  const QFileInfo fi(fileName);
  QMutexLocker lock(&mutex);
  if (fi.exists() && fileName == cPath && fi.size() == cSize && fi.lastModified().toMSecsSinceEpoch() == cTime && cResult.ok) return cResult;
  LoadResult r = loadFile(fileName);
  if (r.ok && fi.exists())
    {
      cPath = fileName;
      cSize = fi.size();
      cTime = fi.lastModified().toMSecsSinceEpoch();
      cResult = r;
    }
  return r;
}

bool saveFile(const QString &fileName, const TemplateDocument &doc, QString *error)
{
  QSaveFile f(fileName);
  if (!f.open(QIODevice::WriteOnly))
    {
      if (error) *error = f.errorString();
      return false;
    }
  const QByteArray data = toJson(doc);
  if (data.size() > limits::maxFileBytes)
    { if (error) *error = QStringLiteral("template exceeds the native file size limit"); return false; }
  if (f.write(data) != data.size())
    { if (error) *error = f.errorString(); return false; }
  if (!f.commit())
    {
      if (error) *error = f.errorString();
      return false;
    }
  return true;
}

}  // namespace tpl
