// Tests for src/template: legacy import, JSON round trip, validation and the renderer's parity with the old editor.
//   test <fixtures-dir> <golden-dir>
// Built and run by tests/template/run.sh (standalone g++ build, no application globals needed).
#include <QtGui>
#include <cmath>
#include "templatedoc.h"
#include "templaterender.h"
#include "templatevars.h"

static int passed = 0, failed = 0;

static void check(bool ok, const QString &what)
{
  printf("  %s %s\n", ok ? "ok  " : "FAIL", qPrintable(what));
  ok ? passed++ : failed++;
}

// PSNR over the RGB channels of two same-sized images (composited on black), 999 when identical
static double psnr(const QImage &a, const QImage &b, int *maxDiff = nullptr)
{
  QImage x = a.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  QImage y = b.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  if (x.size() != y.size() || x.isNull()) return -1;
  double mse = 0;
  int md = 0;
  for (int j = 0; j < x.height(); j++)
    {
      const QRgb *p = reinterpret_cast<const QRgb *>(x.constScanLine(j));
      const QRgb *q = reinterpret_cast<const QRgb *>(y.constScanLine(j));
      for (int i = 0; i < x.width(); i++)
        {
          const int d[4] = {qRed(p[i]) - qRed(q[i]), qGreen(p[i]) - qGreen(q[i]), qBlue(p[i]) - qBlue(q[i]), qAlpha(p[i]) - qAlpha(q[i])};
          for (int c = 0; c < 4; c++)
            {
              mse += double(d[c]) * d[c];
              md = qMax(md, qAbs(d[c]));
            }
        }
    }
  if (maxDiff) *maxDiff = md;
  mse /= double(x.width()) * x.height() * 4;
  return mse == 0 ? 999 : 10 * std::log10(255.0 * 255.0 / mse);
}

static tpl::RenderOptions options(const QImage &replay)
{
  tpl::RenderOptions o;
  o.vars.insert('c', "W1AW");
  o.vars.insert('r', "599");
  o.vars.insert('m', "N0CALL");
  o.vars.insert('t', "12:34");
  o.vars.insert('d', "2026/10/22");
  o.replay = replay;
  return o;
}

static void testGoldenParity(const QString &fx, const QString &gd)
{
  printf("== renderer parity with the legacy editor (golden images)\n");
  tpl::LoadResult lr = tpl::loadFile(fx + "/synthetic.templ");
  check(lr.ok, "synthetic.templ loads (" + lr.error + ")");
  if (!lr.ok) return;
  check(lr.doc.fromLegacy && lr.doc.items.size() == 14, QString("14 items imported (got %1)").arg(lr.doc.items.size()));
  check(lr.doc.canvas == QRectF(0, 0, 640, 496), "canvas is the saved border 640x496");
  check(lr.doc.hasReplay(), "replay slot present");
  // the legacy renderer mitred rectangles and picture borders whatever the pen said (the fixture's rectangle has a round-join pen)
  bool mitred = true, roundText = false;
  for (const tpl::Element &e : lr.doc.items)
    {
      if (e.type == tpl::ItemType::Rect || e.type == tpl::ItemType::Image || e.type == tpl::ItemType::Replay) mitred &= e.outline.join == Qt::MiterJoin;
      if (e.type == tpl::ItemType::Text && e.outline.join == Qt::RoundJoin) roundText = true;
    }
  check(mitred && roundText, "imported rectangles and borders are mitred, text keeps the join of its pen");
  const QImage replay(fx + "/replay.png");
  for (QSize sz : {QSize(320, 256), QSize(640, 496), QSize(1024, 768)})
    {
      const QImage base(QString("%1/base_%2x%3.png").arg(gd).arg(sz.width()).arg(sz.height()));
      for (bool withReplay : {true, false})
        {
          const QString name = QString("synthetic_%1x%2_%3.png").arg(sz.width()).arg(sz.height()).arg(withReplay ? "replay" : "noreplay");
          const QImage golden(gd + "/" + name);
          const QImage got = tpl::overlay(lr.doc, base, options(withReplay ? replay : QImage()));
          int md = 0;
          const double p = psnr(got, golden, &md);
          check(p >= 45, QString("%1: PSNR %2 dB, max channel diff %3").arg(name).arg(p, 0, 'f', 1).arg(md));
          if (p < 45) got.save(QDir::tempPath() + "/tpl_fail_" + name);
        }
    }
  int md = 0;
  const QImage thumb = tpl::renderThumbnail(lr.doc);
  const double p = psnr(thumb, QImage(gd + "/synthetic_thumb.png"), &md);
  check(p >= 45, QString("thumbnail: PSNR %1 dB, max channel diff %2").arg(p, 0, 'f', 1).arg(md));
}

static void testJsonRoundTrip(const QString &fx, const QString &gd)
{
  printf("== JSON round trip\n");
  tpl::LoadResult lr = tpl::loadFile(fx + "/synthetic.templ");
  const QByteArray json = tpl::toJson(lr.doc);
  tpl::LoadResult jr = tpl::loadJson(json);
  check(jr.ok, "own output parses (" + jr.error + ")");
  if (!jr.ok) return;
  check(jr.doc.items.size() == lr.doc.items.size() && !jr.doc.fromLegacy, "same item count, no longer flagged legacy");
  const QImage base(gd + "/base_640x496.png");
  const QImage replay(fx + "/replay.png");
  const double p = psnr(tpl::overlay(lr.doc, base, options(replay)), tpl::overlay(jr.doc, base, options(replay)));
  check(p >= 60, QString("legacy and re-read JSON render alike: PSNR %1 dB").arg(p, 0, 'f', 1));
  check(tpl::toJson(jr.doc) == json, "JSON write is stable (write, read, write gives the same bytes)");
  check(json.size() < 40000, QString("document size %1 bytes").arg(json.size()));
  // a hand written minimal template
  const char *minimal = R"({"format":"qsstv-template","version":1,"canvas":{"w":100,"h":50},
      "items":[{"type":"rect","x":5,"y":5,"w":20,"h":10,"fill":"#f00"},{"type":"text","text":"%c","x":2,"y":20,"font":{"family":"Sans","size":10}}]})";
  tpl::LoadResult m = tpl::loadJson(minimal);
  check(m.ok && m.doc.items.size() == 2 && m.doc.canvas.width() == 100, "hand written minimal template loads");
  check(m.ok && m.doc.items.at(0).fill == QColor(255, 0, 0), "colour parsed");
}

static void testValidation()
{
  printf("== validation\n");
  check(!tpl::loadJson("{").ok, "broken JSON rejected");
  check(!tpl::loadJson("[]").ok, "non-object rejected");
  check(!tpl::loadJson(R"({"format":"other","version":1})").ok, "wrong format name rejected");
  check(!tpl::loadJson(R"({"format":"qsstv-template","version":99})").ok, "newer version rejected");
  check(!tpl::loadJson(R"({"format":"qsstv-template"})").ok, "missing version rejected");
  tpl::LoadResult r = tpl::loadJson(R"({"format":"qsstv-template","version":1,"canvas":{"w":1e30,"h":-5},
      "items":[{"type":"rect","x":1e300,"y":-1e300,"w":1e300,"h":1e300,"rotation":1e300,"opacity":7,
               "outline":{"w":1e9,"miter":-3},"fill":"#nothex"},
               {"type":"spaceship"},{"type":"text","text":"x","font":{"size":-4,"weight":99999}}]})");
  check(r.ok && r.doc.items.size() == 2, "unknown item kind skipped, others kept");
  check(!r.warnings.isEmpty(), "skipped item reported as a warning");
  if (r.ok)
    {
      const tpl::Element &e = r.doc.items.at(0);
      check(std::abs(e.pos.x()) <= tpl::limits::maxCoord && e.rect.width() <= tpl::limits::maxCoord, "huge coordinates clamped");
      check(e.rotation <= 3600 && e.opacity == 1.0 && e.outline.width <= 200 && e.outline.miterLimit >= 1, "rotation/opacity/pen clamped");
      check(e.fill.isValid(), "bad colour replaced");
      check(r.doc.canvas.width() >= 1 && r.doc.canvas.height() >= 1, "degenerate canvas replaced by a default");
      const tpl::Element &t = r.doc.items.at(1);
      check(t.font.pointSize >= 1 && t.font.weight <= 1000, "font size/weight clamped");
    }
  // too many items
  QByteArray many = R"({"format":"qsstv-template","version":1,"items":[)";
  for (int i = 0; i < tpl::limits::maxItems + 1; i++) many += i ? R"(,{"type":"line"})" : R"({"type":"line"})";
  many += "]}";
  check(!tpl::loadJson(many).ok, "more than the item limit rejected");
  // a gradient with a hundred stops, an image that is not a PNG, a 1 MB string
  QByteArray stops;
  for (int i = 0; i < 100; i++) stops += (i ? "," : "") + QByteArray(R"(["0.5","#fff"])");
  QByteArray bigText(1 << 20, 'a');
  tpl::LoadResult g = tpl::loadJson(R"({"format":"qsstv-template","version":1,"items":[
      {"type":"rect","gradient":{"type":"linear","stops":[[0,"#f00"],[1,"#00f"],[0.5,"#0f0"],[0.5,"#0f0"],[0.5,"#0f0"]]}},
      {"type":"image","image":"data:image/png;base64,AAAA"},
      {"type":"text","text":")" + bigText + R"("}]})");
  check(g.ok && g.doc.items.at(0).gradient.stops.size() == 5, "gradient stops read");
  check(g.ok && g.doc.items.at(1).image.isNull(), "garbage image data ignored");
  check(g.ok && g.doc.items.at(2).text.size() == tpl::limits::maxTextLength, "oversized text cut to the limit");
  check(tpl::looksLikeTemplate("4QZS....") && tpl::looksLikeTemplate("  \n{\"a\":1}") && tpl::looksLikeTemplate("\xEF\xBB\xBF{")
        && !tpl::looksLikeTemplate("GIF89a") && !tpl::looksLikeTemplate("\x89PNG") && !tpl::looksLikeTemplate(""), "content sniffing");
  check(!tpl::loadFile("/nonexistent/file.qtempl").ok, "missing file reported");
  check(!tpl::loadFile("/etc/hostname").ok, "a non-template file is refused without being parsed");
}

// --- layout, text box, shadow, rounded corners: the model and the file format ------------------------------------------
static tpl::Element element(tpl::ItemType t, QPointF pos, QRectF rect)
{
  tpl::Element e;
  e.type = t;
  e.pos = pos;
  e.rect = rect;
  return e;
}

static void testNewFieldsFormat()
{
  printf("== layout, text box, shadow and corner fields in the file\n");
  tpl::TemplateDocument d;
  d.canvas = QRectF(0, 0, 640, 496);
  d.layout = tpl::TemplateDocument::Anchored;
  tpl::Element bar = element(tpl::ItemType::Rect, QPointF(0, 422), QRectF(0, 0, 640, 74));
  bar.anchorH = tpl::AnchorH::Span;
  bar.anchorV = tpl::AnchorV::Bottom;
  bar.radius = 12;
  bar.id = "bar";
  tpl::Element call = element(tpl::ItemType::Text, QPointF(20, 440), QRectF(0, 0, 300, 60));
  call.text = "%c de %m";
  call.textBox = true;
  call.align = tpl::TextAlign::Center;
  call.valign = tpl::TextVAlign::Bottom;
  call.wrap = true;
  call.shrinkToFit = true;
  call.shadow.on = true;
  call.shadow.color = QColor(10, 20, 30, 200);
  call.shadow.dx = -3;
  call.shadow.dy = 4.5;
  call.shadow.blur = 6;
  call.anchorH = tpl::AnchorH::Right;
  call.anchorV = tpl::AnchorV::Middle;
  call.id = "call";
  d.items << bar << call;
  const QByteArray json = tpl::toJson(d);
  tpl::LoadResult r = tpl::loadJson(json);
  check(r.ok && r.doc.items.size() == 2 && r.doc.layout == tpl::TemplateDocument::Anchored, "an anchored document with the new fields reads back");
  if (!r.ok || r.doc.items.size() != 2) return;
  const tpl::Element &b = r.doc.items.at(0), &c = r.doc.items.at(1);
  check(b.anchorH == tpl::AnchorH::Span && b.anchorV == tpl::AnchorV::Bottom && b.radius == 12, "anchors and corner radius survive");
  check(c.textBox && c.align == tpl::TextAlign::Center && c.valign == tpl::TextVAlign::Bottom && c.wrap && c.shrinkToFit, "text box settings survive");
  check(c.rect == QRectF(0, 0, 300, 60), "the box rectangle survives");
  check(c.shadow.on && c.shadow.color == QColor(10, 20, 30, 200) && c.shadow.dx == -3 && c.shadow.dy == 4.5 && c.shadow.blur == 6, "shadow survives");
  check(c.anchorH == tpl::AnchorH::Right && c.anchorV == tpl::AnchorV::Middle, "anchors of a text survive");
  check(tpl::toJson(r.doc) == json, "and the write is stable");

  // old documents: nothing new appears in their JSON, so they are written exactly as before
  tpl::TemplateDocument plain;
  tpl::Element t = element(tpl::ItemType::Text, QPointF(5, 5), QRectF(0, 0, 100, 100));
  t.text = "x";
  plain.items << t << element(tpl::ItemType::Rect, QPointF(0, 0), QRectF(0, 0, 10, 10));
  const QByteArray pj = tpl::toJson(plain);
  check(!pj.contains("anchor") && !pj.contains("shadow") && !pj.contains("radius") && !pj.contains("box") && !pj.contains("align") && !pj.contains("wrap") && !pj.contains("fit"),
        "a template that uses none of them has none of the new keys");
  tpl::LoadResult pr = tpl::loadJson(pj);
  check(pr.ok && !pr.doc.items.at(0).textBox && !pr.doc.items.at(0).shadow.on && pr.doc.items.at(1).radius == 0 && pr.doc.items.at(1).anchorH == tpl::AnchorH::Left,
        "and reads back with everything off");

  // hostile values
  tpl::LoadResult h = tpl::loadJson(R"({"format":"qsstv-template","version":1,"items":[
      {"type":"rect","radius":1e300,"shadow":{"dx":1e300,"dy":-1e300,"blur":1e300,"color":"#nothex"},"anchor":{"h":"sideways","v":7}},
      {"type":"text","text":"x","box":"yes","align":"justify","valign":5,"wrap":3,"fit":"grow"},
      {"type":"rect","shadow":true,"anchor":"left"}]})");
  check(h.ok && h.doc.items.size() == 3, "hostile new fields do not make the file unreadable");
  if (h.ok && h.doc.items.size() == 3)
    {
      const tpl::Element &e = h.doc.items.at(0);
      check(e.radius <= tpl::limits::maxCoord && std::abs(e.shadow.dx) <= tpl::limits::maxShadowOffset && std::abs(e.shadow.dy) <= tpl::limits::maxShadowOffset
            && e.shadow.blur <= tpl::limits::maxShadowBlur && e.shadow.color.isValid(), "radius and shadow values are clamped");
      check(e.anchorH == tpl::AnchorH::Left && e.anchorV == tpl::AnchorV::Top, "an unknown anchor name falls back to left/top");
      const tpl::Element &t2 = h.doc.items.at(1);
      check(!t2.textBox && t2.align == tpl::TextAlign::Left && t2.valign == tpl::TextVAlign::Top && !t2.wrap && !t2.shrinkToFit, "wrong types or names for the text box fields mean off");
      check(!h.doc.items.at(2).shadow.on, "a shadow that is not an object is ignored");
    }
}

static void testAutoAnchor()
{
  printf("== automatic anchors\n");
  const QRectF canvas(0, 0, 600, 300);
  auto anchors = [&](tpl::Element e) {
    tpl::autoAnchor(e, canvas);
    return QString("%1/%2").arg(int(e.anchorH)).arg(int(e.anchorV));
  };
  using tpl::AnchorH;
  using tpl::AnchorV;
  const QString LT = QString("%1/%2").arg(int(AnchorH::Left)).arg(int(AnchorV::Top));
  const QString RB = QString("%1/%2").arg(int(AnchorH::Right)).arg(int(AnchorV::Bottom));
  const QString CM = QString("%1/%2").arg(int(AnchorH::Center)).arg(int(AnchorV::Middle));
  const QString SB = QString("%1/%2").arg(int(AnchorH::Span)).arg(int(AnchorV::Bottom));
  const QString RT = QString("%1/%2").arg(int(AnchorH::Right)).arg(int(AnchorV::Top));
  check(anchors(element(tpl::ItemType::Rect, QPointF(10, 10), QRectF(0, 0, 100, 50))) == LT, "a rectangle in the top left corner");
  check(anchors(element(tpl::ItemType::Rect, QPointF(480, 240), QRectF(0, 0, 100, 50))) == RB, "a rectangle in the bottom right corner");
  check(anchors(element(tpl::ItemType::Rect, QPointF(250, 130), QRectF(0, 0, 100, 40))) == CM, "a rectangle in the middle");
  check(anchors(element(tpl::ItemType::Rect, QPointF(0, 240), QRectF(0, 0, 600, 60))) == SB, "a bar across the bottom spans the width and sticks to the bottom");
  check(anchors(element(tpl::ItemType::Rect, QPointF(400, 10), QRectF(0, 0, 190, 140))) == RT, "a replay window in the top right corner");
  tpl::Element text = element(tpl::ItemType::Text, QPointF(20, 280), QRectF(0, -30, 500, 40));
  text.text = "wide text";
  check(anchors(text) == QString("%1/%2").arg(int(AnchorH::Left)).arg(int(AnchorV::Bottom)), "free text is placed by its origin, not by its (long) extent");
  text.pos = QPointF(450, 40);
  check(anchors(text) == RT, "free text whose origin is in the right third");
  text.textBox = true;
  text.rect = QRectF(0, 0, 580, 60);
  check(anchors(text) == QString("%1/%2").arg(int(AnchorH::Span)).arg(int(AnchorV::Top)), "text in a box that is nearly as wide as the canvas spans it");
  tpl::Element bar = element(tpl::ItemType::Rect, QPointF(0, 100), QRectF(0, 0, 580, 20));
  const QString spanTop = QString("%1").arg(int(AnchorH::Span));
  check(anchors(bar).startsWith(spanTop + "/"), "(a long flat bar spans)");
  bar.rotation = 90;   // turned upright, it stands in the middle: placed by where it is, not stretched
  bar.pos = QPointF(10, 150);
  bar.rect = QRectF(0, 0, 580, 20);
  const QString turnedAnchors = anchors(bar);
  check(!turnedAnchors.startsWith(spanTop + "/") && !turnedAnchors.endsWith("/" + QString::number(int(AnchorV::Span))), "a turned item never spans");
  tpl::Element off = element(tpl::ItemType::Rect, QPointF(-50, -50), QRectF(0, 0, 40, 40));
  tpl::autoAnchor(off, QRectF(0, 0, 0, 0));
  check(off.anchorH == AnchorH::Left && off.anchorV == AnchorV::Top, "an empty canvas gives the defaults");
}

// --- renderer: anchored layout, text box, shadow, rounded corners ------------------------------------------------------
static QImage transparent(int w, int h)
{
  QImage im(w, h, QImage::Format_ARGB32_Premultiplied);
  im.fill(0);
  return im;
}

/*! bounding box of the pixels that are at least `minAlpha` opaque (null when there are none) */
static QRect ink(const QImage &im, int minAlpha = 40)
{
  int x0 = im.width(), y0 = im.height(), x1 = -1, y1 = -1;
  for (int y = 0; y < im.height(); y++)
    for (int x = 0; x < im.width(); x++)
      if (qAlpha(im.pixel(x, y)) >= minAlpha)
        {
          x0 = qMin(x0, x);
          y0 = qMin(y0, y);
          x1 = qMax(x1, x);
          y1 = qMax(y1, y);
        }
  return x1 < 0 ? QRect() : QRect(QPoint(x0, y0), QPoint(x1, y1));
}

static bool sameImage(const QImage &a, const QImage &b)
{
  return a.size() == b.size() && a.format() == b.format() && a.constBits() && memcmp(a.constBits(), b.constBits(), size_t(a.sizeInBytes())) == 0;
}

static tpl::Element anchored(tpl::AnchorH h, tpl::AnchorV v, QPointF pos = QPointF(100, 50), QRectF rect = QRectF(0, 0, 200, 100))
{
  tpl::Element e = element(tpl::ItemType::Rect, pos, rect);
  e.anchorH = h;
  e.anchorV = v;
  return e;
}

static void testAnchoredLayout(const QString &fx, const QString &gd)
{
  printf("== anchored layout\n");
  using tpl::AnchorH;
  using tpl::AnchorV;
  const QRectF canvas(0, 0, 600, 300);
  auto at = [&](const tpl::Element &e, double tw, double th) {
    return tpl::anchoredElement(e, canvas, tpl::anchoredCanvasSize(canvas, QSizeF(tw, th)));
  };
  // wider target (800 x 300): one scale factor of 1, 200 canvas units of extra width
  check(tpl::anchoredScale(canvas, QSizeF(800, 300)) == 1.0 && tpl::anchoredCanvasSize(canvas, QSizeF(800, 300)) == QSizeF(800, 300), "a wider picture: factor 1, 200 extra units across");
  check(at(anchored(AnchorH::Left, AnchorV::Top), 800, 300).pos == QPointF(100, 50), "Left/Top stay where they are");
  check(at(anchored(AnchorH::Center, AnchorV::Top), 800, 300).pos == QPointF(200, 50), "Center moves by half the extra width");
  check(at(anchored(AnchorH::Right, AnchorV::Top), 800, 300).pos == QPointF(300, 50), "Right moves by all of it");
  const tpl::Element span = at(anchored(AnchorH::Span, AnchorV::Top), 800, 300);
  check(span.pos == QPointF(100, 50) && span.rect.width() == 400 && span.rect.height() == 100, "Span keeps the left edge and grows the width");
  const tpl::Element scale = at(anchored(AnchorH::Scale, AnchorV::Top), 800, 300);
  check(std::abs(scale.pos.x() - 100 * 800.0 / 600) < 1e-9 && std::abs(scale.rect.width() - 200 * 800.0 / 600) < 1e-9, "Scale grows position and width in proportion");
  // taller target (600 x 400)
  check(at(anchored(AnchorH::Left, AnchorV::Middle), 600, 400).pos == QPointF(100, 100), "Middle moves by half the extra height");
  check(at(anchored(AnchorH::Left, AnchorV::Bottom), 600, 400).pos == QPointF(100, 150), "Bottom moves by all of it");
  check(at(anchored(AnchorH::Left, AnchorV::Span), 600, 400).rect.height() == 200, "vertical Span grows the height");
  // a smaller picture of another shape (300 x 300): factor 0.5, canvas units 600 x 600
  check(tpl::anchoredScale(canvas, QSizeF(300, 300)) == 0.5 && tpl::anchoredCanvasSize(canvas, QSizeF(300, 300)) == QSizeF(600, 600), "a squarer, smaller picture: factor 0.5, 300 extra units down");
  check(at(anchored(AnchorH::Left, AnchorV::Bottom), 300, 300).pos == QPointF(100, 350), "Bottom follows the bottom edge");
  // same shape, any size: nothing moves
  for (const QSizeF &t : {QSizeF(600, 300), QSizeF(1200, 600), QSizeF(300, 150), QSizeF(320, 160)})
    {
      bool same = true;
      for (AnchorH h : {AnchorH::Center, AnchorH::Right, AnchorH::Span, AnchorH::Scale})
        for (AnchorV v : {AnchorV::Middle, AnchorV::Bottom, AnchorV::Span, AnchorV::Scale})
          {
            const tpl::Element e = anchored(h, v);
            const tpl::Element a = at(e, t.width(), t.height());
            same &= std::abs(a.pos.x() - e.pos.x()) < 1e-6 && std::abs(a.pos.y() - e.pos.y()) < 1e-6 && std::abs(a.rect.width() - e.rect.width()) < 1e-6
                    && std::abs(a.rect.height() - e.rect.height()) < 1e-6;
          }
      check(same, QString("target %1 x %2, the canvas' own shape: every anchor leaves the item alone").arg(t.width()).arg(t.height()));
    }
  // turned items and free text
  tpl::Element turned = anchored(AnchorH::Span, AnchorV::Span);
  turned.rotation = 30;
  const tpl::Element tt = at(turned, 800, 400);
  check(tt.rect == turned.rect && tt.pos == turned.pos, "a turned item is not grown by Span (that would shear it)");
  turned.anchorH = AnchorH::Right;
  turned.anchorV = AnchorV::Bottom;
  check(at(turned, 800, 300).pos == QPointF(300, 50) && at(turned, 600, 400).pos == QPointF(100, 150), "but it is moved by Right/Bottom like any other");
  turned.anchorH = AnchorH::Scale;
  turned.anchorV = AnchorV::Top;
  const tpl::Element ts = at(turned, 800, 300);
  check(ts.rect == turned.rect && std::abs((ts.pos.x() + ts.rect.center().x()) - (turned.pos.x() + turned.rect.center().x()) * 800.0 / 600) < 1e-9, "Scale only moves its centre in proportion");
  tpl::Element text = element(tpl::ItemType::Text, QPointF(100, 250), QRectF(0, -30, 150, 40));
  text.anchorH = AnchorH::Right;
  text.anchorV = AnchorV::Bottom;
  check(at(text, 800, 300).pos == QPointF(300, 250) && at(text, 600, 400).pos == QPointF(100, 350), "free text is moved by its origin");
  text.anchorH = AnchorH::Span;
  check(at(text, 800, 300).pos.x() == 100 && at(text, 800, 300).rect == text.rect, "Span on free text acts as Left");
  text.textBox = true;
  text.rect = QRectF(0, 0, 150, 40);
  text.anchorH = AnchorH::Span;
  check(at(text, 800, 300).rect.width() == 350, "text in a box does span");

  // rendering. Same shape: bit for bit what the old stretch gives
  tpl::LoadResult lr = tpl::loadFile(fx + "/synthetic.templ");
  tpl::TemplateDocument ad = lr.doc;
  ad.layout = tpl::TemplateDocument::Anchored;   // every item Left/Top
  const QImage replay(fx + "/replay.png");
  for (const QSize &sz : {QSize(1280, 992), QSize(480, 372), QSize(640, 496)})
    {
      QImage base(sz, QImage::Format_ARGB32);
      base.fill(QColor(30, 60, 90));
      check(sameImage(tpl::overlay(lr.doc, base, options(replay)), tpl::overlay(ad, base, options(replay))), QString("%1 x %2: the canvas' own shape renders exactly as the stretched layout").arg(sz.width()).arg(sz.height()));
    }
  // wider picture: text keeps its shape (stretch would double its width), a spanning bar reaches both edges
  tpl::TemplateDocument d;
  d.canvas = QRectF(0, 0, 400, 100);
  tpl::Element t = element(tpl::ItemType::Text, QPointF(20, 60), QRectF(0, -30, 150, 40));
  t.text = "HELLO";
  t.font.family = "Montserrat";
  t.font.pointSize = 30;
  t.font.weight = 700;
  t.fill = Qt::white;
  t.outline.style = Qt::NoPen;
  tpl::Element bar = element(tpl::ItemType::Rect, QPointF(0, 80), QRectF(0, 0, 400, 20));
  bar.fill = Qt::red;
  bar.outline.style = Qt::NoPen;
  bar.anchorH = AnchorH::Span;
  bar.anchorV = AnchorV::Bottom;
  tpl::Element box = element(tpl::ItemType::Rect, QPointF(360, 10), QRectF(0, 0, 30, 30));
  box.fill = Qt::green;
  box.outline.style = Qt::NoPen;
  box.anchorH = AnchorH::Right;
  d.items << t << bar << box;
  d.layout = tpl::TemplateDocument::Stretch;
  const QImage narrow = tpl::overlay(d, transparent(400, 100), tpl::RenderOptions());
  const QImage stretched = tpl::overlay(d, transparent(800, 100), tpl::RenderOptions());
  d.layout = tpl::TemplateDocument::Anchored;
  const QImage wide = tpl::overlay(d, transparent(800, 100), tpl::RenderOptions());
  auto textInk = [](const QImage &im) {   // the white pixels only
    QImage w = im;
    for (int y = 0; y < w.height(); y++)
      for (int x = 0; x < w.width(); x++)
        if (qRed(w.pixel(x, y)) < 200 || qGreen(w.pixel(x, y)) < 200 || qBlue(w.pixel(x, y)) < 200) w.setPixel(x, y, 0);
    return ink(w, 200);
  };
  const QRect inkNarrow = textInk(narrow), inkStretched = textInk(stretched), inkWide = textInk(wide);
  check(!inkNarrow.isNull() && inkWide.width() == inkNarrow.width() && inkWide.height() == inkNarrow.height(), QString("the text on a wider picture keeps its shape (%1 x %2, was %3 x %4)").arg(inkWide.width()).arg(inkWide.height()).arg(inkNarrow.width()).arg(inkNarrow.height()));
  check(inkStretched.width() > inkNarrow.width() * 3 / 2, "while the stretched layout widens it");
  check(inkWide.left() == inkNarrow.left(), "and it stays at the left edge");
  check(qRed(wide.pixel(2, 95)) > 200 && qRed(wide.pixel(797, 95)) > 200 && qGreen(wide.pixel(400, 95)) < 50, "the bar spans the whole width, to both edges");
  check(qGreen(wide.pixel(785, 20)) > 200 && qAlpha(wide.pixel(385, 20)) == 0, "the square follows the right edge");
  // taller picture: Bottom follows
  const QImage tall = tpl::overlay(d, transparent(400, 200), tpl::RenderOptions());
  check(qRed(tall.pixel(200, 195)) > 200 && qAlpha(tall.pixel(200, 120)) == 0, "a taller picture: the bar stays at the bottom");
  // thumbnails are canvas shaped and unaffected
  const QImage thumb = tpl::renderThumbnail(d);
  check(thumb.size() == QSize(400, 100), "the thumbnail of an anchored template is the canvas");
  (void)gd;
}

static void testTextBox()
{
  printf("== text in a box\n");
  auto boxed = [](const QString &text, QRectF rect) {
    tpl::Element e = element(tpl::ItemType::Text, QPointF(0, 0), rect);
    e.text = text;
    e.textBox = true;
    e.font.family = "Montserrat";
    e.font.pointSize = 18;
    e.font.weight = 700;
    e.fill = Qt::white;
    e.outline.style = Qt::NoPen;
    return e;
  };
  auto render = [](const tpl::Element &e) {
    tpl::TemplateDocument d;
    d.canvas = QRectF(0, 0, 400, 300);
    d.items << e;
    return tpl::overlay(d, transparent(400, 300), tpl::RenderOptions());
  };
  const QRectF box(100, 60, 200, 120);
  tpl::Element e = boxed("Hello", box);
  QRect l = ink(render(e));
  e.align = tpl::TextAlign::Center;
  QRect c = ink(render(e));
  e.align = tpl::TextAlign::Right;
  QRect r = ink(render(e));
  check(!l.isNull() && l.left() >= 100 && l.left() <= 108, QString("left aligned: the text starts at the left edge of the box (x=%1)").arg(l.left()));
  check(std::abs(c.center().x() - 200) <= 3, QString("centred: the middle of the text is the middle of the box (x=%1)").arg(c.center().x()));
  check(r.right() <= 300 && r.right() >= 292, QString("right aligned: the text ends at the right edge (x=%1)").arg(r.right()));
  check(std::abs(l.width() - c.width()) <= 1 && std::abs(c.width() - r.width()) <= 1, "alignment moves the text, it does not change it");
  e.align = tpl::TextAlign::Left;
  const QRect top = ink(render(e));
  e.valign = tpl::TextVAlign::Middle;
  const QRect mid = ink(render(e));
  e.valign = tpl::TextVAlign::Bottom;
  const QRect bot = ink(render(e));
  check(top.top() >= 60 && top.top() <= 72, QString("top: the text starts at the top of the box (y=%1)").arg(top.top()));
  check(std::abs(mid.center().y() - 120) <= 10, QString("middle: centred in the box (y=%1)").arg(mid.center().y()));
  check(bot.bottom() <= 180 && bot.bottom() >= 168, QString("bottom: the text ends at the bottom of the box (y=%1)").arg(bot.bottom()));

  // a long text without shrink or wrap runs out of the box on one line
  tpl::Element longText = boxed("A fairly long line of text", box);
  const QRect plain = ink(render(longText));
  check(plain.width() > 200, QString("too long for the box: it overflows (%1 px in a 200 px box)").arg(plain.width()));
  longText.shrinkToFit = true;
  const QRect shrunk = ink(render(longText));
  check(shrunk.width() <= 200 && shrunk.width() >= 180, QString("shrink to fit: it fills the box width (%1 px)").arg(shrunk.width()));
  check(shrunk.height() < plain.height(), "by being smaller");
  // never larger than asked for
  tpl::Element shortText = boxed("Hi", QRectF(50, 50, 300, 200));
  const QRect shortPlain = ink(render(shortText));
  shortText.shrinkToFit = true;
  check(sameImage(render(shortText), render(boxed("Hi", QRectF(50, 50, 300, 200)))), "text that fits is drawn exactly as without shrink");
  (void)shortPlain;
  // monotonic in the box width
  int last = 0;
  bool grows = true;
  for (int w : {40, 80, 120, 160, 240})
    {
      tpl::Element s = longText;
      s.rect = QRectF(20, 60, w, 120);
      const int iw = ink(render(s)).width();
      grows &= iw >= last;
      last = iw;
    }
  check(grows, "a wider box never gives smaller text");
  // wrapping
  tpl::Element wrapped = boxed("aaa bbb ccc ddd eee fff ggg hhh", QRectF(100, 20, 150, 250));
  const QRect oneLine = ink(render(wrapped));
  wrapped.wrap = true;
  const QRect lines = ink(render(wrapped));
  check(oneLine.height() * 2 <= lines.height(), QString("wrap: several lines instead of one (%1 px high, was %2)").arg(lines.height()).arg(oneLine.height()));
  check(lines.width() <= 150, QString("none wider than the box (%1 px)").arg(lines.width()));
  tpl::Element word = boxed("Supercalifragilisticexpialidocious", QRectF(100, 20, 90, 250));
  word.wrap = true;
  const QRect wordInk = ink(render(word));
  check(wordInk.width() <= 90 && wordInk.height() > 40, QString("a word wider than the box is broken between letters (%1 px wide, %2 high)").arg(wordInk.width()).arg(wordInk.height()));
  tpl::Element wrapShrink = boxed("aaa bbb ccc ddd eee fff ggg hhh iii jjj kkk lll mmm", QRectF(100, 20, 100, 60));
  wrapShrink.wrap = true;
  wrapShrink.shrinkToFit = true;
  const QRect ws = ink(render(wrapShrink));
  check(ws.width() <= 100 && ws.bottom() <= 80 + 1, QString("wrap and shrink together fit the box (%1 x %2)").arg(ws.width()).arg(ws.height()));
  tpl::Element tiny = boxed("Cannot possibly fit", QRectF(100, 20, 6, 6));
  tiny.shrinkToFit = true;
  check(render(tiny).width() == 400, "a box too small for any text does not break the renderer");
  check(tpl::localBounds(boxed("x", box)) == box, "the extent of a boxed text is its box");
  // the natural box of a free text: turning it into a text box changes nothing that can be seen
  {
    tpl::Element free = element(tpl::ItemType::Text, QPointF(60, 120), QRectF(0, 0, 100, 100));
    free.text = "Hello, World\nsecond line";
    free.font.family = "Montserrat";
    free.font.pointSize = 22;
    free.font.weight = 700;
    free.fill = Qt::white;
    free.outline.style = Qt::NoPen;
    const QImage freeImg = render(free);
    tpl::Element asBox = free;
    asBox.textBox = true;
    asBox.rect = tpl::naturalTextBox(free);
    asBox.shrinkToFit = true;
    const QImage boxImg = render(asBox);
    const QRect fi = ink(freeImg), bi = ink(boxImg);
    check(fi.isValid() && std::abs(fi.left() - bi.left()) <= 1 && std::abs(fi.top() - bi.top()) <= 1 && std::abs(fi.width() - bi.width()) <= 1 && std::abs(fi.height() - bi.height()) <= 1,
          QString("a free text turned into a box of its natural size is drawn where and as big as before (%1,%2 %3x%4 vs %5,%6 %7x%8)").arg(fi.x()).arg(fi.y()).arg(fi.width()).arg(fi.height()).arg(bi.x()).arg(bi.y()).arg(bi.width()).arg(bi.height()));
    check(psnr(freeImg, boxImg) > 35, QString("with the same pixels (PSNR %1 dB)").arg(psnr(freeImg, boxImg), 0, 'f', 1));
    check(tpl::naturalTextBox(free).height() > tpl::textBounds(free).height() * 0.9 && tpl::naturalTextBox(free).width() >= tpl::textBounds(free).width() - 1,
          "the natural box is at least as large as the ink");
  }
  // the shrink factor is remembered: the same box gives the same picture again, another text or size does not reuse it
  {
    tpl::Element a = boxed("A fairly long line of text", QRectF(100, 60, 200, 60));
    a.shrinkToFit = true;
    const QImage first = render(a), again = render(a);
    check(sameImage(first, again), "drawing the same shrunk text again gives the same picture");
    tpl::Element b = a;
    b.text = "Short";
    tpl::Element c = a;
    c.rect = QRectF(100, 60, 100, 60);
    check(!sameImage(render(b), first) && !sameImage(render(c), first), "a different text or box is not given the remembered size");
  }
  // letters are not torn apart when a long word is broken: an accent stays with its letter, a surrogate pair is not split
  {
    const QString word = QString::fromUtf8("E\xcc\x81E\xcc\x81E\xcc\x81E\xcc\x81E\xcc\x81E\xcc\x81E\xcc\x81E\xcc\x81");   // E + combining acute, eight times
    tpl::Element dec = boxed(word, QRectF(100, 20, 70, 250));
    dec.wrap = true;
    const QStringList lines = tpl::textLines(dec);
    bool tidy = lines.size() > 1 && lines.join(QString()) == word;
    for (const QString &l : lines) tidy &= !l.isEmpty() && l.at(0).category() != QChar::Mark_NonSpacing;
    check(tidy, QString("a broken word keeps accents with their letters (%1 lines)").arg(lines.size()));
    const QString faces = QString::fromUtf8("\xf0\x9f\x98\x80\xf0\x9f\x98\x80\xf0\x9f\x98\x80\xf0\x9f\x98\x80\xf0\x9f\x98\x80\xf0\x9f\x98\x80");
    tpl::Element emoji = boxed(faces, QRectF(100, 20, 40, 250));
    emoji.wrap = true;
    const QStringList el = tpl::textLines(emoji);
    bool whole = el.join(QString()) == faces;
    for (const QString &l : el) whole &= l.isEmpty() || (!l.at(0).isLowSurrogate() && !l.at(l.size() - 1).isHighSurrogate());
    check(whole && render(emoji).width() == 400, QString("characters outside the basic plane are not cut in two (%1 lines)").arg(el.size()));
    tpl::Element plain = boxed("one two three four", QRectF(100, 20, 130, 250));
    plain.wrap = true;
    const QStringList pl = tpl::textLines(plain);
    check(pl.size() >= 2 && pl.join(QLatin1Char(' ')) == "one two three four", "lines of wrapped text are cut at the spaces");
    check(tpl::textLines(element(tpl::ItemType::Text, QPointF(), QRectF())).size() == 1 && tpl::textLines(plain, tpl::RenderOptions()).size() == pl.size(), "free text has the lines it is given");
  }
  // macros expand into the box
  tpl::Element macro = boxed("%c", QRectF(100, 60, 200, 60));
  macro.align = tpl::TextAlign::Center;
  tpl::TemplateDocument d;
  d.canvas = QRectF(0, 0, 400, 300);
  d.items << macro;
  tpl::RenderOptions o;
  o.vars.insert('c', "W1AW");
  const QRect m = ink(tpl::overlay(d, transparent(400, 300), o));
  check(std::abs(m.center().x() - 200) <= 3, "an expanded macro is centred in its box");
  // gradient over the box
  tpl::Element grad = boxed("MMMM", QRectF(100, 60, 200, 60));
  grad.gradient.kind = tpl::Gradient::Linear;
  grad.gradient.angle = 0;
  grad.gradient.stops << tpl::GradientStop{0, Qt::red} << tpl::GradientStop{1, Qt::blue};
  const QImage gi = render(grad);
  check(gi.width() == 400 && !ink(gi).isNull(), "a gradient text in a box renders");
}

static void testShadowAndCorners()
{
  printf("== shadow and rounded corners\n");
  auto rect = [](QRectF r, QColor fill) {
    tpl::Element e = element(tpl::ItemType::Rect, QPointF(0, 0), r);
    e.fill = fill;
    e.outline.style = Qt::NoPen;
    return e;
  };
  auto render = [](const tpl::Element &e, int w = 100, int h = 100) {
    tpl::TemplateDocument d;
    d.canvas = QRectF(0, 0, w, h);
    d.items << e;
    return tpl::overlay(d, transparent(w, h), tpl::RenderOptions());
  };
  tpl::Element e = rect(QRectF(10, 10, 20, 20), Qt::red);
  const QImage plain = render(e);
  e.shadow.on = true;
  e.shadow.color = QColor(0, 0, 0, 255);
  e.shadow.dx = 8;
  e.shadow.dy = 6;
  const QImage s = render(e);
  check(qAlpha(s.pixel(34, 30)) == 255 && qRed(s.pixel(34, 30)) == 0, "a hard shadow: black where the item's shape, moved by (8, 6), lies outside the item");
  check(qRed(s.pixel(15, 15)) == 255 && qGreen(s.pixel(15, 15)) == 0, "the item itself is on top of it");
  check(qAlpha(s.pixel(5, 5)) == 0 && qAlpha(s.pixel(60, 60)) == 0, "and nothing else is drawn");
  check(ink(s) == QRect(10, 10, 28, 26) && ink(plain) == QRect(10, 10, 20, 20), QString("the shadow adds exactly the offset to the extent (%1,%2 %3x%4)").arg(ink(s).x()).arg(ink(s).y()).arg(ink(s).width()).arg(ink(s).height()));
  // a shadow that is switched off or fully transparent changes nothing
  tpl::Element off = e;
  off.shadow.on = false;
  check(sameImage(render(off), plain), "shadow off: the same picture as without the fields");
  tpl::Element clear = e;
  clear.shadow.color = QColor(0, 0, 0, 0);
  check(sameImage(render(clear), plain), "a fully transparent shadow draws nothing");
  // the offset follows the canvas, not the turned item
  tpl::Element turned = rect(QRectF(0, 0, 40, 10), Qt::red);
  turned.pos = QPointF(30, 45);
  turned.rotation = 90;
  turned.shadow = e.shadow;
  turned.shadow.dx = 12;
  turned.shadow.dy = 0;
  turned.shadow.color = QColor(0, 0, 255, 255);
  const QImage ts = render(turned);
  QImage blueOnly = ts;
  for (int y = 0; y < blueOnly.height(); y++)
    for (int x = 0; x < blueOnly.width(); x++)
      if (qBlue(blueOnly.pixel(x, y)) < 200 || qRed(blueOnly.pixel(x, y)) > 50) blueOnly.setPixel(x, y, 0);
  const QRect shadowInk = ink(blueOnly, 200), redInk = [&]() {
    QImage r = ts;
    for (int y = 0; y < r.height(); y++)
      for (int x = 0; x < r.width(); x++)
        if (qRed(r.pixel(x, y)) < 200) r.setPixel(x, y, 0);
    return ink(r, 200);
  }();
  check(!shadowInk.isNull() && shadowInk.right() == redInk.right() + 12, QString("the shadow of a turned item is moved right by 12 canvas pixels (item %1, shadow %2)").arg(redInk.right()).arg(shadowInk.right()));
  // opacity applies to the shadow too
  tpl::Element half = e;
  half.opacity = 0.5;
  check(std::abs(qAlpha(render(half).pixel(34, 30)) - 128) <= 2, "item opacity halves the shadow as well");
  // soft shadow: spreads, keeps its weight, stays within the margin
  tpl::Element soft = e;
  soft.shadow.dx = 0;
  soft.shadow.dy = 0;
  soft.shadow.blur = 8;
  soft.fill = QColor(0, 0, 0, 0);   // only the shadow is visible
  tpl::Element hardRef = soft;
  hardRef.shadow.blur = 0;
  const QImage sh = render(soft, 140, 140), hd = render(hardRef, 140, 140);
  auto weight = [](const QImage &im) {
    double w = 0;
    for (int y = 0; y < im.height(); y++)
      for (int x = 0; x < im.width(); x++) w += qAlpha(im.pixel(x, y));
    return w;
  };
  check(std::abs(weight(sh) - weight(hd)) < weight(hd) * 0.03, QString("blurring keeps the total weight of the shadow (%1 vs %2)").arg(weight(sh), 0, 'f', 0).arg(weight(hd), 0, 'f', 0));
  check(qAlpha(sh.pixel(8, 20)) > 0 && qAlpha(sh.pixel(8, 20)) < 255, "it spreads beyond the edge of the item, softly");
  check(qAlpha(sh.pixel(20, 20)) > 180, "and is still strong in the middle");
  check(qAlpha(sh.pixel(60, 20)) == 0 && qAlpha(sh.pixel(20, 70)) == 0, "but ends within the blur radius's reach");
  check(ink(sh, 1).width() > ink(hd, 1).width() + 10, "the blurred shadow is wider than the hard one");
  // shadows of the other kinds
  for (tpl::ItemType t : {tpl::ItemType::Ellipse, tpl::ItemType::Line, tpl::ItemType::Image, tpl::ItemType::Replay, tpl::ItemType::Text})
    {
      tpl::Element k = element(t, QPointF(20, 30), QRectF(0, 0, 40, 20));
      if (t == tpl::ItemType::Text) { k.text = "Hi"; k.font.family = "Montserrat"; k.font.pointSize = 20; k.fill = Qt::white; }
      if (t == tpl::ItemType::Image) { QImage im(8, 8, QImage::Format_ARGB32); im.fill(Qt::yellow); k.image = im; }
      k.outline.color = Qt::white;
      k.outline.width = 3;
      const QImage without = render(k);
      k.shadow.on = true;
      k.shadow.color = QColor(0, 0, 0, 255);
      k.shadow.dx = 10;
      k.shadow.dy = 10;
      const QImage with = render(k);
      check(!sameImage(with, without) && ink(with).right() > ink(without).right(), QString("item kind %1 casts a shadow").arg(int(t)));
    }
  // a huge blur on a tiny item does not allocate absurd layers or crash
  tpl::Element big = rect(QRectF(0, 0, 10, 10), Qt::red);
  big.shadow.on = true;
  big.shadow.blur = 100;
  check(!render(big).isNull(), "the largest blur renders");
  tpl::Element off2 = rect(QRectF(5000, 5000, 20, 20), Qt::red);
  off2.shadow.on = true;
  off2.shadow.blur = 20;
  check(!render(off2).isNull() && ink(render(off2)).isNull(), "an item far outside the picture is skipped quickly");

  // rounded corners
  tpl::Element sq = rect(QRectF(10, 10, 80, 40), Qt::blue);
  const QImage square = render(sq);
  sq.radius = 12;
  const QImage round = render(sq);
  check(qAlpha(square.pixel(10, 10)) == 255 && qAlpha(round.pixel(10, 10)) == 0, "a radius cuts the corner off");
  check(qAlpha(round.pixel(50, 30)) == 255 && qAlpha(round.pixel(50, 10)) == 255, "and leaves the straight edges and the middle");
  sq.radius = 1000;
  const QImage pill = render(sq);
  check(qAlpha(pill.pixel(10, 10)) == 0 && qAlpha(pill.pixel(50, 30)) == 255 && qAlpha(pill.pixel(12, 30)) == 255, "a radius bigger than the item makes a pill (clamped to half the short side)");
  sq.radius = 0;
  check(sameImage(render(sq), square), "radius 0 is the plain rectangle, bit for bit");

  // the corner style of the outline: a 10 px white line centred on the edge of (20,20)-(80,80) reaches out to (15,15)
  tpl::Element framed = element(tpl::ItemType::Rect, QPointF(0, 0), QRectF(20, 20, 60, 60));
  framed.fill = Qt::blue;
  framed.outline.color = Qt::white;
  framed.outline.width = 10;
  auto joined = [&](tpl::Element e, Qt::PenJoinStyle js) {
    e.outline.join = js;
    return render(e);
  };
  const QImage mitre = joined(framed, Qt::MiterJoin);
  const QImage roundJ = joined(framed, Qt::RoundJoin);
  const QImage bevel = joined(framed, Qt::BevelJoin);
  check(qAlpha(mitre.pixel(15, 15)) == 255, "a rectangle with mitred corners is square out to the very corner");
  check(qAlpha(roundJ.pixel(15, 15)) == 0 && qAlpha(roundJ.pixel(50, 15)) == 255, "round corners cut the tip off a rectangle's outline and leave the edges");
  check(qAlpha(bevel.pixel(15, 15)) == 0 && qAlpha(bevel.pixel(50, 15)) == 255, "so do bevelled corners");
  check(!sameImage(roundJ, bevel) && !sameImage(roundJ, mitre) && !sameImage(bevel, mitre), "all three corner styles look different");

  // and the border of a picture and of the replay window, which has its own drawing code
  QImage pic(60, 60, QImage::Format_ARGB32);
  pic.fill(Qt::green);
  tpl::Element framedPic = element(tpl::ItemType::Image, QPointF(0, 0), QRectF(20, 20, 60, 60));
  framedPic.image = pic;
  framedPic.outline.color = Qt::white;
  framedPic.outline.width = 10;
  check(qAlpha(joined(framedPic, Qt::MiterJoin).pixel(20, 20)) == 255 && qAlpha(joined(framedPic, Qt::RoundJoin).pixel(20, 20)) == 0
        && qAlpha(joined(framedPic, Qt::BevelJoin).pixel(20, 20)) == 0, "the corner style applies to the border of a picture");
  tpl::Element window = framedPic;
  window.type = tpl::ItemType::Replay;
  window.image = QImage();
  check(qAlpha(joined(window, Qt::MiterJoin).pixel(20, 20)) == 255 && qAlpha(joined(window, Qt::RoundJoin).pixel(20, 20)) == 0,
        "and to the border of the replay window");
}

static void testMacros()
{
  printf("== macros\n");
  QHash<QChar, QString> v;
  v.insert('c', "W1AW");
  v.insert('m', "N0CALL");
  check(tpl::expandMacros("%c de %m", v) == "W1AW de N0CALL", "known macros");
  check(tpl::expandMacros("100%% %q|%", v) == "100% |", "escaped percent, unknown macro, trailing percent");
  check(tpl::expandMacros("no macros", v) == "no macros", "plain text untouched");

  // the macro table behind the editor's palette and the "unknown macro" warning
  QSet<QChar> seen;
  bool unique = true;
  for (const tpl::MacroInfo &m : tpl::macroTable())
    {
      if (seen.contains(m.tag)) unique = false;
      seen.insert(m.tag);
    }
  check(unique, "every macro tag is used once");
  check(seen.contains('a') && seen.contains('b'), "latitude (%a) and longitude (%b) are macros");
  QHash<QChar, QString> pos;
  pos.insert('a', "47.60620");
  pos.insert('b', "-122.33210");
  check(tpl::expandMacros("Lat %a Long %b", pos) == "Lat 47.60620 Long -122.33210", "position macros expand");
  check(tpl::expandMacros("[%a]", QHash<QChar, QString>()) == "[]", "an empty position prints nothing");
  check(tpl::unknownMacros("%a %b %m %%").isEmpty(), "position macros are not reported as unknown");
  check(tpl::unknownMacros("%k") == "k", "a tag that is not in the table is reported");
  check(tpl::sampleVars().value('a') == "47.60620" && tpl::sampleVars().value('b') == "-122.33210", "sample position values");
  check(seen.contains('g'), "the place (%g) is a macro");
  QHash<QChar, QString> place;
  place.insert('g', "Seattle, WA");
  check(tpl::expandMacros("QTH %g!", place) == "QTH Seattle, WA!", "the place macro expands");
  check(tpl::expandMacros("[%g]", QHash<QChar, QString>()) == "[]", "an empty place prints nothing");
  check(tpl::unknownMacros("%g %a").isEmpty() && tpl::sampleVars().value('g') == "Seattle, WA", "the place macro is known and has a sample");
}

static void testFuzz(const QString &fx)
{
  printf("== legacy loader fuzz (damaged files must be rejected or loaded, never crash or hang)\n");
  QFile f(fx + "/synthetic.templ");
  if (!f.open(QIODevice::ReadOnly)) { check(false, "synthetic.templ readable"); return; }
  const QByteArray orig = f.readAll();
  QElapsedTimer total;
  total.start();
  int ok = 0, rejected = 0;
  qint64 worst = 0;
  for (int n = 0; n < orig.size(); n++)
    {
      QElapsedTimer t; t.start();
      (tpl::loadLegacy(orig.left(n)).ok ? ok : rejected)++;
      worst = qMax(worst, t.elapsed());
    }
  // the format has no item count or footer, so a file cut exactly between two items is a valid shorter template
  check(ok <= 16, QString("every truncation handled: %1 rejected, %2 accepted (only cuts at an item boundary may load)").arg(rejected).arg(ok));
  quint32 r = 12345;
  auto rnd = [&]() { r = r * 1664525 + 1013904223; return r >> 8; };
  const int rounds = qEnvironmentVariableIntValue("TPL_FUZZ_ROUNDS") ? qEnvironmentVariableIntValue("TPL_FUZZ_ROUNDS") : 6000;
  ok = rejected = 0;
  for (int it = 0; it < rounds; it++)
    {
      QByteArray m = orig;
      const int flips = 1 + rnd() % 6;
      for (int k = 0; k < flips; k++) m[rnd() % m.size()] = char(rnd());
      if (it % 7 == 0) m.insert(rnd() % m.size(), QByteArray(1 + rnd() % 9, char(rnd())));
      if (it % 11 == 0) m.remove(rnd() % m.size(), 1 + rnd() % 16);
      QElapsedTimer t; t.start();
      tpl::LoadResult lr = tpl::loadLegacy(m);
      (lr.ok ? ok : rejected)++;
      if (lr.ok)
        {
          QImage bg(64, 48, QImage::Format_ARGB32);
          bg.fill(Qt::gray);
          tpl::overlay(lr.doc, bg, options(QImage()));          // whatever loaded must also render
        }
      worst = qMax(worst, t.elapsed());
    }
  check(worst < 2000, QString("%1 byte-level mutations: %2 loaded, %3 rejected, slowest %4 ms, total %5 s, no crash")
        .arg(rounds).arg(ok).arg(rejected).arg(worst).arg(total.elapsed() / 1000.0, 0, 'f', 1));
}

static void quietHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
  if (msg.contains("libpng") || msg.contains("QImage") || msg.contains("allocation")) return;   // damaged embedded images, expected while fuzzing
  fprintf(stderr, "%s\n", qPrintable(msg));
  if (type == QtFatalMsg) abort();
}

int main(int argc, char **argv)
{
  qInstallMessageHandler(quietHandler);
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QGuiApplication app(argc, argv);
  if (argc < 3) { fprintf(stderr, "usage: test <fixtures-dir> <golden-dir>\n"); return 2; }
  const QString fx = argv[1], gd = argv[2];
  QFontDatabase::addApplicationFont(fx + "/Montserrat-Bold.otf");
  testGoldenParity(fx, gd);
  testJsonRoundTrip(fx, gd);
  testValidation();
  testNewFieldsFormat();
  testAutoAnchor();
  testAnchoredLayout(fx, gd);
  testTextBox();
  testShadowAndCorners();
  testMacros();
  testFuzz(fx);
  printf("\n%d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
