// The wait cursor and startup (issue #195), in the REAL application objects, offscreen:
//   app <scratch-dir>
//   - starting the program (mainWindow::init, which sets up the Transmit tab and its size slider) does not ask for a wait cursor:
//     on macOS Qt's wait cursor crashed there, once for every launch (txWidget::slotSizeApply)
//   - after that, moving the DRM size slider re-encodes the picture and still shows the wait cursor (where the platform has one),
//     and gives it back
// Built and run by tests/busycursor/app.sh against an existing build-cmake tree (it links the application's object files).
#include <QtTest>
#include <QtWidgets>
#include <QDir>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include "appglobal.h"
#include "busycursor.h"
#include "mainwindow.h"
#include "txwidget.h"

static int passed = 0, failed = 0;
static void check(bool ok, const QString &what)
{
  printf("  %s %s\n", ok ? "ok  " : "FAIL", qPrintable(what));
  ok ? passed++ : failed++;
}

[[noreturn]] static void finish()
{
  printf("\n%d passed, %d failed\n", passed, failed);
  fflush(stdout);
  _exit(failed ? 1 : 0);   // no destructors, no closeEvent: nothing is saved here
}

int main(int argc, char **argv)
{
  setvbuf(stdout, nullptr, _IOLBF, 0);
  if (argc < 2) return 2;
  const QString scratch = argv[1];
  qputenv("HOME", scratch.toLocal8Bit());
  qputenv("XDG_CONFIG_HOME", (scratch + "/config").toLocal8Bit());
  qputenv("XDG_DATA_HOME", (scratch + "/data").toLocal8Bit());
  qputenv("QT_QPA_PLATFORM", "offscreen");
  qputenv("PIPEWIRE_REMOTE", "qsstv-test-no-such-server");
  qputenv("PULSE_SERVER", "unix:/nonexistent/qsstv-test");
  QCoreApplication::setOrganizationName("qsstv-app-test");
  QCoreApplication::setApplicationName("qsstv-app-test");
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
  mainWindowPtr->hide();
  mainWindowPtr->startRunning(false);
  QTest::qWait(300);

  check(busyCursor::timesSet() == 0, "starting the program asked for no wait cursor");
  check(QGuiApplication::overrideCursor() == nullptr, "and none is left on the screen");

  QSlider *slider = txWidgetPtr->findChild<QSlider *>("sizeSlider");
  check(slider != nullptr, "the size slider exists");
  if (!slider) finish();
  txWidgetPtr->changeTransmissionMode(TRXDRM);   // the slider is the DRM picture size
  QTest::qWait(100);
  const int before = busyCursor::timesSet();
  int target = slider->value() + 7;
  if (target > slider->maximum()) target = slider->value() - 7;
  if (target < slider->minimum() || target == slider->value()) target = slider->minimum() == slider->value() ? slider->maximum() : slider->minimum();
  printf("  .. slider %d..%d, %d -> %d\n", slider->minimum(), slider->maximum(), slider->value(), target);
  slider->setValue(target);   // not held down: the way a click on the groove or the keys move it
  QTest::qWait(300);
  check(slider->value() == target, "the slider moved");
#if BUSYCURSOR_AVAILABLE
  check(busyCursor::timesSet() == before + 1, "moving it shows the wait cursor once while the picture is made again");
#else
  check(busyCursor::timesSet() == before, "moving it asks for no wait cursor (this platform has none)");
#endif
  check(QGuiApplication::overrideCursor() == nullptr, "and the cursor is back to normal afterwards");
  finish();
}
