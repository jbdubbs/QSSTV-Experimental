// Screenshots of the Qt Quick interface (src/qml), offscreen, with the real controllers and the real (hidden) main window behind them:
//   shot <scratch-dir> <out-dir> <width> <height> <scene>...
// scenes: idle (listening, nothing yet) receiving (a picture half drawn, Martin 1) done (the picture complete) options (the Receive options
// sheet open) digital (the DRM mode) stopped (receiver stopped) quiet (the level too low) loud tx gallery (placeholders).
// Each scene is saved as <out-dir>/qml-<w>x<h>-<scene>.png. Built and run by tests/qmlui/app.sh (links the objects of the build-qml tree).
// Environment: QSSTV_QML_THEME=light|dark, QSSTV_QML_FONTSCALE=1.3 are read by the interface itself.
#include <QtTest>
#include <QtWidgets>
#include <QtQuick>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include "appglobal.h"
#include "configparams.h"
#include "dispatcher.h"
#include "mainwindow.h"
#include "uimode.h"
#include "qmlui.h"
#include "appmodel.h"
#include "rxcontroller.h"
#include "txcontroller.h"
#include "txwidget.h"
#include "imageviewer.h"
#include "rxwidget.h"
#include "spectrumwidget.h"
#include "gallerymodel.h"
#include "settingscontroller.h"
#include "directoriesconfig.h"

static QQuickWindow *theWindow()
{
  for (QWindow *w : QGuiApplication::allWindows())
    if (QQuickWindow *q = qobject_cast<QQuickWindow *>(w)) return q;
  return nullptr;
}

// an item by objectName, through the visual tree (the QObject tree of QML items is not the one that holds them)
static QQuickItem *findItem(QQuickItem *root, const QString &name)
{
  if (root->objectName() == name) return root;
  for (QQuickItem *c : root->childItems())
    if (QQuickItem *f = findItem(c, name)) return f;
  return nullptr;
}

static void feedWaterfall(int frames)
{
  // what the receiver sends the spectrum display: tones of an SSTV line and some noise
  std::vector<double> samples(8192);
  for (int frame = 0; frame < frames; frame++)
    {
      for (size_t i = 0; i < samples.size(); i++)
        {
          const double f = 1500.0 + 700.0 * sin(frame / 7.0);   // a tone that wanders between 800 and 2200 Hz
          samples[i] = 3000 * sin(2 * M_PI * f * i / 12000.0) + 1200 * sin(2 * M_PI * 1200.0 * i / 12000.0) + (rand() % 2001 - 1000);
        }
      mainWindowPtr->spectrumFramePtr->realFFT(samples.data());
    }
}

static QImage testPicture(int w, int h, int lines)
{
  QImage im(w, h, QImage::Format_ARGB32_Premultiplied);
  im.fill(QColor(16, 20, 28));
  QPainter p(&im);
  QLinearGradient g(0, 0, 0, h * 0.62);
  g.setColorAt(0, QColor("#6ec1ff"));
  g.setColorAt(1, QColor("#d9f0ff"));
  p.fillRect(QRectF(0, 0, w, h * 0.7), g);
  p.setBrush(QColor("#fff6c8"));
  p.setPen(Qt::NoPen);
  p.drawEllipse(QPointF(w * 0.3, h * 0.25), h * 0.1, h * 0.1);
  p.setBrush(QColor("#2f7d3a"));
  QPainterPath hill;
  hill.moveTo(0, h * 0.72);
  hill.cubicTo(w * 0.25, h * 0.5, w * 0.5, h * 0.7, w, h * 0.55);
  hill.lineTo(w, h);
  hill.lineTo(0, h);
  p.drawPath(hill);
  p.end();
  // only the lines received so far are there
  for (int y = lines; y < h; y++) memset(im.scanLine(y), 0, size_t(im.bytesPerLine()));
  return im;
}

// pictures in the gallery folders, of today, yesterday, last week and last year, so that the grid has its day headings
static void makeGallery(const QString &scratch)
{
  rxSSTVImagesPath = scratch + "/g/rx_sstv/";
  rxDRMImagesPath = scratch + "/g/rx_drm/";
  txSSTVImagesPath = scratch + "/g/tx_sstv/";
  txDRMImagesPath = scratch + "/g/tx_drm/";
  txStockImagesPath = scratch + "/g/tx_stock/";
  templatesPath = scratch + "/g/templates/";
  for (const QString &d : {rxSSTVImagesPath, rxDRMImagesPath, txSSTVImagesPath, txDRMImagesPath, txStockImagesPath, templatesPath}) QDir().mkpath(d);
  struct F {QString dir, name; int hoursAgo; int lines;};
  const std::vector<F> files = {
    {rxSSTVImagesPath, "m1", 1, 256}, {rxSSTVImagesPath, "s1", 3, 240}, {rxSSTVImagesPath, "pd120", 5, 256}, {rxSSTVImagesPath, "r36", 7, 200},
    {rxSSTVImagesPath, "m1", 27, 256}, {rxSSTVImagesPath, "pd90", 30, 256}, {rxSSTVImagesPath, "s2", 24 * 5, 256}, {rxSSTVImagesPath, "m2", 24 * 5 + 2, 128},
    {rxSSTVImagesPath, "r72", 24 * 40, 256}, {rxDRMImagesPath, "drm", 2, 256}, {rxDRMImagesPath, "drm", 28, 256},
    {txSSTVImagesPath, "tx", 4, 256}, {txSSTVImagesPath, "tx", 26, 256}, {txDRMImagesPath, "tx", 50, 256},
    {txStockImagesPath, "stock-a", 24 * 300, 256}, {txStockImagesPath, "stock-b", 24 * 300, 256}};
  int n = 0;
  for (const F &f : files)
    {
      const QDateTime when = QDateTime::currentDateTimeUtc().addSecs(-3600 * f.hoursAgo);
      const QString file = QString("%1%2_%3_%4.png").arg(f.dir, f.name, when.toString("yyyyMMdd_HHmmss")).arg(n++);
      QImage im = testPicture(320, 256, f.lines);
      QPainter p(&im);
      p.setPen(Qt::white);
      p.setFont(QFont("sans", 40, QFont::Bold));
      p.drawText(im.rect(), Qt::AlignCenter, QString::number(n));
      p.end();
      im.save(file);
      QFile fl(file);
      if (fl.open(QIODevice::ReadWrite)) {fl.setFileTime(when, QFileDevice::FileModificationTime); fl.close();}
    }
  QFile::copy(QStringLiteral(SRC_TEMPLATE_FIXTURE), templatesPath + "synthetic.templ");
}

int main(int argc, char **argv)
{
  setvbuf(stdout, nullptr, _IOLBF, 0);
  if (argc < 6) {fprintf(stderr, "usage: shot scratch outdir width height scene...\n"); return 2;}
  const QString scratch = argv[1], outDir = argv[2];
  const int width = atoi(argv[3]), height = atoi(argv[4]);
  qputenv("HOME", scratch.toLocal8Bit());
  qputenv("XDG_CONFIG_HOME", (scratch + "/config").toLocal8Bit());
  qputenv("XDG_DATA_HOME", (scratch + "/data").toLocal8Bit());
  qputenv("QT_QPA_PLATFORM", "offscreen");
  qputenv("QT_QUICK_BACKEND", "software");
  qputenv("PIPEWIRE_REMOTE", "qsstv-test-no-such-server");
  qputenv("PULSE_SERVER", "unix:/nonexistent/qsstv-test");
  qputenv("QSSTV_UI", "qml");
  QCoreApplication::setOrganizationName("qsstv-qmlui");
  QCoreApplication::setApplicationName("qsstv-qmlui");
  QApplication app(argc, argv);
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, scratch + "/settings");
  {
    QSettings qs;
    qs.setValue("update/asked", true);
    qs.setValue("update/autoCheck", false);
    qs.setValue("appimage/integrationAsked", true);
  }
  QDir().mkpath(outDir);
  globalInit();
  mainWindowPtr = new mainWindow;
  QTest::qWait(300);
  mainWindowPtr->init();
  mainWindowPtr->startRunning(false);   // no sound card receiver
  makeGallery(scratch);   // after the settings were read
  qmlUi::show();
  QTest::qWait(500);
  QQuickWindow *win = theWindow();
  if (!win) {fprintf(stderr, "FAIL: the Qt Quick window did not come up\n"); return 1;}
  win->resize(width, height);
  QTest::qWait(300);
  int exitCode = 0;
  for (int i = 5; i < argc; i++)
    {
      const QString scene = argv[i];
      AppModel *app_ = AppModel::instance();
      RxController *rx = RxController::instance();
      // nothing is left open from the scene before
      win->setProperty("aboutOpen", false);
      for (const char *screenName : {"receiveScreen", "transmitScreen"})
        if (QQuickItem *s = findItem(win->contentItem(), screenName))
          for (const char *prop : {"optionsOpen", "modesOpen", "detailsOpen", "digitalOpen", "toolsOpen", "open"})
            if (s->property(prop).isValid()) s->setProperty(prop, false);
      TxController::instance()->setTestSending(-1);
      if (QQuickItem *v = findItem(win->contentItem(), "viewerScreen")) v->setProperty("infoOpen", false);
      GalleryModel::instance()->closeViewer();
      AppModel::instance()->setSettingsOpen(false);
      GalleryModel::instance()->clearSelection();
      app_->setScreen(0);
      app_->setDigital(scene == "digital" || scene.startsWith("tx-digital"));
      // the receiver does not run here: tell the screens what a running one would
      rx->setTestListening(scene == "stopped" ? 0 : 1);
      rx->setTestLevel(scene == "quiet" ? 0.1 : scene == "loud" ? 0.97 : 0.55);
      feedWaterfall(60);
      if (scene == "receiving" || scene == "done" || scene == "options")
        {
          const int w = 320, h = 256;
          rxWidgetPtr->getImageViewerPtr()->createImage(QSize(w, h), QColor(16, 20, 28), true);
          const int lines = scene == "receiving" ? 118 : h;
          *rxWidgetPtr->getImageViewerPtr()->getDisplayedImage() = testPicture(w, h, lines);
          qmlUi::rxImageStarted(w, h);
          qmlUi::rxStatus(false, "Receiving Martin 1");
          qmlUi::rxLine(lines);
          qmlUi::rxSync(20);
          if (scene == "done") qmlUi::rxStatus(false, "Saved");
        }
      else if (scene.startsWith("tx"))
        {
          app_->setScreen(1);
          TxController *tx = TxController::instance();
          if (scene != "tx")   // tx-picture, tx-modes, tx-sending, tx-details: a picture is loaded
            {
              const QString picFile = scratch + "/tx-test.png";
              testPicture(320, 256, 256).save(picFile);
              txWidgetPtr->setImage(picFile);
              QTest::qWait(300);
            }
          if (scene == "tx-sending") tx->setTestSending(0.38);
          if (scene == "tx-sending-tool") tx->setTestSending(0.4, "Test tone, 1000 Hz", 5);
        }
      else if (scene.startsWith("settings"))
        {
          app_->setScreen(0);
          AppModel::instance()->setSettingsOpen(true);
          QQuickItem *st = findItem(win->contentItem(), "settingsScreen");
          const int page = scene == "settings-operator" ? 1 : scene == "settings-pictures" || scene == "settings-format" ? 2 : scene == "settings-position" ? 3 : scene == "settings-advanced" ? 4 : 0;
          if (st) st->setProperty("page", page);
          if (scene == "settings-format" && st) QMetaObject::invokeMethod(st, "openFormat");
        }
      else if (scene == "saved")
        {
          const QDir dir(rxSSTVImagesPath);
          qmlUi::rxImageSaved(dir.entryInfoList(QDir::Files).first().absoluteFilePath());   // "Saved to Gallery" with View
        }
      else if (scene.startsWith("gallery"))
        {
          app_->setScreen(2);
          GalleryModel *g = GalleryModel::instance();
          g->clearSelection();
          g->closeViewer();
          g->setTab(scene == "gallery-sent" ? 1 : scene == "gallery-templates" ? 2 : 0);
          g->rescan();
          if (scene == "gallery-empty") {g->setTab(1); QDir(txSSTVImagesPath).removeRecursively(); QDir(txDRMImagesPath).removeRecursively(); g->rescan();}
          QTest::qWait(600);
          if (scene == "gallery-select") {g->startSelecting(1); g->toggle(3);}
          if (scene == "gallery-viewer" || scene == "gallery-info") g->setViewerIndex(1);
          if (scene == "gallery-delete") {QTest::qWait(500); g->setViewerIndex(1); QTest::qWait(700); g->deleteIndex(1); QTest::qWait(900);}
          if (scene == "gallery-info") {QTest::qWait(500); if (QQuickItem *v = findItem(win->contentItem(), "viewerScreen")) v->setProperty("infoOpen", true);}
        }
      QTest::qWait(400);
      QQuickItem *content = win->contentItem();
      if (scene == "about") {win->setProperty("aboutOpen", true); QTest::qWait(500);}
      if (scene == "tx-tools")
        {
          QQuickItem *t = findItem(content, "transmitScreen");
          if (t) t->setProperty("toolsOpen", true);
          QTest::qWait(500);
        }
      if (scene == "tx-digital-details")
        {
          QQuickItem *t = findItem(content, "transmitScreen");
          if (t) t->setProperty("digitalOpen", true);
          QTest::qWait(500);
        }
      if (scene == "tx-modes" || scene == "tx-details")
        {
          QQuickItem *t = findItem(content, "transmitScreen");
          if (!t) fprintf(stderr, "no transmitScreen item\n");
          else if (!t->setProperty(scene == "tx-modes" ? "modesOpen" : "detailsOpen", true)) fprintf(stderr, "cannot set the sheet property\n");
          QTest::qWait(500);
        }
      if (scene == "options")
        {
          QQuickItem *r = findItem(content, "receiveScreen");
          if (r) r->setProperty("optionsOpen", true);
          QTest::qWait(500);
        }
      QImage img = win->grabWindow();
      const QString file = QString("%1/qml-%2x%3-%4.png").arg(outDir).arg(width).arg(height).arg(scene);
      if (img.isNull() || !img.save(file)) {fprintf(stderr, "FAIL: no picture for %s\n", qPrintable(scene)); exitCode = 1;}
      else printf("  ok    %s (%dx%d)\n", qPrintable(file), img.width(), img.height());
    }
  fflush(stdout);
  _exit(exitCode);   // no shutdown: the hidden pages and threads are not worth unwinding here
}
