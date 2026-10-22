// Tests for src/template: legacy import, JSON round trip, validation and the renderer's parity with the old editor.
//   test <fixtures-dir> <golden-dir>
// Built and run by tests/template/run.sh (standalone g++ build, no application globals needed).
#include <QtGui>
#include <cmath>
#include "templatedoc.h"
#include "templaterender.h"

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

static void testMacros()
{
  printf("== macros\n");
  QHash<QChar, QString> v;
  v.insert('c', "W1AW");
  v.insert('m', "N0CALL");
  check(tpl::expandMacros("%c de %m", v) == "W1AW de N0CALL", "known macros");
  check(tpl::expandMacros("100%% %q|%", v) == "100% |", "escaped percent, unknown macro, trailing percent");
  check(tpl::expandMacros("no macros", v) == "no macros", "plain text untouched");
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
  testMacros();
  testFuzz(fx);
  printf("\n%d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
