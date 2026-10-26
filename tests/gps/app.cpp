// Drives the GPS inside the REAL application objects (main window, TX tab, Configuration > GPS), offscreen, with a fake GPS
// receiver on a pseudo terminal and a fake web server in place of the place service of OpenStreetMap:
//   app <scratch-dir> <fixtures-dir>
// Built and run by tests/gps/app.sh against an existing build-cmake tree (it links the application's object files).
#include <QtTest>
#include <QtWidgets>
#include <QSerialPortInfo>
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
#include "placelookup.h"
#include "fakeservice.h"
#include "rigcontrol.h"
#include "serialports.h"
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

  fakeService fake;
  placeLookupPtr->setServiceUrl(fake.url());

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
    check(txw->beaconWaitPosition && txw->beaconError.isEmpty(), "while the position is being read, the beacon waits");
    check(waitFor([&] {return !txw->beaconWaitPosition;}), "and goes on when it arrives");
    check(lat->text() == "47.60620" && lon->text() == "-122.33210", "with the position in the boxes (" + lat->text() + ", " + lon->text() + ")");
    check(txw->beaconError.contains("no valid picture"), "and the beacon continued: " + txw->beaconError);
    txw->stopBeacon();

    // no position: the beacon is not held up and the old position stays
    gpsPort = "/dev/does-not-exist-gps";
    txw->beaconRunning = true;
    txw->beaconError.clear();
    check(txw->requestGps(false), "(the beacon asks again, the port is gone)");
    txw->beaconTransmitLoaded();
    check(txw->beaconWaitPosition, "the beacon waits again");
    check(waitFor([&] {return !txw->beaconWaitPosition;}), "and goes on even though the GPS failed");
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

  printf("\n== city and state (%%g)\n");
  {
    QLineEdit *place = txw->findChild<QLineEdit *>("placeLineEdit");
    check(place != nullptr, "the Transmit tab has a Place box");
    if (!place) return 1;
    gpsPort = slave;
    gpsEnabled = true;
    gpsPlaceLookup = false;
    place->clear();
    txw->slotGetParams();
    fake.body = seattle;
    feeding = true;
    sentence = nmea("GPGGA,123519,4736.372,N,12219.926,W,1,08,0.9,545.4,M,46.9,M,,");

    // off: the position stays on the computer
    QTest::mouseClick(gpsButton, Qt::LeftButton);
    checkWait([&] {return lat->text() == "47.60620" && gpsButton->isEnabled();}, [&] {return "(the GPS button reads the position, " + lat->text() + ")";});
    QTest::qWait(300);
    check(fake.hits == 0 && place->text().isEmpty(), "with the lookup off nothing is asked and the Place box stays empty");

    // a template that prints the place
    tpl::TemplateDocument d;
    d.canvas = QRectF(0, 0, 320, 256);
    tpl::Element e;
    e.type = tpl::ItemType::Text;
    e.text = "%g";
    e.pos = QPointF(10, 120);
    e.font.pointSize = 36;
    e.fill = Qt::white;
    d.items << e;
    QString err;
    check(tpl::saveFile(templatesPath + "/gpsplace.qtempl", d, &err), "(template with %g saved)");
    galleryWidgetPtr->changedMatrix(imageViewer::TEMPLATETHUMB);
    txw->setupTemplatesComboBox();
    QTest::qWait(200);
    QComboBox *combo = txw->findChild<QComboBox *>("templatesComboBox");
    check(combo && combo->findText("gpsplace") >= 0, "(the template is in the TX list)");
    if (!combo) return 1;
    combo->setCurrentIndex(combo->findText("gpsplace"));
    txw->slotGetParams();
    auto lit = [](const QImage &im) {
      int n = 0;
      for (int y = 0; y < im.height(); y++)
        for (int x = 0; x < im.width(); x++)
          if (qGray(im.pixel(x, y)) > 128) n++;
      return n;
    };
    check(lit(*tx->getDisplayedImage()) < 50, "without a place the macro prints nothing");

    // on: the GPS button finds the place of the position
    gpsPlaceLookup = true;
    QTest::mouseClick(gpsButton, Qt::LeftButton);
    checkWait([&] {return place->text() == "Seattle, WA";}, [&] {return "the GPS button also fills in the place (" + place->text() + ")";});
    check(fake.hits == 1 && fake.lastRequest.contains("lat=47.60620") && fake.lastRequest.contains("lon=-122.33210"), "that asked the service for the position");
    check(tx->place == "Seattle, WA", "the template value follows");
    check(lit(*tx->getDisplayedImage()) > 300, "and the picture shows it (" + QString::number(lit(*tx->getDisplayedImage())) + " light pixels)");
    check(editorMacroValues().value('g') == "Seattle, WA", "the template editor previews the same value");

    // a place that was typed stays when a coordinate box is left without a change
    place->setText("My Town, XX");
    emit place->editingFinished();
    check(tx->place == "My Town, XX", "a typed place reaches the template value");
    emit lat->editingFinished();
    QTest::qWait(300);
    check(fake.hits == 1 && place->text() == "My Town, XX", "leaving the Lat box unchanged does not look it up again");

    // typed coordinates: the place follows
    fake.body = missoula;
    lat->setText("46.8721");
    lon->setText("-113.9940");
    emit lat->editingFinished();
    checkWait([&] {return place->text() == "Missoula, MT";}, [&] {return "new coordinates typed in bring a new place (" + place->text() + ")";});
    check(fake.hits == 2 && tx->place == "Missoula, MT", "with one request");

    // a lookup that fails keeps the place; typed coordinates give no message, and the next try goes out again
    fake.status = 500;
    boxes = 0;
    lat->setText("40.5");
    lon->setText("-100.5");
    emit lat->editingFinished();
    checkWait([&] {return fake.hits == 3 && !txw->placeAwaiting;}, [&] {return "(the service answers with an error, " + QString::number(fake.hits) + " requests)";});
    QTest::qWait(200);
    check(place->text() == "Missoula, MT" && boxes == 0, "a failed lookup keeps the place and does not interrupt with a message box");
    fake.status = 200;
    fake.body = seattle;
    emit lon->editingFinished();
    checkWait([&] {return place->text() == "Seattle, WA";}, [&] {return "leaving a box again tries the lookup again (" + place->text() + ")";});

    // the GPS button says what went wrong
    fake.status = 500;
    sentence = nmea("GPGGA,123519,3351.000,S,15112.000,E,1,08,0.9,545.4,M,46.9,M,,");
    boxes = 0;
    QTest::mouseClick(gpsButton, Qt::LeftButton);
    checkWait([&] {return boxes > 0;}, [&] {return "the GPS button reports a failed lookup: " + lastBox;});
    check(lastBox.contains("500") && lat->text() == "-33.85000" && place->text() == "Seattle, WA", "with the position taken and the place kept");
    fake.status = 200;

    // the beacon sends the picture after the place has arrived (the TX tab holds a picture by now: marked as not valid, a beacon that
    // gets as far as sending stops with an error instead of transmitting)
    tx->setValidImage(false);
    fake.body = missoula;
    fake.delayMs = 700;
    sentence = nmea("GPGGA,123519,4736.372,N,12219.926,W,1,08,0.9,545.4,M,46.9,M,,");
    QString shown;
    QObject::connect(txw, &txWidget::beaconCountdownChanged, txw, [&](const QString &text, bool) {shown = text;});
    txw->beaconRunning = true;
    txw->beaconError.clear();
    check(txw->requestGps(false), "(the beacon asks for a position)");
    txw->beaconTransmitLoaded();
    check(txw->beaconWaitPosition, "the beacon waits for the position");
    checkWait([&] {return txw->placeAwaiting;}, [&] {return QString("(the position is in, the place is being looked up)");});
    txw->slotBeaconTick();
    check(txw->beaconWaitPosition && txw->beaconError.isEmpty() && shown.contains("looking up the place"), "and then for the place: " + shown);
    checkWait([&] {return !txw->beaconWaitPosition;}, [&] {return QString("the beacon goes on");});
    check(place->text() == "Missoula, MT" && txw->beaconError.contains("no valid picture"), "once the place is in the picture (" + place->text() + ")");
    txw->stopBeacon();
    fake.delayMs = 0;

    // a lookup that fails never holds the beacon back
    fake.status = 500;
    sentence = nmea("GPGGA,123519,3351.000,S,15112.000,E,1,08,0.9,545.4,M,46.9,M,,");
    boxes = 0;
    txw->beaconRunning = true;
    txw->beaconError.clear();
    check(txw->requestGps(false), "(the beacon asks again, the service is failing)");
    txw->beaconTransmitLoaded();
    checkWait([&] {return !txw->beaconWaitPosition;}, [&] {return QString("the beacon goes on without a place");});
    check(txw->beaconError.contains("no valid picture") && place->text() == "Missoula, MT" && boxes == 0, "keeping the last place and showing no message box");
    txw->stopBeacon();
    fake.status = 200;
  }

  printf("\n== the position is remembered\n");
  {
    txw->writeSettings();
    QSettings qs;
    check(qs.value("TX/latitude").toString() == "-33.85000" && qs.value("TX/longitude").toString() == "151.20000", "saved with the TX settings");
    check(qs.value("TX/place").toString() == "Missoula, MT", "with the place");
    lat->setText("");
    lon->setText("");
    txw->findChild<QLineEdit *>("placeLineEdit")->setText("");
    txw->slotGetParams();
    check(tx->latitude.isEmpty() && tx->place.isEmpty(), "(cleared)");
    txw->readSettings();
    check(lat->text() == "-33.85000" && lon->text() == "151.20000" && tx->latitude == "-33.85000", "and restored at the next start");
    check(tx->place == "Missoula, MT" && txw->findChild<QLineEdit *>("placeLineEdit")->text() == "Missoula, MT", "also the place");
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
        QCheckBox *placeBox = page->findChild<QCheckBox *>("placeCheckBox");
        QPushButton *testButton = page->findChild<QPushButton *>("testPushButton");
        QLabel *result = page->findChild<QLabel *>("resultLabel");
        check(portBox && baudBox && enableBox && refreshBox && placeBox && testButton && result, "with port, baud, enable, beacon option, place lookup, Test button and result");
        if (portBox && baudBox && enableBox && refreshBox && placeBox && testButton && result)
          {
            // settings round trip: the page holds the values, writeSettings() saves them, readSettings() loads them
            QSettings qs;
            enableBox->setChecked(true);
            portBox->setCurrentText(slave);
            baudBox->setCurrentText("9600");
            refreshBox->setChecked(false);
            placeBox->setChecked(true);
            configDialogPtr->writeSettings();
            check(qs.value("GPS/enabled").toBool() && qs.value("GPS/port").toString() == slave && qs.value("GPS/baud").toInt() == 9600
                  && qs.value("GPS/beaconRefresh").toBool() == false && qs.value("GPS/placeLookup").toBool() == true, "the settings are written");
            check(gpsEnabled && gpsPort == slave && gpsBaud == 9600 && !gpsBeaconRefresh && gpsPlaceLookup, "and are what the program uses");
            gpsEnabled = false;
            gpsPort.clear();
            gpsBaud = 0;
            gpsBeaconRefresh = true;
            gpsPlaceLookup = false;
            configDialogPtr->readSettings();
            check(gpsEnabled && gpsPort == slave && gpsBaud == 9600 && !gpsBeaconRefresh && gpsPlaceLookup, "and read back");
            check(portBox->currentText() == slave && baudBox->currentText() == "9600" && enableBox->isChecked() && !refreshBox->isChecked() && placeBox->isChecked(), "and shown on the page");
            gpsBeaconRefresh = true;

            // Test uses the values on the page, leaves the TX tab alone
            lat->setText("1.0");
            txw->slotGetParams();
            portBox->setCurrentText(slave);
            feeding = true;
            sentence = nmea("GPGGA,123519,4736.372,N,12219.926,W,1,08,0.9,545.4,M,46.9,M,,");
            placeBox->setChecked(false);
            const int hitsBefore = fake.hits;
            QTest::mouseClick(testButton, Qt::LeftButton);
            checkWait([&] {return result->text().startsWith("Position:");}, [&] {return "Test shows the position: " + result->text();});
            check(result->text().contains("47.60620") && result->text().contains("-122.33210"), "with the coordinates");
            check(lat->text() == "1.0" && tx->latitude == "1.0", "and does not touch the Lat and Long of the TX tab");
            check(testButton->isEnabled(), "the Test button is usable again");
            check(fake.hits == hitsBefore, "(no place was asked for: the lookup was not on when the position came)");

            // with the lookup checked on the page the Test button also shows the place, whatever is saved
            placeBox->setChecked(true);
            gpsPlaceLookup = false;
            fake.body = seattle;
            sentence = nmea("GPGGA,123519,3000.000,N,09000.000,W,1,08,0.9,545.4,M,46.9,M,,");
            const QString placeBefore = tx->place;
            QTest::mouseClick(testButton, Qt::LeftButton);
            checkWait([&] {return result->text().contains("Place:") || result->text().contains("busy");}, [&] {return "Test also shows the place: " + result->text();});
            check(result->text().contains("30.00000") && result->text().contains("Place: Seattle, WA"), "next to the position");
            check(tx->place == placeBefore, "and does not change the Place of the TX tab");
            check(testButton->isEnabled(), "(the Test button is usable again)");

            fake.status = 500;
            sentence = nmea("GPGGA,123519,2000.000,N,08000.000,W,1,08,0.9,545.4,M,46.9,M,,");
            QTest::mouseClick(testButton, Qt::LeftButton);
            checkWait([&] {return result->text().contains("500");}, [&] {return "a failing lookup is explained under the position: " + result->text();});
            check(result->text().contains("20.00000") && testButton->isEnabled(), "(the position is still shown)");
            fake.status = 200;

            portBox->setCurrentText("/dev/does-not-exist-gps");
            QTest::mouseClick(testButton, Qt::LeftButton);
            checkWait([&] {return result->text().contains("Cannot open");}, [&] {return "Test explains a port that cannot be opened: " + result->text();});

            // the list of ports: on Linux the by-id names
            {
              QTemporaryDir dir;
              check(dir.isValid(), "(a scratch folder for fake /dev/serial/by-id links)");
              QFile::link(slave, dir.path() + "/usb-Fake_GPS_0001-if00-port0");
              QFile::link("/dev/does-not-exist-gps", dir.path() + "/usb-Broken-if00-port0");
              const QList<serialPortChoice> list = serialPortList(dir.path());
              check(list.size() == 2 && list.at(0).name == dir.path() + "/usb-Broken-if00-port0" && list.at(1).name == dir.path() + "/usb-Fake_GPS_0001-if00-port0",
                    "the ports offered are the links of the by-id folder, sorted (" + QString::number(list.size()) + " found)");
              check(list.size() == 2 && list.at(1).info == QFileInfo(slave).absoluteFilePath(), "each with the device it points to as its hint");
              QTemporaryDir empty;
              const QList<serialPortChoice> fallback = serialPortList(empty.path());
              check(fallback.size() == QSerialPortInfo::availablePorts().size(), "without by-id links the ports of the system are offered (" + QString::number(fallback.size()) + ")");
              for (const serialPortChoice &p : fallback) if (p.name.startsWith(empty.path())) check(false, "(fallback names a link of the empty folder)");
              check(serialPortList("/nonexistent-folder/by-id").size() == fallback.size(), "and so does a computer without that folder");

              // the page itself lists what the real folder holds: no generic /dev/ttyUSB* names next to the by-id ones
              const QList<serialPortChoice> real = serialPortList();
              QStringList wanted;
              for (const serialPortChoice &p : real) wanted << p.name;
              QMetaObject::invokeMethod(page, "slotRescanPorts");
              QStringList shown;
              for (int i = 0; i < portBox->count(); i++) shown << portBox->itemText(i);
              check(shown == wanted, "Rescan fills the list with them (" + QString::number(shown.size()) + " ports)");
              if (QDir("/dev/serial/by-id").exists() && !QDir("/dev/serial/by-id").isEmpty(QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot))
                check(!shown.isEmpty() && shown.first().startsWith("/dev/serial/by-id/") && !shown.contains("/dev/ttyUSB0"), "this computer has by-id ports: those are listed, not /dev/ttyUSB0");

              // the same adapter under two names is still one port
              check(samePort(slave, dir.path() + "/usb-Fake_GPS_0001-if00-port0"), "a by-id link and its device are the same port");
              check(!samePort(slave, "/dev/null") && samePort("COM1", "com1") && !samePort("COM1", "COM2"), "other ports and plain names are told apart");
              rigControllerPtr->params()->enableCAT = true;
              rigControllerPtr->params()->serialPort = slave;
              portBox->setCurrentText(dir.path() + "/usb-Fake_GPS_0001-if00-port0");
              enableBox->setChecked(false);
              boxes = 0;
              QTest::mouseClick(enableBox, Qt::LeftButton);
              checkWait([&] {return boxes > 0 && !enableBox->isChecked();}, [&] {return "the GPS cannot be enabled on a by-id link of the CAT port: " + lastBox;});
              rigControllerPtr->params()->enableCAT = false;
            }

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

  printf("\n== Configuration > CAT: the CAT and PTT serial ports\n");
  {
    rigConfig *rig = configDialogPtr->findChild<rigConfig *>("catWidget");
    check(rig != nullptr, "the Configuration dialog has a CAT page");
    QComboBox *catBox = rig ? rig->findChild<QComboBox *>("serialPortComboBox") : nullptr;
    QComboBox *pttBox = rig ? rig->findChild<QComboBox *>("pttSerialPortComboBox") : nullptr;
    QToolButton *catRescan = rig ? rig->findChild<QToolButton *>("rescanToolButton") : nullptr;
    QToolButton *pttRescan = rig ? rig->findChild<QToolButton *>("pttRescanToolButton") : nullptr;
    QCheckBox *catEnable = rig ? rig->findChild<QCheckBox *>("enableCATCheckBox") : nullptr;
    QCheckBox *pttEnable = rig ? rig->findChild<QCheckBox *>("enablePTTCheckBox") : nullptr;
    check(catBox && pttBox && catRescan && pttRescan && catEnable && pttEnable, "with a CAT port and a PTT port list, a Rescan button for each and the two enable boxes");
    if (catBox && pttBox && catRescan && pttRescan && catEnable && pttEnable)
      {
        check(catBox->isEditable() && pttBox->isEditable(), "both lists accept a name that is not in them");
        scatParams *cp = rigControllerPtr->params();
        QTemporaryDir dir;
        check(dir.isValid(), "(a scratch folder for fake /dev/serial/by-id links)");
        const QString fakeLink = dir.path() + "/usb-Fake_Rig_0001-if00-port0";
        const QString brokenLink = dir.path() + "/usb-Broken-if00-port0";
        QFile::link(slave, fakeLink);
        QFile::link("/dev/does-not-exist-rig", brokenLink);

        // the list: the by-id links, each with the device it points to as its hint, for the CAT and the PTT port alike
        for (QComboBox *box : {catBox, pttBox})
          {
            fillPortComboBox(box, dir.path());
            QStringList shown;
            for (int i = 0; i < box->count(); i++) shown << box->itemText(i);
            check(shown == QStringList({brokenLink, fakeLink}), QString("%1 offers the links of the by-id folder, sorted").arg(box->objectName()));
            check(box->count() == 2 && box->itemData(1, Qt::ToolTipRole).toString() == QFileInfo(slave).absoluteFilePath(), "each with the device it points to as its hint");
            box->setCurrentText("localhost:4532");
            fillPortComboBox(box, dir.path());
            check(box->currentText() == "localhost:4532", "a host of a rig server that was typed stays when the list is filled again");
          }

        // a long name in the list must not make the page, and with it the whole Configuration dialog, wider
        {
          QComboBox *gpsBox = configDialogPtr->findChild<QWidget *>("gpsWidget")->findChild<QComboBox *>("portComboBox");
          const QString longLink = dir.path() + "/usb-" + QString(120, 'X') + "-if00-port0";
          QFile::link(slave, longLink);
          for (QComboBox *box : {catBox, pttBox, gpsBox})
            {
              fillPortComboBox(box, dir.path());
              check(box->count() == 3 && box->minimumSizeHint().width() < 300 && box->sizeHint().width() < 300,
                    QString("%1 stays narrow with a %2 character name in the list (%3 px)").arg(box->objectName()).arg(longLink.size()).arg(box->minimumSizeHint().width()));
            }
          QFile::remove(longLink);
        }

        // Rescan on either button fills both lists with what the real folder holds
        const QList<serialPortChoice> real = serialPortList();
        QStringList wanted;
        for (const serialPortChoice &p : real) wanted << p.name;
        for (QToolButton *button : {catRescan, pttRescan})
          {
            catBox->clear();
            pttBox->clear();
            QTest::mouseClick(button, Qt::LeftButton);
            QStringList catShown, pttShown;
            for (int i = 0; i < catBox->count(); i++) catShown << catBox->itemText(i);
            for (int i = 0; i < pttBox->count(); i++) pttShown << pttBox->itemText(i);
            check(catShown == wanted && pttShown == wanted, QString("%1 fills both lists with the ports of this computer (%2)").arg(button->objectName()).arg(wanted.size()));
          }
        if (QDir("/dev/serial/by-id").exists() && !QDir("/dev/serial/by-id").isEmpty(QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot))
          check(!wanted.isEmpty() && wanted.first().startsWith("/dev/serial/by-id/") && !wanted.contains("/dev/ttyUSB0"), "this computer has by-id ports: those are listed, not /dev/ttyUSB0");

        // settings round trip
        QSettings qs;
        catEnable->setChecked(false);
        pttEnable->setChecked(false);
        catBox->setCurrentText(fakeLink);
        pttBox->setCurrentText("  " + brokenLink + " ");
        configDialogPtr->writeSettings();
        check(cp->serialPort == fakeLink && cp->pttSerialPort == brokenLink, "the CAT and PTT ports are what the program uses (spaces around a name are dropped)");
        cp->serialPort.clear();
        cp->pttSerialPort.clear();
        catBox->setCurrentText("");
        pttBox->setCurrentText("");
        configDialogPtr->readSettings();
        check(cp->serialPort == fakeLink && cp->pttSerialPort == brokenLink, "and are read back");
        check(catBox->currentText() == fakeLink && pttBox->currentText() == brokenLink, "and shown on the page");

        catBox->setCurrentText("localhost:4532");
        configDialogPtr->writeSettings();
        configDialogPtr->readSettings();
        check(cp->serialPort == "localhost:4532" && catBox->currentText() == "localhost:4532", "a host:port for a rig server is kept as it was typed");

        // the PTT port opens through a by-id link (the real code path of the serial PTT; the device is a pseudo terminal)
        {
          cp->enableSerialPTT = true;
          cp->activeRTS = true;
          cp->pttSerialPort = brokenLink;
          boxes = 0;
          rigControllerPtr->activatePTT(true);
          check(boxes > 0 && lastBox.contains("Unable to open serial port"), "a link to a device that is not there is reported: " + lastBox.section('\n', 0, 0));
          cp->pttSerialPort = fakeLink;
          boxes = 0;
          rigControllerPtr->activatePTT(true);
          QTest::qWait(300);
          check(boxes == 0, "a link to a device that is there opens it without a message");
          rigControllerPtr->activatePTT(false);
          cp->enableSerialPTT = false;
          cp->activeRTS = false;
        }

        // one port cannot be both: a by-id link of the CAT port is the CAT port
        catBox->setCurrentText(slave);
        pttBox->setCurrentText(fakeLink);
        catEnable->setChecked(true);   // (setChecked does not click: the rig is not started)
        pttEnable->setChecked(false);
        boxes = 0;
        QTest::mouseClick(pttEnable, Qt::LeftButton);
        checkWait([&] {return boxes > 0 && !pttEnable->isChecked();}, [&] {return "the PTT port cannot be a by-id link of the CAT port: " + lastBox;});
        pttBox->setCurrentText("/dev/null");
        boxes = 0;
        QTest::mouseClick(pttEnable, Qt::LeftButton);
        QTest::qWait(300);
        check(boxes == 0 && pttEnable->isChecked(), "another port is accepted");
        pttEnable->setChecked(false);

        // a saved pair that is the same port twice does not start with both on
        cp->enableCAT = true;
        cp->enableSerialPTT = true;
        cp->serialPort = slave;
        cp->pttSerialPort = fakeLink;
        rig->setParams();
        check(!cp->enableSerialPTT && !pttEnable->isChecked(), "a saved CAT and PTT port that are the same device start with the PTT port off");
        cp->enableCAT = false;
        cp->enableSerialPTT = false;
        catEnable->setChecked(false);
        rig->getParams();
      }
  }

  printf("\n%d passed, %d failed\n", passed, failed);
  fflush(stdout);
  _exit(failed ? 1 : 0);   // the application's threads are not shut down by this probe
}
