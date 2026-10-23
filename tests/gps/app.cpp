// Drives the GPS inside the REAL application objects (main window, TX tab, Configuration > GPS), offscreen, with a fake GPS
// receiver on a pseudo terminal:
//   app <scratch-dir> <fixtures-dir>
// Built and run by tests/gps/app.sh against an existing build-cmake tree (it links the application's object files).
#include <QtTest>
#include <QtWidgets>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include "appglobal.h"
#include "configparams.h"
#include "configdialog.h"
#include "dispatcher.h"
#include "editorsetup.h"
#include "gallerywidget.h"
#include "imageviewer.h"
#include "mainwindow.h"
#include "templatedoc.h"
#include "nmeaparser.h"
#include "rigcontrol.h"
#include "sstvparam.h"
#include "txfunctions.h"
#include "drmtransmitter.h"
#include "ui_txwidget.h"
#include "ftpfunctions.h"
#include "beaconcapture.h"
// the beacon state machine is private; the test steps it without a camera
#define private public
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

// waits for the condition, then reports it with a description that is made after the wait (so it can show the final state)
template <typename C, typename W> static void checkWait(C cond, W what, int ms = 6000)
{
  const bool ok = waitFor(cond, ms);
  check(ok, what());
}

static QByteArray nmea(const QByteArray &body)
{
  int sum = 0;
  for (char c : body) sum ^= static_cast<unsigned char>(c);
  return "$" + body + QByteArray("*") + QByteArray::number(sum, 16).rightJustified(2, '0').toUpper() + "\r\n";
}

int main(int argc, char **argv)
{
  const QString scratch = argv[1];
  qputenv("HOME", scratch.toLocal8Bit());
  qputenv("XDG_CONFIG_HOME", (scratch + "/config").toLocal8Bit());
  qputenv("XDG_DATA_HOME", (scratch + "/data").toLocal8Bit());
  qputenv("QT_QPA_PLATFORM", "offscreen");
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
  mainWindowPtr->startRunning(false);   // no sound card receiver
  QTest::qWait(300);

  // the fake receiver: a pseudo terminal that repeats one sentence while "feeding" is on
  const int master = ::posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (master < 0 || ::grantpt(master) < 0 || ::unlockpt(master) < 0) {printf("no pty\n"); return 2;}
  const QString slave = QString::fromLocal8Bit(::ptsname(master));
  bool feeding = false;
  QByteArray sentence = nmea("GPGGA,123519,4736.372,N,12219.926,W,1,08,0.9,545.4,M,46.9,M,,");
  QTimer feeder;
  QObject::connect(&feeder, &QTimer::timeout, [&] {if (feeding && ::write(master, sentence.constData(), sentence.size()) < 0) {}});
  feeder.start(100);
  // message boxes (a failure of the Get GPS button) would block the run: count and close them
  int boxes = 0;
  QString lastBox;
  QTimer closer;
  QObject::connect(&closer, &QTimer::timeout, [&] {
    if (QWidget *w = QApplication::activeModalWidget())
      {
        boxes++;
        if (QMessageBox *mb = qobject_cast<QMessageBox *>(w)) lastBox = mb->text();
        w->close();
      }
  });
  closer.start(100);

  txWidget *txw = txWidgetPtr;
  imageViewer *tx = txw->getImageViewerPtr();
  QLineEdit *lat = txw->findChild<QLineEdit *>("latLineEdit");
  QLineEdit *lon = txw->findChild<QLineEdit *>("lonLineEdit");
  QToolButton *gpsButton = txw->findChild<QToolButton *>("gpsToolButton");

  printf("== the Transmit tab\n");
  check(lat && lon && gpsButton, "has Lat and Long boxes and a GPS button");
  if (!lat || !lon || !gpsButton) return 1;

  gpsEnabled = true;
  gpsPort = slave;
  gpsBaud = 4800;
  gpsBeaconRefresh = true;

  printf("\n== WA8LMF beacon: the picture waits for the position\n");
  {
    // no picture is loaded, so a beacon that gets as far as sending stops with this error: that proves it carried on
    check(!tx->hasValidImage(), "(no picture on the TX tab)");
    txw->beaconRunning = true;
    txw->beaconError.clear();
    feeding = true;
    check(txw->beaconUsesGps(), "the beacon reads the GPS when it is set up");
    check(txw->requestGps(false), "(the beacon asks for a position)");
    txw->beaconTransmitLoaded();
    check(txw->beaconWaitGps && txw->beaconError.isEmpty(), "while the position is being read, the beacon waits");
    check(waitFor([&] {return !txw->beaconWaitGps;}), "and goes on when it arrives");
    check(lat->text() == "47.60620" && lon->text() == "-122.33210", "with the position in the boxes (" + lat->text() + ", " + lon->text() + ")");
    check(txw->beaconError.contains("no valid picture"), "and the beacon continued: " + txw->beaconError);
    txw->stopBeacon();

    // no position: the beacon is not held up and the old position stays
    gpsPort = "/dev/does-not-exist-gps";
    txw->beaconRunning = true;
    txw->beaconError.clear();
    check(txw->requestGps(false), "(the beacon asks again, the port is gone)");
    txw->beaconTransmitLoaded();
    check(txw->beaconWaitGps, "the beacon waits again");
    check(waitFor([&] {return !txw->beaconWaitGps;}), "and goes on even though the GPS failed");
    check(txw->beaconError.contains("no valid picture"), "(it did continue)");
    check(lat->text() == "47.60620" && boxes == 0, "the last position stays and no message box blocks an unattended beacon");
    txw->stopBeacon();

    // the option off: nothing is asked
    gpsPort = slave;
    gpsBeaconRefresh = false;
    check(!txw->beaconUsesGps(), "with the option off the beacon does not use the GPS");
    gpsBeaconRefresh = true;
    lat->setText("");
    lon->setText("");
    txw->slotGetParams();
  }

  printf("\n== template macros %%a and %%b\n");
  {
    tpl::TemplateDocument d;
    d.canvas = QRectF(0, 0, 320, 256);
    tpl::Element e;
    e.type = tpl::ItemType::Text;
    e.text = "%a %b";
    e.pos = QPointF(10, 120);
    e.font.pointSize = 36;
    e.fill = Qt::white;
    d.items << e;
    QString err;
    QDir().mkpath(templatesPath);
    check(tpl::saveFile(templatesPath + "/gpstest.qtempl", d, &err), "(template with %a %b saved)");
    galleryWidgetPtr->changedMatrix(imageViewer::TEMPLATETHUMB);
    txw->setupTemplatesComboBox();
    QTest::qWait(200);
    QComboBox *combo = txw->findChild<QComboBox *>("templatesComboBox");
    QCheckBox *useBox = txw->findChild<QCheckBox *>("templateCheckBox");
    check(combo && useBox && combo->findText("gpstest") >= 0, "(the template is in the TX list)");
    if (!combo || !useBox) return 1;
    const QString pics = txStockImagesPath;
    QDir().mkpath(pics);
    QImage base(320, 256, QImage::Format_ARGB32);
    base.fill(QColor(0, 0, 0));
    base.save(pics + "/gps-base.png");
    QString fn = pics + "/gps-base.png";
    tx->openImage(fn, true, false, false, true);
    tx->setFilename(fn);
    useBox->setChecked(true);
    combo->setCurrentIndex(combo->findText("gpstest"));
    txw->slotGetParams();
    auto lit = [](const QImage &im) {
      int n = 0;
      for (int y = 0; y < im.height(); y++)
        for (int x = 0; x < im.width(); x++)
          if (qGray(im.pixel(x, y)) > 128) n++;
      return n;
    };
    const QImage empty = tx->getDisplayedImage()->copy();
    check(lit(empty) < 50, "without a position the macros print nothing (" + QString::number(lit(empty)) + " light pixels)");

    // typed in
    lat->setText("12.5");
    lon->setText("-45.25");
    emit lat->editingFinished();
    check(tx->latitude == "12.5" && tx->longitude == "-45.25", "typed values reach the template values");
    const QImage typed = tx->getDisplayedImage()->copy();
    check(lit(typed) > 300, "and the picture shows them (" + QString::number(lit(typed)) + " light pixels)");
    const QHash<QChar, QString> mv = editorMacroValues();
    check(mv.value('a') == "12.5" && mv.value('b') == "-45.25", "the template editor previews the same values");

    // from the GPS
    feeding = true;
    QTest::mouseClick(gpsButton, Qt::LeftButton);
    check(!gpsButton->isEnabled(), "the GPS button is off while the position is being read");
    checkWait([&] {return lat->text() == "47.60620";}, [&] {return "the GPS button fills in the latitude (" + lat->text() + ")";});
    check(lon->text() == "-122.33210", "and the longitude (" + lon->text() + ")");
    check(waitFor([&] {return gpsButton->isEnabled();}), "and is on again");
    check(tx->latitude == "47.60620" && tx->longitude == "-122.33210", "the template values follow");
    check(*tx->getDisplayedImage() != typed, "and the picture is rendered again");

    // a south-west position is signed
    sentence = nmea("GPGGA,123519,3351.000,S,15112.000,E,1,08,0.9,545.4,M,46.9,M,,");
    QTest::mouseClick(gpsButton, Qt::LeftButton);
    checkWait([&] {return lat->text() == "-33.85000" && lon->text() == "151.20000";}, [&] {return "southern and eastern positions come out signed (" + lat->text() + ", " + lon->text() + ")";});
  }

  printf("\n== failures of the GPS button\n");
  {
    feeding = false;
    boxes = 0;
    gpsPort = "/dev/does-not-exist-gps";
    QTest::mouseClick(gpsButton, Qt::LeftButton);
    checkWait([&] {return boxes > 0;}, [&] {return "an unusable port gives a message: " + lastBox;});
    check(lastBox.contains("Cannot open"), "that names the problem");
    check(lat->text() == "-33.85000" && gpsButton->isEnabled(), "the boxes keep their values and the button is usable again");

    boxes = 0;
    gpsEnabled = false;
    QTest::mouseClick(gpsButton, Qt::LeftButton);
    checkWait([&] {return boxes > 0 && lastBox.contains("not enabled");}, [&] {return "with the GPS disabled the button says so: " + lastBox;});
    gpsEnabled = true;
  }

  printf("\n== the position is remembered\n");
  {
    txw->writeSettings();
    QSettings qs;
    check(qs.value("TX/latitude").toString() == "-33.85000" && qs.value("TX/longitude").toString() == "151.20000", "saved with the TX settings");
    lat->setText("");
    lon->setText("");
    txw->slotGetParams();
    check(tx->latitude.isEmpty(), "(cleared)");
    txw->readSettings();
    check(lat->text() == "-33.85000" && lon->text() == "151.20000" && tx->latitude == "-33.85000", "and restored at the next start");
  }

  printf("\n== Configuration > GPS\n");
  {
    QWidget *page = configDialogPtr->findChild<QWidget *>("gpsWidget");
    check(page != nullptr, "the Configuration dialog has a GPS page");
    if (page)
      {
        QComboBox *portBox = page->findChild<QComboBox *>("portComboBox");
        QComboBox *baudBox = page->findChild<QComboBox *>("baudComboBox");
        QCheckBox *enableBox = page->findChild<QCheckBox *>("enableCheckBox");
        QCheckBox *refreshBox = page->findChild<QCheckBox *>("beaconRefreshCheckBox");
        QPushButton *testButton = page->findChild<QPushButton *>("testPushButton");
        QLabel *result = page->findChild<QLabel *>("resultLabel");
        check(portBox && baudBox && enableBox && refreshBox && testButton && result, "with port, baud, enable, beacon option, Test button and result");
        if (portBox && baudBox && enableBox && refreshBox && testButton && result)
          {
            // settings round trip: the page holds the values, writeSettings() saves them, readSettings() loads them
            QSettings qs;
            enableBox->setChecked(true);
            portBox->setCurrentText(slave);
            baudBox->setCurrentText("9600");
            refreshBox->setChecked(false);
            configDialogPtr->writeSettings();
            check(qs.value("GPS/enabled").toBool() && qs.value("GPS/port").toString() == slave && qs.value("GPS/baud").toInt() == 9600
                  && qs.value("GPS/beaconRefresh").toBool() == false, "the settings are written");
            check(gpsEnabled && gpsPort == slave && gpsBaud == 9600 && !gpsBeaconRefresh, "and are what the program uses");
            gpsEnabled = false;
            gpsPort.clear();
            gpsBaud = 0;
            gpsBeaconRefresh = true;
            configDialogPtr->readSettings();
            check(gpsEnabled && gpsPort == slave && gpsBaud == 9600 && !gpsBeaconRefresh, "and read back");
            check(portBox->currentText() == slave && baudBox->currentText() == "9600" && enableBox->isChecked() && !refreshBox->isChecked(), "and shown on the page");
            gpsBeaconRefresh = true;

            // Test uses the values on the page, leaves the TX tab alone
            lat->setText("1.0");
            txw->slotGetParams();
            portBox->setCurrentText(slave);
            feeding = true;
            sentence = nmea("GPGGA,123519,4736.372,N,12219.926,W,1,08,0.9,545.4,M,46.9,M,,");
            QTest::mouseClick(testButton, Qt::LeftButton);
            checkWait([&] {return result->text().startsWith("Position:");}, [&] {return "Test shows the position: " + result->text();});
            check(result->text().contains("47.60620") && result->text().contains("-122.33210"), "with the coordinates");
            check(lat->text() == "1.0" && tx->latitude == "1.0", "and does not touch the Lat and Long of the TX tab");
            check(testButton->isEnabled(), "the Test button is usable again");

            portBox->setCurrentText("/dev/does-not-exist-gps");
            QTest::mouseClick(testButton, Qt::LeftButton);
            checkWait([&] {return result->text().contains("Cannot open");}, [&] {return "Test explains a port that cannot be opened: " + result->text();});

            // the GPS may not use the CAT port
            rigControllerPtr->params()->enableCAT = true;
            rigControllerPtr->params()->serialPort = slave;
            portBox->setCurrentText(slave);
            enableBox->setChecked(false);
            boxes = 0;
            QTest::mouseClick(enableBox, Qt::LeftButton);
            checkWait([&] {return boxes > 0 && !enableBox->isChecked();}, [&] {return "the GPS cannot be enabled on the CAT port: " + lastBox;});
            rigControllerPtr->params()->enableCAT = false;
          }
      }
  }

  printf("\n%d passed, %d failed\n", passed, failed);
  fflush(stdout);
  _exit(failed ? 1 : 0);   // the application's threads are not shut down by this probe
}
