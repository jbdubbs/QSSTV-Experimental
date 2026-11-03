// A Start that cannot transmit says why (issue #162), in the REAL application objects, offscreen:
//   app <scratch-dir>
//   - Start while the receiver is busy, or a picture is arriving: the transmission starts (what is being received does not matter);
//     a picture that has arrived far enough (Save if Complete) is kept, a shorter one is dropped
//   - Start while a transmission runs: a status bar line (it used to be silent)
//   - Start with no working sound output: a box that stays (non-modal, shown once) and a status bar line, the controls unlocked again;
//     a calibration / --encode transmission and a beacon get no box
//   - a DRM picture that cannot be compressed (#166): the tab unlocks, the sound output is let go of, a box says why
//   - keying the radio fails: a box when the transmitter is keyed down, none when it is released, none when no PTT is meant
// Built and run by tests/starthint/app.sh against an existing build-cmake tree (it links the application's object files).
#include <QtTest>
#include <QtWidgets>
#include <QDir>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#define private public
#include "sstvrx.h"
#undef private
#include "appglobal.h"
#include "directoriesconfig.h"
#include "dispatcher.h"
#include "mainwindow.h"
#include "rigcontrol.h"
#include "rxfunctions.h"
#include "rxwidget.h"
#include "soundbase.h"
#include "txfunctions.h"
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

static QList<QMessageBox *> boxes()
{
  QList<QMessageBox *> l;
  for (QWidget *w : QApplication::topLevelWidgets())
    if (w->isVisible())
      if (QMessageBox *m = qobject_cast<QMessageBox *>(w)) l << m;
  return l;
}
static void closeBoxes()
{
  for (QMessageBox *m : boxes()) m->close();
  QTest::qWait(50);   // deleteLater of the closed boxes
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}
static bool boxWith(const QString &title, const QString &text)
{
  for (QMessageBox *m : boxes())
    if (m->windowTitle() == title && m->text().contains(text)) return true;
  return false;
}
static void say(const QString &s) { printf("  .. %s\n", qPrintable(s)); fflush(stdout); }
static bool txRunning() { return txWidgetPtr->functionsPtr()->txBusy(); }
static bool waitFor(std::function<bool()> f, int ms)
{
  QElapsedTimer t; t.start();
  while (t.elapsed() < ms) { if (f()) return true; QTest::qWait(5); }
  return f();
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
  qputenv("PIPEWIRE_REMOTE", "qsstv-test-no-such-server");   // no sound server: the output cannot be started
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

  QToolButton *startBtn = txWidgetPtr->findChild<QToolButton *>("startToolButton");
  QComboBox *modeBox = txWidgetPtr->findChild<QComboBox *>("sstvModeComboBox");
  rxFunctions *rxf = rxWidgetPtr->functionsPtr();
  check(startBtn && modeBox && rxf && rigControllerPtr, "the Transmit tab, the receiver and the rig controller exist");
  if (!(startBtn && modeBox && rxf && rigControllerPtr)) finish();
  auto locked = [&] { return !modeBox->isEnabled(); };

  transmissionModeIndex = TRXSSTV;
  txWidgetPtr->changeTransmissionMode(TRXSSTV);
  QImage img(320, 256, QImage::Format_RGB32);
  img.fill(QColor(40, 90, 160));
  txWidgetPtr->getImageViewerPtr()->openImage(img);
  QTest::qWait(100);
  check(txWidgetPtr->getImageViewerPtr()->hasValidImage(), "a picture is on the Transmit tab");

  // the sound output is a file from here on (like --encode), so a transmission can run without a sound card; a short mode keeps it small
  txWidgetPtr->slotModeChanged(R36);
  printf("\n== Start while the receiver is busy: it is not held back\n");
  soundBase::txFileName = scratch + "/forced.wav";
  statusBarPtr->clearMessage();
  rxf->sstvRxPtr->SSTVState = sstvRx::PROCESSING;
  check(mainWindowPtr->busyMode() >= 0, "the receiver counts as busy");
  QTest::mouseClick(startBtn, Qt::LeftButton);
  check(locked(), "Start is taken: the controls lock at once");
  check(waitFor([&] { return !locked() && !txRunning(); }, 60000) && QFileInfo(scratch + "/forced.wav").size() > 100000,
        "the transmission was sent and ended");
  check(mainWindowPtr->busyMode() < 0, "the receiver is back to listening afterwards");
  check(!statusBarPtr->currentMessage().contains("Not transmitting"), "no 'Not transmitting' line for a receiver");

  printf("\n== Start while a picture is arriving: what has arrived is kept when it is enough (Save if Complete)\n");
  const QString inWav = scratch + "/in.wav";
  soundBase::txFileName = inWav;
  QTest::mouseClick(startBtn, Qt::LeftButton);
  waitFor([&] { return !locked() && !txRunning(); }, 60000);
  check(QFileInfo(inWav).size() > 1000000, "a Robot 36 transmission is recorded to be received");
  auto saved = [&] { return QDir(rxSSTVImagesPath).entryList(QStringList() << "R36_*", QDir::Files).size(); };
  // receive the recording (about ten times real time) and press Start when `percent` of it has gone through the receiver
  auto startAt = [&](int percent, const QString &out) -> bool {
    soundBase::txFileName.clear();
    dispatcherPtr->idleAll();
    QString err;
    if (!soundIOPtr->startFileCapture(inWav, true, err)) { printf("  .. %s\n", qPrintable(err)); return false; }
    rxf->startRX();
    if (!waitFor([&] { return soundIOPtr->fileProgressPercent() >= percent && rxf->sstvRxPtr->isReceivingImage(); }, 30000)) return false;
    soundBase::txFileName = out;
    QTest::mouseClick(startBtn, Qt::LeftButton);
    const bool taken = locked();
    const bool ended = waitFor([&] { return !locked() && !txRunning(); }, 60000);
    soundIOPtr->clearFileSource();
    return taken && ended && QFileInfo(out).size() > 100000;
  };
  int before = saved();
  say(QString("pictures in %1 before: %2").arg(rxSSTVImagesPath).arg(before));
  check(startAt(75, scratch + "/over_a.wav"), "Start at about 75 % of the picture: the transmission is sent");
  check(saved() == before + 1, QString("and the picture is kept (%1 saved)").arg(saved() - before));
  before = saved();
  check(startAt(30, scratch + "/over_b.wav"), "Start at about 25 % of the picture: the transmission is sent");
  check(saved() == before, QString("and the picture is dropped, it is under Save if Complete (%1 saved)").arg(saved() - before));
  check(mainWindowPtr->busyMode() < 0, "the receiver is idle again");

  printf("\n== Start while a transmission runs\n");
  statusBarPtr->clearMessage();
  txWidgetPtr->functionsPtr()->startTX(txFunctions::TXSENDDRMTXT);   // a state that stays busy (and sends nothing)
  check(waitFor([] { return txWidgetPtr->functionsPtr()->txBusy(); }, 2000), "the transmitter is busy");
  QTest::mouseClick(startBtn, Qt::LeftButton);
  QTest::qWait(100);
  QString msg = statusBarPtr->currentMessage();
  printf("  .. status bar: '%s'\n", qPrintable(msg));
  check(msg.contains("Not transmitting") && msg.contains("transmission is running"), "the status bar says that a transmission is running");
  dispatcherPtr->idleAll();
  check(waitFor([] { return !txWidgetPtr->functionsPtr()->txBusy(); }, 2000), "(the transmitter is idle again)");

  printf("\n== Start with no working sound output\n");
  soundBase::txFileName.clear();
  check(!soundIOPtr->driverOK(), "(this run has no sound output)");
  statusBarPtr->clearMessage();
  closeBoxes();
  QTest::mouseClick(startBtn, Qt::LeftButton);
  check(waitFor([&] { return !boxes().isEmpty(); }, 3000), "a box appears");
  printf("  .. box: '%s'\n", boxes().isEmpty() ? "" : qPrintable(boxes().first()->text()));
  check(boxWith("Cannot transmit", "no working sound output") && boxWith("Cannot transmit", "Configuration > Sound"),
        "it says there is no working sound output and where to choose one");
  check(boxes().first()->windowModality() == Qt::NonModal, "it is non-modal: nothing waits for it");
  check(!locked() && startBtn->isEnabled(), "the controls are unlocked again behind it");
  check(statusBarPtr->currentMessage().contains("no working sound output"), "the status bar says it too");
  QTest::mouseClick(startBtn, Qt::LeftButton);
  QTest::qWait(300);
  check(boxes().size() == 1, QString("pressing Start again does not pile up boxes (%1 open)").arg(boxes().size()));
  closeBoxes();
  check(boxes().isEmpty(), "closing it works");

  printf("\n== no box for a calibration / --encode transmission\n");
  statusBarPtr->clearMessage();
  const bool sent = txWidgetPtr->sendCalibrationImage(M1, img);
  QTest::qWait(500);
  check(sent && boxes().isEmpty(), "sendCalibrationImage() with no sound output opens no box (the caller reports it)");
  check(!txWidgetPtr->calibrationTxActive() && !locked(), "and the Transmit tab is back as it was");

  printf("\n== the PTT\n");
  scatParams *cp = rigControllerPtr->params();
  soundBase::txFileName.clear();
  auto key = [&](bool on) { closeBoxes(); rigControllerPtr->activatePTT(on); QTest::qWait(50); };
  cp->enableXMLRPC = false;
  cp->enableSerialPTT = false;
  cp->enableCAT = false;

  key(true);
  check(boxes().isEmpty(), "no PTT set up (VOX): keying opens nothing");

  cp->enableSerialPTT = true;
  cp->pttSerialPort = "/dev/qsstv-no-such-port";
  key(true);
  printf("  .. box: '%s'\n", boxes().isEmpty() ? "" : qPrintable(boxes().first()->text()));
  check(boxWith("PTT problem", "qsstv-no-such-port") && boxWith("PTT problem", "cannot be opened") && boxWith("PTT problem", "Stop ends"),
        "serial PTT on a port that cannot be opened: a box names the port and says the picture goes on");
  check(boxes().first()->windowModality() == Qt::NonModal, "it is non-modal");
  closeBoxes();
  rigControllerPtr->activatePTT(false);
  QTest::qWait(50);
  check(boxes().isEmpty(), "letting go of that PTT stays quiet");
  rigControllerPtr->activatePTT(true);
  rigControllerPtr->activatePTT(true);
  QTest::qWait(50);
  check(boxes().size() == 1, QString("the same problem twice is one box (%1 open)").arg(boxes().size()));

  cp->pttSerialPort.clear();
  key(true);
  check(boxWith("PTT problem", "no serial port is chosen"), "serial PTT with no port chosen: a box says so");
  key(false);
  check(boxes().isEmpty(), "(and the release is quiet)");

  cp->enableSerialPTT = false;
  cp->enableCAT = true;
  cp->pttType = RIG_PTT_RIG;
  key(true);
  check(boxWith("PTT problem", "not connected"), "CAT PTT while the radio is not connected: a box says so");
  key(false);
  check(boxes().isEmpty(), "(and the release is quiet)");

  cp->pttType = RIG_PTT_NONE;
  key(true);
  check(boxes().isEmpty(), "CAT on with PTT type None (VOX): nothing is wrong, no box");

  cp->enableCAT = false;
  cp->enableXMLRPC = true;
  cp->pttType = RIG_PTT_RIG;
  key(true);
  check(boxes().isEmpty(), "PTT through XML-RPC: no box");
  cp->enableXMLRPC = false;
  closeBoxes();

  printf("\n== a DRM picture that cannot be compressed (#166): nothing stays locked\n");
  // the encoder writes its output to a temporary file in QDir::tempPath(): a folder that is not there makes every encode fail
  const QString goodTmp = scratch + "/tmp";
  QDir().mkpath(goodTmp);
  qputenv("TMPDIR", "/nonexistent-qsstv-tmp");
  mainWindowPtr->switchMode(TRXDRM);
  check(transmissionModeIndex == TRXDRM, "(DRM mode)");
  const QString pic = scratch + "/pic.png";
  img.save(pic);
  imageViewer *iv = txWidgetPtr->getImageViewerPtr();
  QString picName = pic;
  iv->openImage(picName, false, true, false, false);
  waitFor([&] { return !iv->encodePending(); }, 30000);
  QTest::qWait(300);
  {
    QByteArray probe;
    check(!iv->copyToBuffer(&probe) && probe.isEmpty(), "(the encoder really gives nothing with that temporary folder)");
  }
  soundBase::txFileName = scratch + "/drmfail.wav";   // the sound output is a file: only the encode can fail
  closeBoxes();
  statusBarPtr->clearMessage();
  QTest::mouseClick(startBtn, Qt::LeftButton);
  check(waitFor([&] { return !boxes().isEmpty(); }, 8000), "a box appears");
  printf("  .. box: '%s'\n", boxes().isEmpty() ? "" : qPrintable(boxes().first()->text()));
  check(boxWith("Cannot transmit", "could not be compressed") && boxWith("Cannot transmit", "/nonexistent-qsstv-tmp"),
        "it says the picture could not be compressed and names the temporary folder");
  check(waitFor([&] { return !locked() && startBtn->isEnabled() && !txRunning(); }, 5000), "the Transmit tab is unlocked again");
  check(!soundIOPtr->isPlaying(), "the sound output is let go of");
  check(statusBarPtr->currentMessage().contains("Nothing was transmitted"), "the status bar says it too");
  closeBoxes();

  qputenv("TMPDIR", goodTmp.toLocal8Bit());
  iv->openImage(picName, false, true, false, false);
  waitFor([&] { return !iv->encodePending(); }, 30000);
  QTest::qWait(300);
  soundBase::txFileName = scratch + "/drmok.wav";
  QTest::mouseClick(startBtn, Qt::LeftButton);
  check(locked(), "with a usable temporary folder Start is taken");
  check(waitFor([&] { return !locked() && !txRunning(); }, 120000) && QFileInfo(scratch + "/drmok.wav").size() > 100000, "and the DRM picture is sent");
  check(boxes().isEmpty(), "without a box");

  finish();
}
