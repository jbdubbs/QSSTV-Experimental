// The live camera picture of WA8LMF Beacon Mode (#150) inside the REAL application objects (main window, TX tab, TX image viewer),
// offscreen and without a camera: frames are handed to the TX tab as the camera would, and the tab must show them under the template,
// leave the picture alone while one is being sent or waited for, and go live again afterwards:
//   app <scratch-dir> <fixtures-dir>
// Built and run by tests/beacon/app.sh against an existing build-cmake tree (it links the application's object files).
#include <QtTest>
#include <QtWidgets>
#include <cstdio>
#include <cstdlib>
// the beacon state machine and the viewer's buffers are private; the test steps them without a camera
#define private public
#include "appglobal.h"
#include "configparams.h"
#include "configdialog.h"
#include "dispatcher.h"
#include "editorsetup.h"
#include "gallerywidget.h"
#include "mainwindow.h"
#include "templatedoc.h"
#include "rigcontrol.h"
#include "sstvparam.h"
#include "txfunctions.h"
#include "drmtransmitter.h"
#include "ui_txwidget.h"
#include "ftpfunctions.h"
#include "beaconcapture.h"
#include "imageviewer.h"
#include "txwidget.h"
#undef private

static int passed = 0, failed = 0;
static void check(bool ok, const QString &what)
{
  printf("  %s %s\n", ok ? "ok  " : "FAIL", qPrintable(what));
  ok ? passed++ : failed++;
}

template <typename F> static bool waitFor(F cond, int ms = 6000)
{
  for (int t = 0; t < ms && !cond(); t += 50) QTest::qWait(50);
  return cond();
}

// a picture that is different for every seed
static QImage frame(int w, int h, int seed)
{
  QImage im(w, h, QImage::Format_RGB32);
  for (int y = 0; y < h; y++)
    {
      QRgb *line = reinterpret_cast<QRgb *>(im.scanLine(y));
      for (int x = 0; x < w; x++) line[x] = qRgb((x * 255 / w + seed * 40) & 255, (y * 255 / h + seed * 70) & 255, (seed * 90 + 30) & 255);
    }
  return im;
}

static bool same(const QImage &a, const QImage &b)
{
  return a.size() == b.size() && a.convertToFormat(QImage::Format_ARGB32) == b.convertToFormat(QImage::Format_ARGB32);
}

int main(int argc, char **argv)
{
  setvbuf(stdout, nullptr, _IOLBF, 0);
  const QString scratch = argv[1], fixtures = argv[2];
  qputenv("HOME", scratch.toLocal8Bit());
  qputenv("XDG_CONFIG_HOME", (scratch + "/config").toLocal8Bit());
  qputenv("XDG_DATA_HOME", (scratch + "/data").toLocal8Bit());
  qputenv("QT_QPA_PLATFORM", "offscreen");
  // no sound server: a transmission must not play through the speakers or an audio interface of the rig, it finds no sound output
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
  mainWindowPtr->startRunning(false);   // no sound card receiver
  mainWindowPtr->show();
  QTest::qWait(300);
  int boxes = 0;
  QTimer closer;
  QObject::connect(&closer, &QTimer::timeout, [&] {
    if (QWidget *w = QApplication::activeModalWidget()) {boxes++; w->close();}
  });
  closer.start(100);

  txWidget *txw = txWidgetPtr;
  imageViewer *tx = txw->getImageViewerPtr();
  const QString templ = scratch + "/synthetic.templ";
  QFile::copy(fixtures + "/synthetic.templ", templ);

  printf("== the viewer shows a camera frame like a loaded picture (SSTV)\n");
  {
    transmissionModeIndex = TRXSSTV;
    const QImage f1 = frame(640, 480, 1), f2 = frame(640, 480, 2);
    tx->setParam(templ, false, 320, 256);
    tx->openImage(f1);
    tx->applyTemplate();
    const QImage plain = *tx->getDisplayedImage();
    check(plain.size() == QSize(320, 256), "without a template the picture is framed to the mode size");
    check(tx->setLiveFrame(f1) && same(*tx->getDisplayedImage(), plain), "a live frame is rendered the same way");
    check(tx->setLiveFrame(f2) && !same(*tx->getDisplayedImage(), plain), "and the next frame replaces it");

    tx->setParam(templ, true, 320, 256);
    tx->openImage(f1);
    tx->applyTemplate();
    const QImage templated = *tx->getDisplayedImage();
    check(!same(templated, plain), "(the template changes the picture)");
    check(tx->setLiveFrame(f1) && same(*tx->getDisplayedImage(), templated), "with the template on, a live frame is exactly what applyTemplate() makes of it");
    check(same(*tx->getImagePtr(), f1) && tx->getFilename().isEmpty(), "the frame is the source picture and no file is left behind");
    check(tx->compressedImageData.isEmpty(), "nothing is encoded");

    tx->setGrid(2, 1);
    check(!tx->setLiveFrame(f2), "a grid takes no live frame");
    tx->setGrid(1, 1);
    check(!tx->setLiveFrame(QImage()), "nor does a null frame");
  }

  printf("\n== DRM: the template is shown, the picture is not encoded\n");
  {
    transmissionModeIndex = TRXDRM;
    const QImage f1 = frame(1280, 720, 3);
    tx->setParam(templ, true, 0, 0);
    tx->openImage(f1);
    QElapsedTimer t;
    t.start();
    check(tx->setLiveFrame(f1), "a 1280x720 frame is accepted");
    const qint64 ms = t.elapsed();
    check(tx->getDisplayedImage()->size() == f1.size() && !same(*tx->getDisplayedImage(), f1), "shown full size with the template over it");
    check(tx->compressedImageData.isEmpty() && !tx->encodePending(), "and not encoded");
    check(ms < 400, QString("in time for a live picture (%1 ms)").arg(ms));
    transmissionModeIndex = TRXSSTV;
    tx->setParam(templ, true, 320, 256);
  }

  printf("\n== the TX tab: when the live picture may replace the Transmit picture\n");
  {
    // the main tabs: the page that holds the TX tab is selected
    QTabWidget *tabs = mainWindowPtr->findChild<QTabWidget *>("maintabWidget");
    auto showTx = [&] {
      for (int i = 0; tabs && i < tabs->count(); i++)
        if (tabs->widget(i) == txw || tabs->widget(i)->isAncestorOf(txw)) tabs->setCurrentIndex(i);
      QTest::qWait(100);
    };
    showTx();
    check(txw->isVisible(), "the TX tab is showing");
    txw->beaconRunning = true;
    txw->livePaused = false;
    txw->txLocked = false;
    const QImage mine = frame(640, 480, 5), a = frame(640, 480, 6), b = frame(640, 480, 7);
    tx->openImage(mine);
    txw->liveKey = tx->getImagePtr()->cacheKey();
    txw->slotBeaconLive(a);
    check(same(*tx->getImagePtr(), a), "a frame replaces the picture the beacon loaded");
    txw->slotBeaconLive(b);
    check(same(*tx->getImagePtr(), b), "and each next frame the one before");

    // template, mode and size changes re-render the live picture, they do not make the user's picture of it
    txw->applyTemplate();
    txw->slotGetParams();
    txw->slotBeaconLive(a);
    check(same(*tx->getImagePtr(), a) && !txw->livePaused, "changes on the tab do not stop it");

    // a picture loaded by the user stays
    const QImage user = frame(640, 480, 8);
    tx->openImage(user);
    txw->slotBeaconLive(b);
    check(same(*tx->getImagePtr(), user) && txw->livePaused, "a picture the user loads stays");
    txw->slotBeaconLive(a);
    check(same(*tx->getImagePtr(), user), "(and keeps staying)");
    txw->enableButtons(true);
    txw->slotBeaconLive(a);
    check(same(*tx->getImagePtr(), a) && !txw->livePaused, "until a transmission is over: the live picture comes back");

    // a picture on its way to the air
    txw->enableButtons(false);
    check(txw->txLocked, "prepareTx() locks the controls ...");
    txw->slotBeaconLive(b);
    check(same(*tx->getImagePtr(), a), "... and the picture does not change");
    txw->enableButtons(true);
    txw->slotBeaconLive(b);
    check(same(*tx->getImagePtr(), b), "the transmission is over: live again");

    // the camera converts frames often only while somebody can see them, also while an error is showing in the status bar
    txw->beaconError = "not sent";
    txw->enableButtons(false);
    txw->slotBeaconTick();
    check(!txw->beaconCap.previewWanted, "no frames are converted often while a picture is on its way");
    txw->enableButtons(true);
    check(txw->beaconCap.previewWanted, "and they are again when it is over");
    txw->beaconError.clear();
    txw->slotBeaconLive(b);

    const struct {const char *name; bool *flag;} blockers[] = {
      {"a beacon picture on its way", &txw->beaconPending},
      {"the beacon waiting for the position", &txw->beaconWaitPosition},
      {"a Start waiting for the position", &txw->startWaitPosition},
      {"a calibration transmission", &txw->calTxActive},
    };
    for (const auto &bl : blockers)
      {
        *bl.flag = true;
        txw->slotBeaconLive(a);
        const bool kept = same(*tx->getImagePtr(), b);
        *bl.flag = false;
        txw->slotBeaconLive(a);
        check(kept && same(*tx->getImagePtr(), a), QString("%1: the picture stays, then goes on").arg(bl.name));
        txw->slotBeaconLive(b);
      }

    txw->beaconRunning = false;
    txw->slotBeaconLive(a);
    check(same(*tx->getImagePtr(), b), "without the beacon there is no live picture");
    txw->beaconRunning = true;

    mainWindowPtr->hide();
    QTest::qWait(100);
    txw->slotBeaconLive(a);
    check(same(*tx->getImagePtr(), b), "nothing is rendered while the tab is not showing");
    mainWindowPtr->show();
    showTx();
    txw->slotBeaconLive(a);
    check(same(*tx->getImagePtr(), a), "and it goes on when it is");
    txw->beaconRunning = false;
  }

  printf("\n== a beacon picture that cannot be sent gives the live picture back\n");
  {
    // the sound output is gone (see the environment above): the transmitter cannot start after the picture was prepared
    txw->beaconRunning = true;
    txw->beaconMinutes = 5;
    txw->beaconHold = false;
    txw->beaconError.clear();
    txw->livePaused = false;
    const QImage cam = frame(640, 480, 9), next = frame(640, 480, 10);
    txw->slotBeaconFrame(cam);
    check(txw->beaconPending || txw->txLocked || !txw->beaconError.isEmpty(), "the picture is committed (prepared, or refused)");
    check(waitFor([&] {return !txw->beaconPending && !txw->txLocked;}, 15000), "when it cannot be sent, the tab is not left locked");
    txw->slotBeaconLive(next);
    check(same(*tx->getImagePtr(), next), "and the live picture goes on: " + txw->beaconError);
    txw->stopBeacon();
  }

  printf("\n== the timer with no camera\n");
  {
    txw->beaconRunning = true;
    txw->beaconMinutes = 5;
    txw->beaconHold = false;
    txw->beaconPending = false;
    txw->beaconError.clear();
    txw->slotBeaconTimer();
    check(txw->beaconError.contains("camera") && txw->beaconTimer.isActive(), "says that the camera gave no picture and tries again in 30 s: " + txw->beaconError);
    txw->stopBeacon();
  }

  printf("\n%d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
