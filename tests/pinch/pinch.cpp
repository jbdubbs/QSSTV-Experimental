// Two fingers on the picture viewer of the touch shell: does a pinch zoom it in and out, smoothly, and does a double tap zoom?
//   pinch <scratch-dir>      built and run by tests/pinch/app.sh (links the objects of build-cmake)
// Touch events are made with QTest, offscreen, in the phone shell (QSSTV_UI=phone). Exit code 0 when every check holds.
#include <QtTest>
#include <QtWidgets>
#include <cstdio>
#include <cstdlib>
#include "appglobal.h"
#include "configparams.h"
#include "dispatcher.h"
#include "mainwindow.h"
#include "extviewer.h"
#include "imageviewer.h"
#include "uimode.h"
#include "touchpage.h"

static int failures = 0;
#define CHECK(cond, what) do {if (cond) printf("  ok    %s\n", what); else {printf("  FAIL  %s\n", what); failures++;}} while (0)

int main(int argc, char **argv)
{
  setvbuf(stdout, nullptr, _IOLBF, 0);
  if (argc < 2) {fprintf(stderr, "usage: pinch scratch\n"); return 2;}
  const QString scratch = argv[1];
  qputenv("HOME", scratch.toLocal8Bit());
  qputenv("XDG_CONFIG_HOME", (scratch + "/config").toLocal8Bit());
  qputenv("XDG_DATA_HOME", (scratch + "/data").toLocal8Bit());
  qputenv("QT_QPA_PLATFORM", "offscreen");
  qputenv("PULSE_SERVER", "unix:/nonexistent/qsstv-test");
  qputenv("QSSTV_UI", "phone");
  qputenv("QSSTV_UI_WIDTH", "411");
  QCoreApplication::setOrganizationName("qsstv-pinch");
  QCoreApplication::setApplicationName("qsstv-pinch");
  QApplication app(argc, argv);
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, scratch + "/settings");
  {
    QSettings qs;
    qs.setValue("update/asked", true);
    qs.setValue("update/autoCheck", false);
    qs.setValue("appimage/integrationAsked", true);
  }
  globalInit();
  mainWindowPtr = new mainWindow;
  QTest::qWait(300);
  mainWindowPtr->init();
  mainWindowPtr->startRunning(false);
  mainWindowPtr->resize(411, 800);
  mainWindowPtr->show();
  QTest::qWait(500);

  QImage pic(1200, 900, QImage::Format_RGB32);
  pic.fill(Qt::darkGray);
  extViewer ev;
  ev.setup(pic, "pinch test");
  ev.show();
  QTest::qWait(500);
  imageViewer *v = ev.findChild<imageViewer *>();
  if (!v) {printf("  FAIL  no viewer in the dialog\n"); return 1;}
  const QPoint c = v->rect().center();
  QPointingDevice *dev = QTest::createTouchDevice();
  auto widthOf = [&]() {return v->viewRect().isNull() ? pic.width() : v->viewRect().width();};
  printf("viewer %dx%d, picture %dx%d\n", v->width(), v->height(), pic.width(), pic.height());

  // two fingers 100 px apart spread to 200 px, in several steps (a finger does not jump)
  auto pinch = [&](double from, double to, int steps)
  {
    auto seq = QTest::touchEvent(v, dev);
    seq.press(0, c - QPoint(int(from / 2), 0), v).press(1, c + QPoint(int(from / 2), 0), v).commit();
    for (int i = 1; i <= steps; i++)
      {
        const double d = from + (to - from) * i / steps;
        seq.move(0, c - QPoint(int(d / 2), 0), v).move(1, c + QPoint(int(d / 2), 0), v).commit();
      }
    seq.release(0, c - QPoint(int(to / 2), 0), v).release(1, c + QPoint(int(to / 2), 0), v).commit();
    QTest::qWait(50);
  };

  CHECK(widthOf() == pic.width(), "starts showing the whole picture");
  pinch(100, 150, 5);
  const int w1 = widthOf();
  CHECK(w1 < pic.width(), "spreading two fingers zooms in");
  CHECK(w1 > pic.width() * 6 / 10 && w1 < pic.width() * 8 / 10, "by a factor 1.5 for fingers 1.5 times as far apart, not by a step");
  pinch(100, 200, 8);
  const int w2 = widthOf();
  CHECK(w2 < w1, "a second spread zooms in further");
  CHECK(w2 > 48, "and not below the smallest view");
  pinch(200, 100, 8);
  const int w3 = widthOf();
  CHECK(w3 > w2, "pinching the fingers together zooms out");
  CHECK(w3 > w2 * 17 / 10 && w3 < w2 * 23 / 10, "by the factor of the distance, 2");
  pinch(200, 30, 12);
  pinch(200, 30, 12);
  pinch(200, 30, 12);
  CHECK(v->viewRect().isNull() || v->viewRect().width() >= pic.width(), "pinching in far enough shows the whole picture again, and no more");

  // one finger pans a zoomed picture
  pinch(100, 300, 10);
  const QRect before = v->viewRect();
  CHECK(!before.isNull() && before.width() < pic.width(), "zoomed in again");
  {
    auto seq = QTest::touchEvent(v, dev);
    seq.press(0, c, v).commit();
    for (int i = 1; i <= 6; i++) seq.move(0, c - QPoint(20 * i, 0), v).commit();
    seq.release(0, c - QPoint(120, 0), v).commit();
  }
  QTest::qWait(50);
  CHECK(v->viewRect().x() > before.x(), "a finger dragged to the left moves the view to the right");
  CHECK(v->viewRect().width() == before.width(), "without changing the zoom");

  // a double tap goes back to the whole picture, and one zooms in again
  auto tap = [&]()
  {
    auto seq = QTest::touchEvent(v, dev);
    seq.press(0, c, v).commit();
    seq.release(0, c, v).commit();
    QTest::qWait(60);
  };
  tap();
  tap();
  CHECK(v->viewRect().isNull() || v->viewRect().width() >= pic.width(), "a double tap on a zoomed picture shows all of it");
  QTest::qWait(500);
  tap();
  tap();
  CHECK(!v->viewRect().isNull() && v->viewRect().width() < pic.width(), "a double tap on the whole picture zooms in");
  printf("%s\n", failures ? "FAILED" : "all ok");
  return failures ? 1 : 0;
}
