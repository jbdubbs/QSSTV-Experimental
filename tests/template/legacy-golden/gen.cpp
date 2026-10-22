// Generates the golden references for tests/template from the LEGACY editor classes (src/editor, as of the commit
// before the template rebuild). It is kept for history and for regenerating the fixtures against a pre-cutover
// checkout; it does not build once the legacy editor is deleted (Phase 2 of the template-editor rebuild).
//
//   gen <out-fixtures-dir> <out-golden-dir>  [extra.templ ...]
//
// Writes fixtures/synthetic.templ (legacy binary format, every item type), fixtures/logo.png, fixtures/replay.png,
// golden/base_WxH.png (the frame the template is laid over) and golden/<name>_WxH_{replay,noreplay}.png rendered through
// the old editorScene::load + addConversion + overlay path, plus golden/<name>_thumb.png (the old renderImage(0,0)).
// Extra .templ arguments are rendered the same way into golden/<basename>_... (used for ad-hoc parity checks).
#include <QtWidgets>
#include "appglobal.h"
#include "logging.h"
#include "editor.h"
#define private public
#include "editorscene.h"
#undef private
#include "basegraphicitem.h"
#include "graphicitems.h"

static QImage makeBase(int w, int h)
{
  QImage im(w, h, QImage::Format_ARGB32);
  for (int y = 0; y < h; y++)
    {
      QRgb *l = reinterpret_cast<QRgb *>(im.scanLine(y));
      for (int x = 0; x < w; x++)
        {
          int r = 40 + 120 * x / w, g = 60 + 110 * y / h, b = 150 - 60 * x / w;
          if (((x / 16) + (y / 16)) % 2 == 0) { r += 12; g += 12; b += 12; }
          l[x] = qRgb(r, g, b);
        }
    }
  return im;
}

static QImage makeLogo()
{
  QImage im(64, 64, QImage::Format_ARGB32);
  im.fill(Qt::transparent);
  QPainter p(&im);
  p.setRenderHint(QPainter::Antialiasing);
  p.setBrush(QColor(250, 210, 0));
  p.setPen(QPen(Qt::black, 3));
  p.drawEllipse(QRectF(4, 4, 56, 56));
  p.setPen(QPen(QColor(200, 0, 0), 5));
  p.drawLine(18, 40, 32, 22);
  p.drawLine(32, 22, 46, 40);
  return im;
}

static QImage makeReplay()
{
  QImage im(160, 120, QImage::Format_RGB32);
  for (int y = 0; y < im.height(); y++)
    for (int x = 0; x < im.width(); x++)
      im.setPixel(x, y, qRgb(x * 255 / im.width(), 255 - y * 255 / im.height(), ((x / 8) ^ (y / 8)) & 1 ? 200 : 60));
  return im;
}

static sgradientParam grad(sgradientParam::gType t, int dir, QColor c1, int p1, QColor c2, int p2, QColor c3 = Qt::black, int p3 = 0, QColor c4 = Qt::black, int p4 = 0)
{
  sgradientParam g;
  g.type = t; g.direction = dir;
  g.color1 = c1; g.pos1 = p1; g.color2 = c2; g.pos2 = p2; g.color3 = c3; g.pos3 = p3; g.color4 = c4; g.pos4 = p4;
  return g;
}

static QPen pen(QColor c, int w, Qt::PenJoinStyle j = Qt::BevelJoin, Qt::PenCapStyle cap = Qt::SquareCap)
{
  QPen p(c);
  p.setWidth(w); p.setJoinStyle(j); p.setCapStyle(cap);
  return p;
}

static QFont font(const QString &fam, int pt, bool bold = false, bool italic = false, bool underline = false)
{
  QFont f(fam, pt);
  f.setBold(bold); f.setItalic(italic); f.setUnderline(underline);
  return f;
}

// sitemParam leaves rotation/shear/gradient uninitialised ("used dynamically, no need to setup"); the real editor always
// sets them in editorScene::itemSetup(), so do the same here to get deterministic files
template <class T> static T *mk()
{
  T *it = new T(nullptr);
  it->setTransform(0, 0, 0);
  it->setGradient(grad(sgradientParam::NONE, 0, Qt::black, 0, Qt::black, 0));
  return it;
}

template <class T> static T *add(editorScene &sc, T *it, QPointF pos, qreal z)
{
  it->setPos(pos);
  it->setZValue(z);
  sc.addItem(it);
  return it;
}

static void buildSynthetic(const QString &path, const QString &fam)
{
  editorScene sc(nullptr);
  sc.addBorder(640, 496);

  // gradient bar across the top (the classic colour bar)
  auto *bar = mk<itemRectangle>();
  bar->setRect(QRectF(0, 0, 640, 48)); bar->setPen(pen(Qt::black, 2)); bar->setBrush(QColor(200, 0, 0));
  bar->setGradient(grad(sgradientParam::LINEAR, 0, QColor(220, 0, 0), 0, QColor(0, 200, 0), 100));
  add(sc, bar, QPointF(0, 0), 2);

  // translucent ellipse with a radial gradient, rotated
  auto *el = mk<itemEllipse>();
  el->setRect(QRectF(0, 0, 160, 90)); el->setPen(pen(QColor(0, 0, 200), 3)); el->setBrush(QColor(255, 240, 0, 180));
  el->setGradient(grad(sgradientParam::RADIAL, 0, QColor(255, 255, 255, 230), 0, QColor(255, 120, 0, 200), 70, QColor(120, 0, 120, 160), 100));
  el->setTransform(30, 0, 0);
  add(sc, el, QPointF(30, 90), 3);

  // plain ellipse and rectangle, solid fills, one rotated
  auto *el2 = mk<itemEllipse>();
  el2->setRect(QRectF(0, 0, 70, 70)); el2->setPen(pen(Qt::white, 1)); el2->setBrush(QColor(0, 120, 255, 255));
  add(sc, el2, QPointF(220, 100), 4);
  auto *rc = mk<itemRectangle>();
  rc->setRect(QRectF(0, 0, 120, 60)); rc->setPen(pen(QColor(255, 255, 255), 4, Qt::RoundJoin)); rc->setBrush(QColor(0, 0, 0, 120));
  rc->setTransform(-15, 0, 0);
  add(sc, rc, QPointF(280, 120), 5);

  // conical gradient rectangle with shear
  auto *cn = mk<itemRectangle>();
  cn->setRect(QRectF(0, 0, 90, 90)); cn->setPen(pen(Qt::black, 1)); cn->setBrush(QColor(0, 0, 0));
  cn->setGradient(grad(sgradientParam::CONICAL, 45, QColor(255, 0, 0), 0, QColor(255, 255, 0), 35, QColor(0, 255, 0), 70, QColor(0, 0, 255), 100));
  cn->setTransform(0, 0.25, 0.1);
  add(sc, cn, QPointF(420, 80), 6);

  // lines
  auto *ln = mk<itemLine>();
  ln->setRect(QRectF(0, 0, 200, 2)); ln->setPen(pen(Qt::white, 5, Qt::BevelJoin, Qt::RoundCap));
  add(sc, ln, QPointF(30, 220), 7);
  auto *ln2 = mk<itemLine>();
  ln2->setRect(QRectF(0, 0, 150, 2)); ln2->setPen(pen(QColor(255, 200, 0), 3));
  ln2->setTransform(20, 0, 0);
  add(sc, ln2, QPointF(260, 240), 8);

  // text: macros, single line, outlined, bold
  auto *t1 = mk<itemText>();
  t1->setFont(font(fam, 24, true)); t1->setText("%c de %m  %r"); t1->setBrush(QColor(255, 255, 255)); t1->setPen(pen(Qt::black, 3, Qt::RoundJoin));
  add(sc, t1, QPointF(12, 300), 9);

  // multi line, gradient fill, italic + underline, unknown macro and escaped percent
  auto *t2 = mk<itemText>();
  t2->setFont(font(fam, 22, false, true, true)); t2->setText("TU 73!\n%d %t\n100%% fun %q|"); t2->setBrush(QColor(255, 255, 255)); t2->setPen(pen(Qt::black, 1));
  t2->setGradient(grad(sgradientParam::LINEAR, 270, QColor(255, 255, 0), 0, QColor(255, 80, 0), 100));
  add(sc, t2, QPointF(12, 350), 10);

  // rotated + sheared text
  auto *t3 = mk<itemText>();
  t3->setFont(font(fam, 26, true)); t3->setText("tilted"); t3->setBrush(QColor(0, 255, 200)); t3->setPen(pen(Qt::black, 2));
  t3->setRect(QRectF(0, 0, 120, 40)); t3->setTransform(-20, 0.2, 0);
  add(sc, t3, QPointF(430, 440), 11);

  // image with border, and a rotated one half covered by the replay slot
  auto *im = mk<itemImage>();
  im->setImage(makeLogo()); im->setRect(QRectF(0, 0, 128, 128)); im->setPen(pen(QColor(255, 255, 255), 3)); im->setBrush(QColor(0, 0, 0));
  add(sc, im, QPointF(500, 70), 12);
  auto *im2 = mk<itemImage>();
  im2->setImage(makeLogo()); im2->setRect(QRectF(0, 0, 80, 50)); im2->setPen(pen(QColor(255, 255, 0), 0)); im2->setBrush(QColor(0, 0, 0));
  im2->setTransform(12, 0, 0);
  add(sc, im2, QPointF(560, 200), 13);

  auto *rp = mk<itemReplayImage>();
  rp->setRect(QRectF(0, 0, 220, 165)); rp->setPen(pen(QColor(255, 255, 255), 2)); rp->setBrush(QColor(0, 0, 0));
  add(sc, rp, QPointF(380, 260), 14);

  // z-order test: a bar drawn UNDER the replay slot
  auto *bar2 = mk<itemRectangle>();
  bar2->setRect(QRectF(0, 0, 640, 30)); bar2->setPen(pen(Qt::white, 1)); bar2->setBrush(QColor(0, 0, 0, 200));
  add(sc, bar2, QPointF(0, 466), 1.5);

  QFile f(path);
  sc.save(f, true);
}

// what applyTemplate() does, minus the replay lookup through the txWidget global
static QImage legacyRender(const QString &templ, const QImage &base, const QImage &replay)
{
  QFile f(templ);
  editorScene s(nullptr);
  if (!s.load(f)) { fprintf(stderr, "cannot load %s\n", qPrintable(templ)); return QImage(); }
  s.addConversion('c', "W1AW", true);
  s.addConversion('r', "599");
  s.addConversion('m', "N0CALL");
  s.addConversion('t', "12:34");
  s.addConversion('d', "2026/10/22");
  s.clearSelection();
  s.setSceneRect(s.border);
  s.localImage = new QImage(base.copy());
  s.convertText();
  if (!replay.isNull())
    for (QGraphicsItem *t : s.items())
      if (t->type() == graphItemBase::REPLAY) static_cast<itemReplayImage *>(t)->setImage(replay);
  QPainter painter(s.localImage);
  painter.setRenderHint(QPainter::Antialiasing);
  s.render(&painter, QRectF(), QRectF(), Qt::IgnoreAspectRatio);
  painter.end();
  return *s.localImage;
}

int main(int argc, char **argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QCoreApplication::setOrganizationName("gen");
  QCoreApplication::setApplicationName("gen");
  QApplication app(argc, argv);
  if (argc < 3) { fprintf(stderr, "usage: gen <fixtures-dir> <golden-dir> [extra.templ ...]\n"); return 2; }
  QString fx = argv[1], gd = argv[2];
  QDir().mkpath(fx); QDir().mkpath(gd);
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QTemporaryDir settingsDir;   // the legacy gradient dialog persists its state; keep it out of the repo
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
  logFilePtr = new logFile();
  int id = QFontDatabase::addApplicationFont(fx + "/Montserrat-Bold.otf");
  QString fam = id >= 0 ? QFontDatabase::applicationFontFamilies(id).value(0) : "Sans Serif";
  fprintf(stderr, "font family: %s  dpi=%.1f\n", qPrintable(fam), QGuiApplication::primaryScreen()->logicalDotsPerInch());

  makeLogo().save(fx + "/logo.png");
  makeReplay().save(fx + "/replay.png");
  buildSynthetic(fx + "/synthetic.templ", fam);

  QStringList templs; templs << fx + "/synthetic.templ";
  for (int i = 3; i < argc; i++) templs << argv[i];
  QImage replay = makeReplay();
  for (const QString &t : templs)
    {
      QString name = QFileInfo(t).completeBaseName();
      for (QSize sz : {QSize(320, 256), QSize(640, 496), QSize(1024, 768)})
        {
          QImage base = makeBase(sz.width(), sz.height());
          if (name == "synthetic") base.save(QString("%1/base_%2x%3.png").arg(gd).arg(sz.width()).arg(sz.height()));
          legacyRender(t, base, replay).save(QString("%1/%2_%3x%4_replay.png").arg(gd, name).arg(sz.width()).arg(sz.height()));
          legacyRender(t, base, QImage()).save(QString("%1/%2_%3x%4_noreplay.png").arg(gd, name).arg(sz.width()).arg(sz.height()));
        }
      QFile f(t);
      editorScene s(nullptr);
      if (s.load(f)) s.renderImage(0, 0)->save(QString("%1/%2_thumb.png").arg(gd, name));
    }
  return 0;
}
