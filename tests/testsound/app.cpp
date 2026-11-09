// Settings > Audio and radio > Test sound sends a short SSTV picture (issue #185), in the REAL application objects, offscreen:
//   app <scratch-dir>
//   - the picture is B/W 8, about ten seconds, through the normal send path into a wav file (the test decodes it afterwards: app.sh)
//   - the Transmit screen is as it was: picture, mode, template flag, Free zoom and pan; nothing new in the sent pictures
//   - the digital page is switched to analog for it and back when it ends, or is stopped
//   - not started while a picture is being received (a notice), and with no sound output a box says so
// Built and run by tests/testsound/app.sh against an existing Qt Quick build (it links the application's object files).
#include <QtTest>
#include <QtWidgets>
#include <QDir>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#define private public
#include "sstvrx.h"
#include "rxfunctions.h"
#undef private
#include "appglobal.h"
#include "appmodel.h"
#include "drm.h"
#include "directoriesconfig.h"
#include "dispatcher.h"
#include "imageviewer.h"
#include "mainwindow.h"
#include "rxwidget.h"
#include "soundbase.h"
#include "sstvparam.h"
#include "txcontroller.h"
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

static bool waitFor(std::function<bool()> f, int ms)
{
  QElapsedTimer t; t.start();
  while (t.elapsed() < ms) { if (f()) return true; QTest::qWait(5); }
  return f();
}

// the length in seconds of a wav file written by the application (16 bit PCM)
static double wavSeconds(const QString &file)
{
  QFile f(file);
  if (!f.open(QIODevice::ReadOnly)) return 0;
  const QByteArray h = f.read(44);
  if (h.size() < 44) return 0;
  auto u32 = [&](int o) { return quint32(quint8(h[o])) | quint32(quint8(h[o + 1])) << 8 | quint32(quint8(h[o + 2])) << 16 | quint32(quint8(h[o + 3])) << 24; };
  auto u16 = [&](int o) { return quint32(quint8(h[o])) | quint32(quint8(h[o + 1])) << 8; };
  const double bytesPerSecond = double(u32(24)) * u16(22) * u16(34) / 8;
  return bytesPerSecond > 0 ? double(f.size() - 44) / bytesPerSecond : 0;
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

  rxFunctions *rxf = rxWidgetPtr->functionsPtr();
  check(rxf && txWidgetPtr && rigControllerPtr, "the Transmit tab, the receiver and the rig controller exist");
  if (!(rxf && txWidgetPtr && rigControllerPtr)) finish();
  TxController *tx = TxController::instance();
  AppModel *am = AppModel::instance();
  imageViewer *iv = txWidgetPtr->getImageViewerPtr();

  QStringList notices, problems;
  QObject::connect(am, &AppModel::noticeShown, am, [&](const QString &t) { notices << t; });
  QObject::connect(am, &AppModel::problemShown, am, [&](const QString &title, const QString &t) { problems << title + "|" + t; });

  // the user's side of the screen: a picture with a template switched off, Robot 36, Free framing zoomed and moved
  transmissionModeIndex = TRXSSTV;
  txWidgetPtr->changeTransmissionMode(TRXSSTV);
  QImage img(320, 256, QImage::Format_RGB32);
  for (int y = 0; y < img.height(); y++)
    for (int x = 0; x < img.width(); x++) img.setPixel(x, y, qRgb(x * 255 / 319, y * 255 / 255, 90));
  iv->openImage(img);
  txWidgetPtr->slotModeChanged(R36);
  iv->setFitMode(imageViewer::FITFREE);
  iv->setFramingState(2.0, QPointF(0.1, -0.05));
  QTest::qWait(100);
  const QImage mine = *iv->getImagePtr();
  auto sameAsBefore = [&](const char *when) {
    double z; QPointF p; iv->framingState(z, p);
    check(*iv->getImagePtr() == mine, QString("%1: the user's picture is the same").arg(when));
    check(sstvModeIndexTx == R36, QString("%1: the mode is Robot 36 again").arg(when));
    check(qFuzzyCompare(z, 2.0) && qFuzzyCompare(p.x() + 1, 1.1) && qFuzzyCompare(p.y() + 1, 0.95), QString("%1: zoom %2 and pan %3, %4 as before").arg(when).arg(z).arg(p.x()).arg(p.y()));
    check(!txWidgetPtr->calibrationTxActive() && !tx->sending() && tx->toolName().isEmpty(), QString("%1: nothing is sending, no tool name").arg(when));
  };
  check(iv->hasValidImage() && iv->fitMode() == imageViewer::FITFREE, "a picture with Free framing is on the Transmit tab");

  // the sound output is a file (like --encode), so a transmission runs without a sound card
  printf("\n== the test picture in analog SSTV\n");
  const QString wav1 = scratch + "/sample1.wav";
  soundBase::txFileName = wav1;
  const int sentBefore = QDir(txSSTVImagesPath).entryList(QDir::Files).size();
  tx->sendSampleImage();
  check(tx->sending() && tx->toolName() == "Test picture, B/W 8", QString("the overlay is up at once, named '%1'").arg(tx->toolName()));
  check(sstvModeIndexTx == BW8, "the mode is B/W 8 while it is sent");
  const int shown = tx->totalText().section(' ', 0, 0).toInt();
  check(shown >= 10 && shown <= 16, QString("the overlay gives the time the picture takes: '%1'").arg(tx->totalText()));
  bool wasSending = true;
  QElapsedTimer t; t.start();
  while (t.elapsed() < 90000 && (txWidgetPtr->calibrationTxActive() || tx->sending() || txWidgetPtr->functionsPtr()->txBusy()))
    {
      QTest::qWait(20);
      if (!tx->sending() && txWidgetPtr->calibrationTxActive()) wasSending = false;   // it must not flap in the gap between the steps
    }
  check(wasSending, "sending never dropped while the test picture was on its way");
  QTest::qWait(500);
  const double secs = wavSeconds(wav1);
  check(secs > 8.5 && secs < 14, QString("the recording is %1 s: about ten seconds").arg(secs, 0, 'f', 1));
  sameAsBefore("after the test picture");
  check(QDir(txSSTVImagesPath).entryList(QDir::Files).size() == sentBefore, "no picture was added to the sent pictures");
  check(notices.isEmpty() && problems.isEmpty(), "no notice, no box");

  printf("\n== from the digital page\n");
  mainWindowPtr->switchMode(TRXDRM);
  QTest::qWait(300);
  check(am->digital(), "the digital page is on");
  soundBase::txFileName = scratch + "/sample2.wav";
  tx->sendSampleImage();
  check(!am->digital() && transmissionModeIndex == TRXSSTV, "it is switched to analog for the test picture");
  check(waitFor([&] { return !txWidgetPtr->calibrationTxActive() && !tx->sending() && !txWidgetPtr->functionsPtr()->txBusy(); }, 90000), "the test picture ends");
  check(waitFor([&] { return am->digital(); }, 3000), "and the digital page is back");
  check(wavSeconds(scratch + "/sample2.wav") > 8.5, "it was a whole picture");
  check(*iv->getImagePtr() == mine, "the user's picture is the same");

  printf("\n== Stop in the middle, from the digital page\n");
  soundBase::txFileName = scratch + "/sample3.wav";
  tx->sendSampleImage();
  QTest::qWait(150);
  tx->stop();
  check(waitFor([&] { return !txWidgetPtr->calibrationTxActive() && !tx->sending() && !txWidgetPtr->functionsPtr()->txBusy(); }, 30000), "Stop ends it");
  check(waitFor([&] { return am->digital(); }, 3000), "the digital page is back after Stop");
  check(*iv->getImagePtr() == mine, "the user's picture is the same after Stop");
  mainWindowPtr->switchMode(TRXSSTV);
  QTest::qWait(300);
  check(!am->digital(), "(the analog page again)");
  sameAsBefore("after Stop");

  printf("\n== not while a picture is being received\n");
  notices.clear();
  rxf->sstvRxPtr->SSTVState = sstvRx::PROCESSING;
  check(mainWindowPtr->busyMode() >= 0, "the receiver counts as busy");
  tx->sendSampleImage();
  QTest::qWait(300);
  check(notices.size() == 1 && !tx->sending() && !txWidgetPtr->calibrationTxActive(), QString("analog: a notice and nothing is sent ('%1')").arg(notices.value(0)));
  rxf->sstvRxPtr->SSTVState = sstvRx::HUNTING;
  QTest::qWait(100);
  mainWindowPtr->switchMode(TRXDRM);
  QTest::qWait(300);
  rxf->rxState = rxFunctions::RXRUNNING;   // a digital picture is being received
  drmBusy = true;
  notices.clear();
  check(mainWindowPtr->busyMode() >= 0, "the digital receiver counts as busy");
  tx->sendSampleImage();
  QTest::qWait(300);
  check(notices.size() == 1 && !tx->sending() && !txWidgetPtr->calibrationTxActive() && am->digital(), "digital: a notice, nothing is sent and the page stays digital");
  drmBusy = false;
  rxf->rxState = rxFunctions::RXIDLE;
  QTest::qWait(100);
  mainWindowPtr->switchMode(TRXSSTV);
  QTest::qWait(300);

  printf("\n== no sound output\n");
  soundBase::txFileName.clear();
  check(!soundIOPtr->driverOK(), "(this run has no sound output)");
  problems.clear();
  notices.clear();
  tx->sendSampleImage();
  check(waitFor([&] { return !problems.isEmpty(); }, 5000), "a box says so");
  printf("  .. %s\n", qPrintable(problems.value(0)));
  check(problems.value(0).startsWith("Test sound|The test picture was not sent") && problems.value(0).contains("Settings > Audio and radio"), "it names the test and where the speaker is chosen");
  check(waitFor([&] { return !txWidgetPtr->calibrationTxActive() && !tx->sending(); }, 5000), "nothing keeps sending");
  sameAsBefore("after the failure");

  finish();
}
