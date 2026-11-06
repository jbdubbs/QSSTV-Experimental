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
#include "imageviewer.h"
#include "rxwidget.h"
#include "spectrumwidget.h"

static QQuickWindow *theWindow()
{
  for (QWindow *w : QGuiApplication::allWindows())
    if (QQuickWindow *q = qobject_cast<QQuickWindow *>(w)) return q;
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
      app_->setScreen(0);
      app_->setDigital(scene == "digital");
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
      else if (scene == "tx") app_->setScreen(1);
      else if (scene == "gallery") app_->setScreen(2);
      QTest::qWait(400);
      QQuickItem *content = win->contentItem();
      if (scene == "options")
        {
          QQuickItem *r = content->findChild<QQuickItem *>("receiveScreen");
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
