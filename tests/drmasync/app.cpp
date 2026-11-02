// The DRM picture encode must not freeze the GUI (issue #164): switching the picture codec on the DRM Transmit tab, and moving the
// size slider, hand the AVIF or JPEG2000 encode to a worker thread. Runs in the REAL application objects (main window, TX tab,
// TX image viewer), offscreen, with a throw-away HOME and temp directory:
//   app <scratch-dir> [ignored]
// A 10 ms timer measures the longest time the event loop was blocked, and the call that switches the codec is timed itself.
// Built and run by tests/drmasync/app.sh against an existing build-cmake tree (it links the application's object files).
#include <QtTest>
#include <QtWidgets>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
// the viewer's compressed-data fields are private; the test reads them
#define private public
#include "appglobal.h"
#include "avifenc.h"
#include "configparams.h"
#include "dispatcher.h"
#include "drmprofileconfig.h"
#include "gallerywidget.h"
#include "imageviewer.h"
#include "mainwindow.h"
#include "rxwidget.h"
#include "sstvparam.h"
#include "txfunctions.h"
#include "drmtransmitter.h"
#include "ui_txwidget.h"
#include "ftpfunctions.h"
#include "beaconcapture.h"
#include "txwidget.h"
#undef private

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
  _exit(failed ? 1 : 0);
}

static void pick(QComboBox *c, int i)
{
  c->setCurrentIndex(i);
  emit c->activated(i);   // what a click in the list does
}

template <typename F> static bool waitFor(F cond, int ms)
{
  for (int t = 0; t < ms && !cond(); t += 20) QTest::qWait(20);
  return cond();
}

static bool isJp2(const QByteArray &ba) {return ba.startsWith(QByteArray::fromHex("0000000c6a502020"));}

static QStringList leftovers(const QString &tmp) {return QDir(tmp).entryList(QStringList() << "qsstv-*", QDir::Files);}

// the longest time the event loop did not get a turn
struct Watchdog
{
  QTimer timer;
  QElapsedTimer clock;
  qint64 last = 0, maxGap = 0;
  Watchdog()
  {
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [this]() {const qint64 n = clock.elapsed(); maxGap = qMax(maxGap, n - last); last = n;});
    clock.start();
    timer.start();
  }
  void reset() {last = clock.elapsed(); maxGap = 0;}
};

int main(int argc, char **argv)
{
  setvbuf(stdout, nullptr, _IOLBF, 0);
  if (argc < 2) return 2;
  const QString scratch = argv[1];
  const QString tmp = scratch + "/tmp";
  QDir().mkpath(tmp);
  qputenv("HOME", scratch.toLocal8Bit());
  qputenv("TMPDIR", tmp.toLocal8Bit());   // QDir::tempPath(): the encoder's temp files are counted here
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
    QSettings qs;   // the first-run questions of the main window would block an offscreen run
    qs.setValue("update/asked", true);
    qs.setValue("update/autoCheck", false);
    qs.setValue("appimage/integrationAsked", true);
  }
  globalInit();
  mainWindowPtr = new mainWindow;
  QTest::qWait(300);
  mainWindowPtr->init();
  mainWindowPtr->hide();
  mainWindowPtr->startRunning(false);   // no sound card receiver
  QTest::qWait(300);

  txWidget *txw = txWidgetPtr;
  imageViewer *tx = txw->getImageViewerPtr();
  QComboBox *encoding = txw->findChild<QComboBox *>("drmEncodingComboBox");
  QTabWidget *txTabs = txw->findChild<QTabWidget *>("settingsTableWidget");
  QSlider *sizeSlider = txw->findChild<QSlider *>("sizeSlider");
  check(encoding && txTabs && sizeSlider, "the Transmit tab has the codec box, the mode tabs and the size slider");
  if (!(encoding && txTabs && sizeSlider)) finish();
  const bool avif = avifWriteSupported();
  if (!avif) printf("  (this build has no libavif: only the JPEG2000 steps run)\n");

  // a picture that takes a real encode: a gradient with noise, big enough that the JPEG2000 encode is not instant
  QString file = scratch + "/picture.png";   // openImage() takes a QString &
  {
    QImage big(1280, 960, QImage::Format_RGB32);
    quint32 r = 12345;
    for (int y = 0; y < big.height(); y++)
      for (int x = 0; x < big.width(); x++)
        {
          r = r * 1664525u + 1013904223u;
          const int n = (r >> 24) & 31;
          big.setPixel(x, y, qRgb(qMin(255, x * 255 / big.width() + n), qMin(255, y * 255 / big.height() + n), qMin(255, (x + y) * 255 / (big.width() + big.height()) + n)));
        }
    big.save(file);
  }
  txTabs->setCurrentIndex(TRXDRM);
  QSignalSpy ready(tx, &imageViewer::compressionReady);
  Watchdog dog;
  qint64 callMs = 0;
  const auto timedPick = [&](int enc)
  {
    QElapsedTimer t;
    t.start();
    pick(encoding, enc);
    callMs = t.elapsed();
    printf("  (switch to %s took %lld ms)\n", enc == imageViewer::ENCAVIF ? "AVIF" : "JPEG2000", static_cast<long long>(callMs));
    return callMs;
  };
  const auto settled = [&]()
  {
    const bool ended = waitFor([&]() {return !tx->encodePending();}, 90000);
    QTest::qWait(300);
    return ended && !tx->encodePending();
  };
  const int kSlow = 250;   // ms: a switch that returns this fast did not wait for an encode
  const int kSize = 90000;   // big enough that the 1280x960 picture is not scaled down before the encode: a real encode

  printf("== a big picture in DRM mode\n");
  tx->openImage(file, false, true, false, false);   // like File > Open: the viewer tells the TX tab
  check(settled(), "the picture is encoded in the background and settles");
  QByteArray ba;
  const bool gotFirst = tx->copyToBuffer(&ba);
  check(gotFirst && !ba.isEmpty(), QString("it has compressed data (%1 bytes, %2)").arg(ba.size()).arg(tx->compressedExt));

  printf("\n== the codec box: AVIF, then JPEG2000\n");
  if (avif)
    {
      timedPick(imageViewer::ENCAVIF);
      const bool ended = settled();
      check(ended && tx->compressedExt == "avif" && avifSniff(tx->compressedImageData), QString("AVIF is encoded (%1 bytes)").arg(tx->compressedImageData.size()));
    }
  sizeSlider->setValue(kSize);
  settled();
  ready.clear();
  dog.reset();
  timedPick(imageViewer::ENCJPEG2000);
  check(callMs < kSlow, QString("switching to JPEG2000 returns at once (%1 ms)").arg(callMs));
  check(tx->encodePending(), "and the encode is still going in the background");
  const bool ended = settled();
  check(ended && ready.count() >= 1, QString("it ends with compressionReady() (%1 signal(s))").arg(ready.count()));
  check(dog.maxGap < kSlow, QString("the event loop was never blocked for long (%1 ms)").arg(dog.maxGap));
  ba = tx->compressedImageData;
  check(tx->compressedExt == "jp2" && isJp2(ba), QString("the data is JPEG2000 (%1 bytes)").arg(ba.size()));
  check(ba.size() > kSize / 2 && ba.size() < kSize * 3 / 2, QString("and near the %1 bytes asked for (%2)").arg(kSize).arg(ba.size()));
  check(!tx->displayedImage.isNull(), "the picture shows the decoded result");

  printf("\n== the codec box switched back and forth quickly\n");
  {
    qint64 worst = 0;
    for (int i = 0; i < 4; i++)
      {
        timedPick((i % 2 == 0 && avif) ? imageViewer::ENCAVIF : imageViewer::ENCJPEG2000);
        worst = qMax(worst, callMs);
        QTest::qWait(20);
      }
    timedPick(imageViewer::ENCJPEG2000);
    worst = qMax(worst, callMs);
    check(worst < kSlow, QString("no switch waited for an encode (worst %1 ms)").arg(worst));
    check(settled(), "the encodes settle");
    ba = tx->compressedImageData;
    check(tx->compressedExt == "jp2" && isJp2(ba) && ba.size() > kSize / 2 && ba.size() < kSize * 3 / 2,
          QString("the last choice wins: JPEG2000, %1 bytes").arg(ba.size()));
    check(waitFor([&]() {return leftovers(tmp).isEmpty();}, 30000), QString("no temp file is left behind (%1)").arg(leftovers(tmp).join(" ")));
  }

  printf("\n== the size slider with JPEG2000\n");
  {
    qint64 worst = 0;
    ready.clear();
    for (int v : {50000, 60000, 70000, 80000, 100000})
      {
        QElapsedTimer t;
        t.start();
        sizeSlider->setValue(v);
        worst = qMax(worst, t.elapsed());
        QTest::qWait(30);
      }
    check(worst < kSlow, QString("no slider move waited for an encode (worst %1 ms)").arg(worst));
    check(settled(), "the encodes settle");
    ba = tx->compressedImageData;
    check(tx->compressedExt == "jp2" && isJp2(ba) && ba.size() > 60000 && ba.size() < 150000,
          QString("the last size wins: 100000 asked, %1 bytes").arg(ba.size()));
    check(waitFor([&]() {return leftovers(tmp).isEmpty();}, 30000), QString("no temp file is left behind (%1)").arg(leftovers(tmp).join(" ")));
  }

  printf("\n== the data is asked for while an encode runs (Start does this)\n");
  {
    sizeSlider->setValue(kSize);
    settled();
    if (avif)
      {
        timedPick(imageViewer::ENCAVIF);
        QByteArray got;
        const bool ok = tx->copyToBuffer(&got);
        check(ok && avifSniff(got), QString("a switch to AVIF, data asked for at once: AVIF (%1 bytes)").arg(got.size()));
      }
    timedPick(imageViewer::ENCJPEG2000);
    QByteArray got;
    const bool ok = tx->copyToBuffer(&got);
    check(ok && isJp2(got) && got.size() > kSize / 2 && got.size() < kSize * 3 / 2,
          QString("a switch to JPEG2000, data asked for at once: JPEG2000 (%1 bytes)").arg(got.size()));
    check(settled() && waitFor([&]() {return leftovers(tmp).isEmpty();}, 30000), "nothing is left running or on disk");
  }
  finish();
}
