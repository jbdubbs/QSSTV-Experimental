/***************************************************************************
 *   Importer for the binary .templ format written by QSSTV 9.x and by the *
 *   old QSSTV-Experimental editor (QDataStream, magic "4QZS").            *
 *                                                                         *
 *   Why this is not a straight `str >> item`: Qt's own QPen/QBrush/QColor *
 *   readers crash (SIGSEGV) on an out-of-range enum byte, and the old     *
 *   loader never looked at QDataStream::status(), so a damaged or hostile *
 *   file could take the program down (the gallery parses every file in    *
 *   the templates folder). Here every field is range checked, the enum    *
 *   bytes Qt cannot cope with are verified before Qt reads them, and the  *
 *   stream status is checked after every field.                           *
 ***************************************************************************/
#include "templatedoc.h"

#include <QBuffer>
#include <QDataStream>
#include <QLineF>
#include <algorithm>
#include <cmath>

namespace tpl
{

namespace
{

const quint32 legacyMagic = (quint32('4') << 24) + (quint32('Q') << 16) + (quint32('Z') << 8) + quint32('S');

// item type values of the legacy graphItemBase::egraphType (QGraphicsItem::UserType + n)
const int userType = 65536;
enum LegacyType { lBase = userType + 1, lRectangle, lEllipse, lImage, lLine, lText, lReplay, lBorder };

struct LegacyGradient
{
  QColor color[4];
  int pos[4] = {0, 0, 0, 0};
  int type = 0;
  int direction = 0;
};

struct LegacyReader
{
  QBuffer buf;
  QDataStream str;

  explicit LegacyReader(const QByteArray &data) : str(&buf)
  {
    buf.setData(data);
    buf.open(QIODevice::ReadOnly);
  }

  bool ok() const { return str.status() == QDataStream::Ok; }

  // QColor::Spec: Invalid, Rgb, Hsv, Cmyk, Hsl, ExtendedRgb
  static bool validSpec(quint8 b) { return b <= 5; }
  // Qt::BrushStyle values QBrush can be constructed with (0..17); TexturePattern (24) carries an image and is never
  // written by the editor, so it is refused rather than parsed
  static bool validBrushStyle(quint8 b) { return b <= 17; }

  bool readColor(QColor &c)
  {
    const QByteArray p = buf.peek(1);
    if (p.size() < 1 || !validSpec(quint8(p.at(0)))) return false;
    str >> c;
    return ok();
  }

  bool readBrush(QBrush &b)
  {
    const QByteArray p = buf.peek(2);
    if (p.size() < 2 || !validBrushStyle(quint8(p.at(0))) || !validSpec(quint8(p.at(1)))) return false;
    str >> b;
    return ok();
  }

  // stream version >= 9: quint16 style|cap|join, bool cosmetic, double width, then the pen's QBrush (style byte at
  // offset 11, colour spec at offset 12), miter limit, dash pattern, dash offset
  bool readPen(QPen &pen)
  {
    const QByteArray p = buf.peek(13);
    if (p.size() < 13 || !validBrushStyle(quint8(p.at(11))) || !validSpec(quint8(p.at(12)))) return false;
    str >> pen;
    return ok();
  }

  bool readGradient(LegacyGradient &g)
  {
    for (int i = 0; i < 4; i++)
      if (!readColor(g.color[i])) return false;
    for (int i = 0; i < 4; i++) str >> g.pos[i];
    str >> g.type;
    str >> g.direction;
    return ok();
  }
};

struct RawItem
{
  int type = 0;
  int rotation = 0;
  double hShear = 0, vShear = 0, z = 0;
  QPointF pos;
  QRectF rect;
  QColor fill;
  QPen pen;
  bool locked = false;
  QImage image;
  QString text;
  QFont font;
  LegacyGradient gradient;
};

bool readItem(LegacyReader &r, RawItem &it)
{
  QDataStream &s = r.str;
  s >> it.rotation >> it.hShear >> it.vShear >> it.z;
  s >> it.pos >> it.rect;
  if (!r.ok()) return false;
  if (!r.readColor(it.fill)) return false;
  if (!r.readPen(it.pen)) return false;
  QBrush brush;                                   // the stored brush is re-derived from fill/gradient when painting
  if (!r.readBrush(brush)) return false;
  s >> it.locked;
  s >> it.image;
  s >> it.text;
  s >> it.font;
  QLineF line;                                    // unused by the legacy renderer
  s >> line;
  if (!r.ok()) return false;
  return r.readGradient(it.gradient);
}

Gradient convertGradient(const LegacyGradient &g)
{
  Gradient out;
  switch (g.type)
    {
    case 1: out.kind = Gradient::Linear; break;
    case 2: out.kind = Gradient::Radial; break;
    case 3: out.kind = Gradient::Conical; break;
    default: out.kind = Gradient::None; break;
    }
  out.angle = qBound(-3600, g.direction, 3600);
  // legacy grSetup(): each further stop only counts while the positions keep increasing
  for (int i = 0; i < 4; i++)
    {
      if (i > 0 && g.pos[i] <= g.pos[i - 1]) break;
      if (g.pos[i] >= 0 && g.pos[i] <= 100)
        {
          GradientStop st;
          st.pos = g.pos[i] / 100.;
          st.color = g.color[i];
          out.stops.append(st);
        }
    }
  return out;
}

}  // namespace

LoadResult loadLegacy(const QByteArray &data)
{
  LoadResult r;
  if (data.size() > limits::maxFileBytes)
    {
      r.error = QStringLiteral("file too large");
      return r;
    }
  LegacyReader rd(data);
  QDataStream &s = rd.str;
  quint32 magic = 0;
  s >> magic;
  if (!rd.ok() || magic != legacyMagic)
    {
      r.error = QStringLiteral("not a QSSTV template");
      return r;
    }
  QString version;
  quint16 streamVersion = 0;
  s >> version >> streamVersion;
  // the editor always wrote Qt_4_4 (10); 9 differs only in what the pen/brush layouts below do not touch
  if (!rd.ok() || (streamVersion != QDataStream::Qt_4_3 && streamVersion != QDataStream::Qt_4_4))
    {
      r.error = QStringLiteral("unsupported template stream version %1").arg(streamVersion);
      return r;
    }
  s.setVersion(streamVersion);

  struct Loaded { double z; int order; Element e; };
  QVector<Loaded> loaded;
  QRectF border;
  bool borderSet = false;
  int order = 0;
  while (!s.atEnd())
    {
      int type = 0;
      s >> type;
      if (!rd.ok() || type < lRectangle || type > lBorder)
        {
          r.error = QStringLiteral("damaged template (item %1)").arg(order);
          return r;
        }
      RawItem it;
      it.type = type;
      if (!readItem(rd, it))
        {
          r.error = QStringLiteral("damaged template (item %1)").arg(order);
          return r;
        }
      if (type == lBorder)
        {
          border = it.rect;
          borderSet = true;
          continue;
        }
      if (loaded.size() >= limits::maxItems)
        {
          r.error = QStringLiteral("too many items");
          return r;
        }
      Element e;
      switch (type)
        {
        case lRectangle: e.type = ItemType::Rect; break;
        case lEllipse: e.type = ItemType::Ellipse; break;
        case lImage: e.type = ItemType::Image; break;
        case lLine: e.type = ItemType::Line; break;
        case lText: e.type = ItemType::Text; break;
        default: e.type = ItemType::Replay; break;
        }
      e.pos = it.pos;
      e.rect = it.rect;
      e.rotation = it.rotation;
      e.skewH = it.hShear;
      e.skewV = it.vShear;
      e.locked = it.locked;
      e.fill = it.fill;
      e.gradient = convertGradient(it.gradient);
      e.outline.color = it.pen.color();
      e.outline.width = it.pen.widthF();
      // the legacy renderer drew rectangles and picture borders with mitred corners whatever the pen said
      const bool mitred = e.type == ItemType::Rect || e.type == ItemType::Image || e.type == ItemType::Replay;
      e.outline.join = mitred ? Qt::MiterJoin : it.pen.joinStyle();
      e.outline.cap = it.pen.capStyle();
      e.outline.miterLimit = it.pen.miterLimit();
      const Qt::PenStyle ps = it.pen.style();
      e.outline.style = (ps == Qt::CustomDashLine) ? Qt::SolidLine : ps;
      e.image = it.image;
      e.text = it.text;
      e.font = FontSpec::fromFont(it.font);
      loaded.append({std::isfinite(it.z) ? it.z : 0, order++, e});
    }

  // the legacy scene stacked items by their saved z, equal z in file order (later on top)
  std::stable_sort(loaded.begin(), loaded.end(), [](const Loaded &a, const Loaded &b) { return a.z < b.z; });
  for (const Loaded &l : loaded) r.doc.items.append(l.e);
  r.doc.canvas = borderSet ? border : QRectF(0, 0, 320, 256);
  r.doc.layout = TemplateDocument::Stretch;
  r.doc.fromLegacy = true;
  sanitize(r.doc);
  r.ok = true;
  return r;
}

}  // namespace tpl
