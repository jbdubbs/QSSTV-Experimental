// A failed DRM reception must not lock the program (issue #153), in the REAL application objects, offscreen:
//   app <scratch-dir>
// The mode lock (busyMode / updateModeLock) read the "DRM receiver is busy" flag without looking at the receiver, and the
// flag is only updated while the receiver runs; so a receiver that stopped (Stop, capture ended) while a weak signal had the
// FAC valid left the SSTV tabs greyed for good. The FAC flag itself was not cleared on a re-sync either.
// Built and run by tests/drmbusy/app.sh against an existing build-cmake tree (it links the application's object files).
#include <QtTest>
#include <QtWidgets>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include "appglobal.h"
#include "demodulator.h"
#include "drm.h"
#include "mainwindow.h"
#include "rxfunctions.h"
#include "rxwidget.h"
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
    // the first-run questions of the main window would block an offscreen run
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
  mainWindowPtr->startRunning(false);   // no sound card receiver: the receiver thread idles
  QTest::qWait(300);

  QTabWidget *rxTabs = rxWidgetPtr->findChild<QTabWidget *>("settingsTableWidget");
  QTabWidget *txTabs = txWidgetPtr->findChild<QTabWidget *>("settingsTableWidget");
  printf("== the tabs\n");
  rxFunctions *rxf = rxWidgetPtr->functionsPtr();
  check(rxTabs && txTabs && demodulatorPtr && rxf, "the mode tabs and the DRM receiver exist");
  if (!(rxTabs && txTabs && demodulatorPtr && rxf)) finish();
  check(rxf->isIdle(), "(the receiver is idle)");

  printf("\n== a stopped receiver with the FAC valid at the time\n");
  transmissionModeIndex = TRXDRM;
  drmBusy = true;   // what drmStatusFrame::setStatus() leaves behind when the receiver stops while the FAC is valid
  mainWindowPtr->updateModeLock();
  check(mainWindowPtr->busyMode() == -1, QString("nothing is busy (busyMode %1)").arg(mainWindowPtr->busyMode()));
  check(rxTabs->isTabEnabled(TRXSSTV), "the SSTV tab of the Receive tab is not greyed out");
  check(txTabs->isTabEnabled(TRXSSTV), "the SSTV tab of the Transmit tab is not greyed out");

  printf("\n== a re-sync forgets the FAC of the lost signal\n");
  fac_valid = 1;
  demodulatorPtr->init();
  check(fac_valid != 1, QString("fac_valid after a re-sync (%1)").arg(fac_valid));

  finish();
}
