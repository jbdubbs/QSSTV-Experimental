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
#include "supportfunctions.h"
#include "gpsreader.h"
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
  setvbuf(stdout, nullptr, _IOLBF, 0);   // a run that hangs shows where
  const QString scratch = argv[1];
  qputenv("HOME", scratch.toLocal8Bit());
  qputenv("XDG_CONFIG_HOME", (scratch + "/config").toLocal8Bit());
  qputenv("XDG_DATA_HOME", (scratch + "/data").toLocal8Bit());
  qputenv("QT_QPA_PLATFORM", "offscreen");
  // no sound server: a Start that really transmits must not play SSTV through the speakers or an audio interface of the rig, so
  // the transmitter finds no sound output and stops after the picture has been handed to it
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
  txw->gpsTracking = false;   // the GPS button is off
  txw->updateGpsButton();
  gpsReaderPtr->setTrackTimeouts(1200, 500);   // a silent receiver is noticed after 1.2 s and a port that failed is tried again every 0.5 s (5 s each in the program)
  // the GPS button is a switch (#158): a quick on and off reads the position once. The Transmit tab's slot is connected first, so the boxes are
  // updated when this counter goes up.
  int positionsSeen = 0;
  const QMetaObject::Connection positionConn = QObject::connect(gpsReaderPtr, &gpsReader::positionUpdated, [&](double, double) {positionsSeen++;});
  auto followOnce = [&] {
    const int n = positionsSeen;
    if (!txw->gpsTracking) QTest::mouseClick(gpsButton, Qt::LeftButton);
    waitFor([&] {return positionsSeen > n;});
    if (txw->gpsTracking) QTest::mouseClick(gpsButton, Qt::LeftButton);
  };
  const auto plain = [&] {return !gpsButton->styleSheet().contains("background-color");};
  const auto amber = [&] {return gpsButton->styleSheet().contains("255,170,0");};
  const auto green = [&] {return gpsButton->styleSheet().contains("0,200,0");};

  printf("\n== WA8LMF beacon: the picture never waits for the position (#158)\n");
  {
    // no picture is loaded, so a beacon that gets as far as sending stops with this error: that proves it carried on
    check(!tx->hasValidImage(), "(no picture on the TX tab)");
    // the GPS button is on and the receiver is silent: there is nothing to wait for
    feeding = false;
    boxes = 0;
    QTest::mouseClick(gpsButton, Qt::LeftButton);
    check(txw->gpsTracking && gpsReaderPtr->tracking(), "(the GPS button is on, the receiver is silent)");
    txw->beaconRunning = true;
    txw->beaconError.clear();
    txw->beaconTransmitLoaded();
    check(txw->beaconError.contains("no valid picture"), "the beacon goes on at once, without a position: " + txw->beaconError);
    check(lat->text().isEmpty() && boxes == 0, "(the boxes are empty, and no message box came up)");
    txw->stopBeacon();

    // a position arrives: the boxes follow, and the beacon has it
    feeding = true;
    checkWait([&] {return lat->text() == "47.60620" && lon->text() == "-122.33210";}, [&] {return "(a position arrives: " + lat->text() + ", " + lon->text() + ")";});
    txw->beaconRunning = true;
    txw->beaconError.clear();
    txw->beaconTransmitLoaded();
    check(txw->beaconError.contains("no valid picture") && tx->latitude == "47.60620", "the beacon goes on with the position that is there");
    txw->stopBeacon();

    // the receiver is gone: the beacon does not mind
    gpsPort = "/dev/does-not-exist-gps";
    txw->gpsSettingsChanged();
    txw->beaconRunning = true;
    txw->beaconError.clear();
    txw->beaconTransmitLoaded();
    check(txw->beaconError.contains("no valid picture") && lat->text() == "47.60620" && boxes == 0, "nor when the port cannot be opened: the last position stays and no message box blocks an unattended beacon");
    txw->stopBeacon();

    // the button off again; the boxes are emptied for the next sections
    gpsPort = slave;
    QTest::mouseClick(gpsButton, Qt::LeftButton);
    check(!txw->gpsTracking && !gpsReaderPtr->tracking(), "(the GPS button is off)");
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
    followOnce();   // switched on and off again: one reading
    checkWait([&] {return lat->text() == "47.60620";}, [&] {return "the GPS button fills in the latitude (" + lat->text() + ")";});
    check(lon->text() == "-122.33210", "and the longitude (" + lon->text() + ")");
    check(!txw->gpsTracking && !lat->isReadOnly(), "(the button is off again and the boxes are free)");
    check(tx->latitude == "47.60620" && tx->longitude == "-122.33210", "the template values follow");
    check(*tx->getDisplayedImage() != typed, "and the picture is rendered again");

    // a south-west position is signed
    sentence = nmea("GPGGA,123519,3351.000,S,15112.000,E,1,08,0.9,545.4,M,46.9,M,,");
    followOnce();
    checkWait([&] {return lat->text() == "-33.85000" && lon->text() == "151.20000";}, [&] {return "southern and eastern positions come out signed (" + lat->text() + ", " + lon->text() + ")";});
  }

  printf("\n== a GPS that cannot be read\n");
  {
    feeding = false;
    boxes = 0;
    gpsPort = "/dev/does-not-exist-gps";
    QTest::mouseClick(gpsButton, Qt::LeftButton);
    check(txw->gpsTracking && amber() && statusBarPtr->currentMessage().contains("Cannot open"), "an unusable port: amber, and the status bar names the problem: " + statusBarPtr->currentMessage());
    QTest::qWait(300);
    check(boxes == 0 && lat->text() == "-33.85000", "no message box, and the boxes keep their values");
    QTest::mouseClick(gpsButton, Qt::LeftButton);
    check(!txw->gpsTracking && !gpsReaderPtr->tracking() && plain(), "a click switches it off");

    boxes = 0;
    gpsEnabled = false;
    QTest::mouseClick(gpsButton, Qt::LeftButton);
    checkWait([&] {return boxes > 0 && lastBox.contains("not enabled");}, [&] {return "with the GPS disabled the button says so: " + lastBox;});
    gpsEnabled = true;
    gpsPort = slave;
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
    followOnce();
    checkWait([&] {return lat->text() == "47.60620";}, [&] {return "(the GPS button reads the position, " + lat->text() + ")";});
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
    followOnce();
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

    // followed by the GPS: the place changes only when the position leaves the area of the last lookup (#158)
    txw->placeFailedAt.invalidate();
    fake.status = 200;
    fake.body = seattle;
    place->setText("My Town, XX");
    emit place->editingFinished();
    sentence = nmea("GPGGA,123519,4736.372,N,12219.926,W,1,08,0.9,545.4,M,46.9,M,,");
    int hits = fake.hits;
    QTest::mouseClick(gpsButton, Qt::LeftButton);
    checkWait([&] {return lat->text() == "47.60620" && fake.hits == hits + 1 && !txw->placeAwaiting;}, [&] {return QString("(switched on: the first position asks for the place, %1 requests)").arg(fake.hits - hits);});
    check(place->text() == "Seattle, WA", "and it is in the box (" + place->text() + ")");
    place->setText("My Town, XX");
    emit place->editingFinished();
    sentence = nmea("GPGGA,123519,4736.500,N,12219.900,W,1,08,0.9,545.4,M,46.9,M,,");   // a few hundred metres on
    checkWait([&] {return lat->text() == "47.60833";}, [&] {return "(the receiver moves a little: " + lat->text() + ")";});
    QTest::qWait(2500);
    check(fake.hits == hits + 1 && place->text() == "My Town, XX", "a position in the same area asks nothing and keeps a place that was typed in");
    fake.body = missoula;
    sentence = nmea("GPGGA,123519,4652.326,N,11359.640,W,1,08,0.9,545.4,M,46.9,M,,");   // 46.8721, -113.994: another city
    checkWait([&] {return place->text() == "Missoula, MT";}, [&] {return "a position in another area looks the place up again (" + place->text() + ")";});
    check(fake.hits == hits + 2, "with one request");

    // the service fails: no message box, one line in the status bar, and no new request every second while the receiver moves on
    fake.status = 500;
    boxes = 0;
    statusBarPtr->clearMessage();
    sentence = nmea("GPGGA,123519,4030.000,N,10030.000,W,1,08,0.9,545.4,M,46.9,M,,");
    hits = fake.hits;
    checkWait([&] {return fake.hits == hits + 1 && !txw->placeAwaiting;}, [&] {return QString("(the service answers with an error, %1 requests)").arg(fake.hits - hits);});
    check(place->text() == "Missoula, MT" && boxes == 0, "a failed lookup keeps the place and shows no message box");
    check(statusBarPtr->currentMessage().contains("500") && statusBarPtr->currentMessage().contains("keeping the last place"), "the status bar says why: " + statusBarPtr->currentMessage());
    sentence = nmea("GPGGA,123519,3000.000,N,09000.000,W,1,08,0.9,545.4,M,46.9,M,,");
    checkWait([&] {return lat->text() == "30.00000";}, [&] {return "(the receiver moves on: " + lat->text() + ")";});
    QTest::qWait(2500);
    check(fake.hits == hits + 1, "and the next try waits: no request every second while the service fails (" + QString::number(fake.hits - hits) + " more)");
    fake.status = 200;
    QTest::mouseClick(gpsButton, Qt::LeftButton);
    check(!txw->gpsTracking, "(off)");
    txw->placeFailedAt.invalidate();
    lat->setText("-33.85000");   // the sections after this one expect these boxes
    lon->setText("151.20000");
    txw->slotGetParams();
  }

  // the two sections about the GPS button change the boxes; the section after them expects them as they are now
  const QString keepLat = lat->text(), keepLon = lon->text(), keepPlace = txw->findChild<QLineEdit *>("placeLineEdit")->text();

  printf("\n== the GPS button: one switch, the position follows the GPS (#158)\n");
  {
    // the font of the button as it is drawn now (the style sheet is repolished when the event loop runs)
    const auto fontOf = [&] {
      QTest::qWait(50);
      const QFontInfo fi(gpsButton->font());
      return QString("%1 %2px").arg(fi.family()).arg(fi.pixelSize());
    };
    gpsPort = slave;
    gpsEnabled = false;
    txw->gpsTracking = false;
    txw->updateGpsButton();
    check(plain() && gpsButton->toolTip().contains("off"), "with the GPS off in Configuration the button has no color: " + gpsButton->toolTip());
    boxes = 0;
    QTest::mouseClick(gpsButton, Qt::LeftButton);
    checkWait([&] {return boxes > 0 && lastBox.contains("not enabled");}, [&] {return "and a click says so: " + lastBox;});
    check(!txw->gpsTracking && !gpsReaderPtr->tracking(), "without switching anything on");
    gpsEnabled = true;
    txw->updateGpsButton();   // what the main window does after the Configuration dialog
    check(plain() && gpsButton->toolTip().contains("follow") && !lat->isReadOnly(), "with the GPS set up and the button off it is plain too, never red: " + gpsButton->toolTip());
    check(gpsButton->contextMenuPolicy() != Qt::CustomContextMenu && gpsButton->actions().isEmpty() && gpsButton->menu() == nullptr, "and there is no right-click menu (a touch screen has none)");

    // on, with a receiver that says nothing: amber, and the boxes are the GPS's
    boxes = 0;
    feeding = false;
    QTest::mouseClick(gpsButton, Qt::LeftButton);
    check(txw->gpsTracking && gpsReaderPtr->tracking(), "a click switches it on");
    check(amber() && !green(), "amber while no position arrives: " + gpsButton->toolTip());
    check(lat->isReadOnly() && lon->isReadOnly() && lat->toolTip().contains("GPS"), "Lat and Long cannot be typed in while it is on");
    QTest::keyClicks(lat, "9");
    check(lat->text() != "9" && !lat->text().endsWith("9"), "(a key typed into the box does nothing)");
    checkWait([&] {return statusBarPtr->currentMessage().contains("No GPS data");}, [&] {return "the status bar says why there is no position: " + statusBarPtr->currentMessage();});

    // a position arrives: green, and the boxes follow it without any transmission
    sentence = nmea("GPGGA,123519,4736.372,N,12219.926,W,1,08,0.9,545.4,M,46.9,M,,");
    feeding = true;
    checkWait([&] {return green() && lat->text() == "47.60620";}, [&] {return "a position turns it green and fills the boxes (" + lat->text() + ", " + lon->text() + ")";});
    check(lon->text() == "-122.33210" && tx->latitude == "47.60620" && tx->longitude == "-122.33210", "and the template values follow");
    check(!amber() && gpsButton->toolTip().contains("Following"), "(" + gpsButton->toolTip() + ")");
    // it goes on updating, every second or two
    sentence = nmea("GPGGA,123519,4736.500,N,12219.000,W,1,08,0.9,545.4,M,46.9,M,,");
    QElapsedTimer follow;
    follow.start();
    checkWait([&] {return lat->text() == "47.60833" && lon->text() == "-122.31667";}, [&] {return "a new position follows by itself (" + lat->text() + ", " + lon->text() + ")";}, 3000);
    check(follow.elapsed() < 2500, QString("within a second or two (%1 ms)").arg(follow.elapsed()));
    int seen = positionsSeen;
    QTest::qWait(2100);
    check(positionsSeen - seen >= 2 && positionsSeen - seen <= 3, QString("about one position a second reaches the Transmit tab, not every sentence (%1 in 2.1 s)").arg(positionsSeen - seen));

    // the receiver is lost: amber again, the last position stays; it comes back: green
    feeding = false;
    checkWait([&] {return amber();}, [&] {return QString("when the position stops the button turns amber");}, 4000);
    check(lat->text() == "47.60833", "and the boxes keep the last position");
    feeding = true;
    checkWait([&] {return green();}, [&] {return QString("and green again when it is back");});

    // off: plain, the boxes keep what they have and can be typed in again
    QTest::mouseClick(gpsButton, Qt::LeftButton);
    check(!txw->gpsTracking && !gpsReaderPtr->tracking() && plain(), "the next click switches it off and the button is plain");
    check(!lat->isReadOnly() && !lon->isReadOnly() && lat->toolTip() == txw->latBoxTip && lat->text() == "47.60833", "the boxes can be typed in again and keep their values");
    seen = positionsSeen;
    sentence = nmea("GPGGA,123519,3351.000,S,15112.000,E,1,08,0.9,545.4,M,46.9,M,,");
    QTest::qWait(1500);
    check(positionsSeen == seen && lat->text() == "47.60833", "and the GPS is no longer read (the port is closed)");

    // a quick on and off is a single update
    followOnce();
    check(lat->text() == "-33.85000" && lon->text() == "151.20000" && !txw->gpsTracking && !gpsReaderPtr->tracking(), "a quick on and off reads the position once (" + lat->text() + ", " + lon->text() + ")");

    // the receiver is not there: amber, a message in the status bar and no message box
    gpsPort = "/dev/does-not-exist-gps";
    boxes = 0;
    QTest::mouseClick(gpsButton, Qt::LeftButton);
    check(txw->gpsTracking && amber() && statusBarPtr->currentMessage().contains("Cannot open"), "with a port that is not there it is amber and the status bar says so: " + statusBarPtr->currentMessage());
    QTest::qWait(300);
    check(boxes == 0 && lat->text() == "-33.85000", "without a message box, and the boxes keep their values");
    // the port is there again (a USB receiver plugged in): found by itself
    gpsPort = slave;
    txw->gpsSettingsChanged();   // Configuration > GPS closed with OK
    checkWait([&] {return green();}, [&] {return QString("after Configuration > GPS names a working port it is green");});
    // the GPS is switched off in Configuration: the tracking stops, the button is remembered as on
    gpsEnabled = false;
    txw->gpsSettingsChanged();
    check(!gpsReaderPtr->tracking() && plain() && txw->gpsTracking && !lat->isReadOnly(), "disabling the GPS in Configuration stops it and releases the boxes");
    gpsEnabled = true;
    txw->gpsSettingsChanged();
    checkWait([&] {return gpsReaderPtr->tracking() && green();}, [&] {return QString("and enabling it again starts following again, as the button was left on");});
    QTest::mouseClick(gpsButton, Qt::LeftButton);
    check(!txw->gpsTracking && !gpsReaderPtr->tracking(), "(off)");

    {
      // the colored button must not make the Transmit tab (and with it the window) wider: the row is as wide as the widest one
      gpsEnabled = false;
      txw->updateGpsButton();
      const int plainWidth = txw->minimumSizeHint().width(), plainButton = gpsButton->sizeHint().width();
      const QString plainFont = fontOf();
      gpsEnabled = true;
      txw->gpsTracking = true;
      txw->gpsTrackState = gpsReader::TRACKWAITING;
      txw->updateGpsButton();
      const int amberWidth = txw->minimumSizeHint().width(), amberButton = gpsButton->sizeHint().width();
      const QString amberFont = fontOf();
      txw->gpsTrackState = gpsReader::TRACKFIXED;
      txw->updateGpsButton();
      const int greenWidth = txw->minimumSizeHint().width(), greenButton = gpsButton->sizeHint().width();
      const QString greenFont = fontOf();
      txw->gpsTracking = false;
      txw->gpsTrackState = -1;
      txw->updateGpsButton();
      check(amberWidth == plainWidth && greenWidth == plainWidth && gpsButton->minimumSize() == gpsButton->maximumSize(), QString("the colors do not widen the Transmit tab (%1 px plain, %2 amber, %3 green; the button %4, %5, %6)")
            .arg(plainWidth).arg(amberWidth).arg(greenWidth).arg(plainButton).arg(amberButton).arg(greenButton));
      // the text keeps its size in every color and after every switch (#152: the first repolish of the style sheet changed the font of
      // the button from the 8 pt of the tab to the application font). Offscreen the button is never polished, so this guards the
      // sheet (it carries the font) but cannot show the original jump; that was measured on the real platform.
      const QFontInfo wanted(txw->gpsButtonFont);
      const QString want = QString("%1 %2px").arg(wanted.family()).arg(wanted.pixelSize());
      check(plainFont == want && amberFont == want && greenFont == want && fontOf() == want && gpsButton->styleSheet().contains("font-size"),
            QString("and the colors keep the text size (%1; plain %2, amber %3, green %4)").arg(want, plainFont, amberFont, greenFont));
    }

    {
      // Windows gives every tool button a white face (toolButtonSheet(), applied by styleToolButtons()). The GPS and the camera button
      // used to replace that sheet with their own and lost the face. Emulated here with the real Windows sheet as their base: the style
      // sheet cascade is the same on every platform. The face is read at the left edge, away from the text and the icon.
      QToolButton *snapButton = txw->findChild<QToolButton *>("snapshotToolButton");
      check(snapButton != nullptr, "(the camera button)");
      if (!snapButton) return 1;
      const QString win = toolButtonSheet(), keepGps = txw->gpsBaseSheet, keepSnap = txw->snapshotBaseSheet;
      const auto face = [&](QToolButton *b) {
        QTest::qWait(50);
        const QImage im = b->grab().toImage();
        return QColor(im.pixel(2, im.height() / 2));
      };
      const auto white = [](const QColor &c) {return c.red() > 240 && c.green() > 240 && c.blue() > 240;};
      txw->gpsBaseSheet = txw->snapshotBaseSheet = win;
      gpsEnabled = false;
      txw->updateGpsButton();
      const QColor gpsOff = face(gpsButton);
      check(gpsButton->styleSheet().startsWith(win) && white(gpsOff), QString("on Windows a GPS that is off keeps the white face (%1)").arg(gpsOff.name()));
      gpsEnabled = true;
      txw->updateGpsButton();
      const QColor gpsNeutral = face(gpsButton);
      check(gpsButton->styleSheet().startsWith(win) && white(gpsNeutral), QString("so does a GPS that is set up and not followed (%1)").arg(gpsNeutral.name()));
      txw->gpsTracking = true;
      txw->gpsTrackState = gpsReader::TRACKWAITING;
      txw->updateGpsButton();
      const QColor gpsAmber = face(gpsButton);
      check(gpsButton->styleSheet().startsWith(win) && gpsAmber.red() > gpsAmber.blue() + 40 && gpsAmber.green() > gpsAmber.blue() + 20, QString("amber is still amber over it (%1)").arg(gpsAmber.name()));
      txw->gpsTrackState = gpsReader::TRACKFIXED;
      txw->updateGpsButton();
      const QColor gpsGreen = face(gpsButton);
      check(gpsButton->styleSheet().startsWith(win) && gpsGreen.green() > gpsGreen.red() + 40, QString("and green is still green (%1)").arg(gpsGreen.name()));
      txw->gpsTracking = false;
      txw->gpsTrackState = -1;
      txw->updateGpsButton();
      txw->beaconRunning = false;
      txw->updateBeaconUi();
      const QColor camOff = face(snapButton);
      check(snapButton->styleSheet() == win && white(camOff), QString("the camera button keeps the white face when no beacon runs (%1)").arg(camOff.name()));
      txw->beaconRunning = true;
      txw->updateBeaconUi();
      const QColor camGreen = face(snapButton);
      check(snapButton->styleSheet().startsWith(win) && camGreen.green() > camGreen.red() + 20, QString("and is green while the beacon runs (%1)").arg(camGreen.name()));
      txw->beaconRunning = false;
      txw->updateBeaconUi();
      check(snapButton->styleSheet() == win, "and gets the face back when it stops");
      txw->gpsBaseSheet = keepGps;
      txw->snapshotBaseSheet = keepSnap;
      txw->updateGpsButton();
      txw->updateBeaconUi();
      check(snapButton->styleSheet() == keepSnap && !gpsButton->styleSheet().contains("QToolButton{background-color:white"), "without that base (every other platform) the sheets are as before");
    }

    // remembered with the TX settings, off when nothing was saved
    txw->gpsTracking = true;
    txw->writeSettings();
    QSettings qs;
    check(qs.value("TX/gpsAutoRead").toBool(), "the switch is saved with the TX settings");
    txw->gpsTracking = false;
    txw->readSettings();
    check(txw->gpsTracking && !gpsReaderPtr->tracking(), "and restored at the next start (reading the settings opens no port: a headless run never does)");
    txw->gpsSettingsChanged();   // what the main window does once it is up, unless the run is headless
    checkWait([&] {return gpsReaderPtr->tracking() && green();}, [&] {return QString("which starts following the GPS again, green");});
    qs.remove("TX/gpsAutoRead");
    txw->readSettings();
    txw->gpsSettingsChanged();
    check(!txw->gpsTracking && !gpsReaderPtr->tracking() && plain() && !lat->isReadOnly(), "with nothing saved the button starts off: a typed position is not replaced");
  }

  printf("\n== Start and the beacon never wait for the GPS (#158, #163)\n");
  {
    QToolButton *startButton = txw->findChild<QToolButton *>("startToolButton");
    QLineEdit *place = txw->findChild<QLineEdit *>("placeLineEdit");
    check(startButton != nullptr, "(the Start button)");
    if (!startButton) return 1;
    check(txw->findChild<QMessageBox *>("gpsNotice") == nullptr, "(there is no position pop-up any more: nothing waits)");
    int boxesBefore = 0;
    QString lastStatus;
    const QMetaObject::Connection statusConn = QObject::connect(statusBarPtr, &QStatusBar::messageChanged, [&](const QString &m) {if (!m.isEmpty()) lastStatus = m;});

    // the picture that was handed to the transmitter is saved: that is what is checked
    saveTXimages = true;
    txSSTVImagesPath = scratch + "/txsaved";
    QDir().mkpath(txSSTVImagesPath);
    auto saved = [&] {return QDir(txSSTVImagesPath).entryList(QDir::Files, QDir::Time);};
    auto loadPicture = [&] {
      QString fn = txStockImagesPath + "/gps-base.png";
      tx->openImage(fn, true, false, false, true);
      tx->setFilename(fn);
      txw->slotGetParams();
    };
    auto render = [&](const QString &la, const QString &lo, const QString &pl = QString()) {
      lat->setText(la);
      lon->setText(lo);
      place->setText(pl);
      txw->slotGetParams();
      return tx->getDisplayedImage()->copy();
    };
    // how far a saved picture is from a reference rendering
    auto distance = [](const QImage &a, QImage b) {
      b = b.scaled(a.size());
      qint64 d = 0;
      for (int y = 0; y < a.height(); y++)
        for (int x = 0; x < a.width(); x++) d += qAbs(qGray(a.pixel(x, y)) - qGray(b.pixel(x, y)));
      return d;
    };
    // a picture is saved under a name with the time in seconds: the next Start waits for the next second
    qint64 lastSec = 0;
    auto newSecond = [&] {while (QDateTime::currentSecsSinceEpoch() == lastSec) QTest::qWait(50);};
    auto waitSaved = [&](int count) {
      const bool ok = waitFor([&] {return saved().size() > count;}, 4000);
      lastSec = QDateTime::currentSecsSinceEpoch();
      return ok;
    };
    // ends the transmission that the Start button began
    auto endTx = [&] {
      txw->slotStop();
      waitFor([&] {return mainWindowPtr->busyMode() < 0;}, 4000);
    };
    // presses Start and returns how long it took until the picture was with the transmitter; -1: it never came
    auto startAndTime = [&](const int count) -> qint64 {
      newSecond();
      QElapsedTimer clock;
      clock.start();
      QTest::mouseClick(startButton, Qt::LeftButton);
      if (!waitSaved(count)) return -1;
      return clock.elapsed();
    };

    {
      // the template that shows %a and %b (a later test section selected another one)
      QComboBox *combo = txw->findChild<QComboBox *>("templatesComboBox");
      QCheckBox *useBox = txw->findChild<QCheckBox *>("templateCheckBox");
      useBox->setChecked(true);
      combo->setCurrentIndex(combo->findText("gpstest"));
    }
    loadPicture();
    gpsPlaceLookup = false;
    gpsPort = slave;
    const QImage stale = render("12.5", "-45.25");
    const QImage fresh = render("47.60620", "-122.33210");
    const QImage blank = render("", "");
    check(distance(stale, fresh) > 20000 && distance(blank, fresh) > 20000, "(the old, the new and no position give different pictures)");
    render("12.5", "-45.25");
    loadPicture();

    // the button is on and the receiver sends: the boxes are current, Start sends at once with them
    feeding = true;
    sentence = nmea("GPGGA,123519,4736.372,N,12219.926,W,1,08,0.9,545.4,M,46.9,M,,");
    QTest::mouseClick(gpsButton, Qt::LeftButton);
    checkWait([&] {return lat->text() == "47.60620";}, [&] {return "(the GPS button is on and the boxes follow, " + lat->text() + ")";});
    int before = saved().size();
    boxesBefore = boxes;
    qint64 ms = startAndTime(before);
    check(ms >= 0 && ms < 2500, QString("Start sends at once (%1 ms)").arg(ms));
    check(startButton->isEnabled() || mainWindowPtr->busyMode() >= 0, "(the Start button was never held back)");
    if (saved().size() > before)
      {
        const QImage sent(txSSTVImagesPath + "/" + saved().first());
        const qint64 toFresh = distance(sent, fresh), toStale = distance(sent, stale);
        check(!sent.isNull() && toFresh < toStale / 4, QString("with the position of the GPS (distance %1 to the new, %2 to the old)").arg(toFresh).arg(toStale));
      }
    endTx();
    check(boxes == boxesBefore, "(no message box)");

    // the receiver goes silent while the button is on: Start does not wait for it, the last position goes out
    feeding = false;
    checkWait([&] {return amber();}, [&] {return QString("(the receiver is silent: amber)");}, 4000);
    before = saved().size();
    ms = startAndTime(before);
    check(ms >= 0 && ms < 2500, QString("with a silent receiver Start still sends at once (%1 ms)").arg(ms));
    check(lat->text() == "47.60620" && boxes == boxesBefore, "with the last position, and no message box");
    if (saved().size() > before)
      {
        const QImage sent(txSSTVImagesPath + "/" + saved().first());
        check(distance(sent, fresh) < distance(sent, stale) / 4, "(the picture shows the last position)");
      }
    endTx();

    // the port is gone altogether
    gpsPort = "/dev/does-not-exist-gps";
    txw->gpsSettingsChanged();
    check(amber() && txw->gpsTracking, "(the port is gone: amber)");
    before = saved().size();
    ms = startAndTime(before);
    check(ms >= 0 && ms < 2500 && boxes == boxesBefore, QString("a port that cannot be opened does not hold Start either (%1 ms)").arg(ms));
    endTx();

    // no position at all yet: the picture is sent without one
    render("", "");
    loadPicture();
    before = saved().size();
    ms = startAndTime(before);
    check(ms >= 0 && ms < 2500, QString("with no position at all Start sends too (%1 ms)").arg(ms));
    if (saved().size() > before)
      {
        const QImage sent(txSSTVImagesPath + "/" + saved().first());
        check(distance(sent, blank) < distance(sent, fresh) / 4, "without the position in the picture");
      }
    endTx();

    // the beacon is not held up either (no picture is loaded, so a beacon that gets as far as sending stops with this error)
    tx->setValidImage(false);
    txw->beaconRunning = true;
    txw->beaconError.clear();
    txw->beaconTransmitLoaded();
    check(txw->beaconError.contains("no valid picture"), "the beacon does not wait for the GPS: " + txw->beaconError);
    txw->stopBeacon();
    loadPicture();

    // the button off: the typed position goes out as it is, and the GPS is not read
    QTest::mouseClick(gpsButton, Qt::LeftButton);
    check(!txw->gpsTracking && !gpsReaderPtr->tracking(), "(the GPS button is off)");
    gpsPort = slave;
    feeding = true;
    render("12.5", "-45.25");
    int positions = positionsSeen;
    before = saved().size();
    ms = startAndTime(before);
    check(ms >= 0 && ms < 2500 && positionsSeen == positions && lat->text() == "12.5", QString("with the button off the typed position goes out and the GPS is not read (%1 ms, %2 reads)").arg(ms).arg(positionsSeen - positions));
    if (saved().size() > before)
      {
        const QImage sent(txSSTVImagesPath + "/" + saved().first());
        check(distance(sent, stale) < distance(sent, fresh) / 4, "(the picture shows the typed position)");
      }
    endTx();

    // the place: a slow service does not hold Start back, the last place goes out
    txw->placeFailedAt.invalidate();
    txw->placeCellKnown = false;
    gpsPlaceLookup = true;
    fake.body = seattle;
    place->setText("Seattle, WA");
    sentence = nmea("GPGGA,123519,4530.000,N,12245.000,W,1,08,0.9,545.4,M,46.9,M,,");
    fake.delayMs = 3500;
    QTest::mouseClick(gpsButton, Qt::LeftButton);
    checkWait([&] {return lat->text() == "45.50000" && txw->placeAwaiting;}, [&] {return QString("(the position is in, the slow place lookup is running)");});
    before = saved().size();
    ms = startAndTime(before);
    check(ms >= 0 && ms < 2500, QString("a place service that is slow does not hold Start back (%1 ms)").arg(ms));
    check(place->text() == "Seattle, WA", "the last place goes out");
    endTx();
    fake.delayMs = 0;
    waitFor([&] {return !txw->placeAwaiting;}, 6000);
    QTest::mouseClick(gpsButton, Qt::LeftButton);
    gpsPlaceLookup = false;
    saveTXimages = false;
    feeding = false;
    QObject::disconnect(statusConn);   // the lambda refers to a variable of this block
    lat->setText(keepLat);
    lon->setText(keepLon);
    place->setText(keepPlace);
    txw->slotGetParams();
    tx->setValidImage(false);
  }

  printf("\n== the position is not remembered (issue #149)\n");
  {
    QLineEdit *place = txw->findChild<QLineEdit *>("placeLineEdit");
    check(lat->text() == "-33.85000" && place->text() == "Missoula, MT", "(a position and a place are in the boxes)");
    QSettings qs;
    qs.setValue("TX/latitude", "12.34567");   // what an older version left in the file
    qs.setValue("TX/longitude", "76.54321");
    qs.setValue("TX/place", "Elsewhere");
    txw->writeSettings();
    check(!qs.contains("TX/latitude") && !qs.contains("TX/longitude") && !qs.contains("TX/place"), "neither is saved with the TX settings, and an older version's are removed");
    qs.setValue("TX/latitude", "12.34567");
    qs.setValue("TX/place", "Elsewhere");
    lat->setText("");
    lon->setText("");
    place->setText("");
    txw->slotGetParams();
    txw->readSettings();
    check(lat->text().isEmpty() && lon->text().isEmpty() && place->text().isEmpty() && tx->latitude.isEmpty() && tx->place.isEmpty(), "nor read at the next start");
    qs.remove("TX/latitude");
    qs.remove("TX/place");
  }

  printf("\n== position format: degrees and decimal minutes (issue #133)\n");
  {
    QLineEdit *place = txw->findChild<QLineEdit *>("placeLineEdit");
    check(place != nullptr, "(the Place box)");
    if (!place) return 1;
    gpsPlaceLookup = false;
    gpsCoordFormat = COORDDECIMAL;
    txw->positionFormat = COORDDECIMAL;
    auto edit = [&](const QString &la, const QString &lo) {
      lat->setText(la);
      lon->setText(lo);
      emit lat->editingFinished();
    };
    const QString degree = QString::fromUtf8("\xC2\xB0");

    // decimal degrees, the default: what is typed is printed as typed
    edit("47" + degree + "36'N", "122.3321 W");
    check(lat->text() == "47" + degree + "36'N" && lon->text() == "122.3321 W" && tx->latitude == lat->text(), "in decimal degrees a position typed in another notation stays as typed");

    // switching converts the boxes, both ways
    edit("-33.85000", "151.20000");
    gpsCoordFormat = COORDDEGMIN;
    txw->positionFormatChanged();
    check(lat->text() == "33 51.00S" && lon->text() == "151 12.00E", "Configuration > GPS set to degrees and minutes: the boxes follow (" + lat->text() + ", " + lon->text() + ")");
    check(tx->latitude == "33 51.00S" && tx->longitude == "151 12.00E", "and so do the template values");
    check(editorMacroValues().value('a') == "33 51.00S" && editorMacroValues().value('b') == "151 12.00E", "and the template editor preview");
    gpsCoordFormat = COORDDECIMAL;
    txw->positionFormatChanged();
    check(lat->text() == "-33.85000" && lon->text() == "151.20000" && tx->latitude == "-33.85000", "and back to decimal degrees");
    txw->positionFormatChanged();
    check(lat->text() == "-33.85000", "(a format that did not change touches nothing)");

    // typed or pasted in any notation: converted when the box is left
    gpsCoordFormat = COORDDEGMIN;
    txw->positionFormatChanged();
    edit("47.6062", "-122.3321");
    check(lat->text() == "47 36.37N" && lon->text() == "122 19.93W", "decimal degrees from a map become degrees and minutes (" + lat->text() + ", " + lon->text() + ")");
    check(tx->latitude == "47 36.37N" && tx->longitude == "122 19.93W", "and reach the template values");
    edit("42 44.54N", "84 23.82 w");
    check(lat->text() == "42 44.54N" && lon->text() == "084 23.82W", "the example of the issue: 42 44.54N 084 23.82W, the longitude padded to three digits (" + lon->text() + ")");
    edit("47" + degree + "36'22\"N", "122" + degree + "19.93'W");
    check(lat->text() == "47 36.37N" && lon->text() == "122 19.93W", "degrees, minutes, seconds and the degree marks are understood (" + lat->text() + ", " + lon->text() + ")");
    edit("S 33.85", "E 151.2");
    check(lat->text() == "33 51.00S" && lon->text() == "151 12.00E", "so are letters in front (" + lat->text() + ", " + lon->text() + ")");
    edit("somewhere", "123 45.6E 7");
    check(lat->text() == "somewhere" && lon->text() == "123 45.6E 7", "text that is no coordinate is left alone");
    edit("47.6062", "E 200");
    check(lat->text() == "47 36.37N" && lon->text() == "E 200", "and so is a longitude out of range, while the other box is converted");

    // a new position asks for the place once (not the one the beacon test cached: the lookup remembers positions within 0.01 degree)
    gpsPlaceLookup = true;
    place->setText("My Town, XX");
    emit place->editingFinished();
    fake.body = seattle;
    const int hitsBefore = fake.hits;
    edit("42 44.54N", "084 23.82W");
    checkWait([&] {return place->text() == "Seattle, WA";}, [&] {return "a position typed in asks for the place (" + place->text() + ")";});
    check(fake.hits == hitsBefore + 1 && lat->text() == "42 44.54N" && lon->text() == "084 23.82W", "once, and the boxes stay as they are (" + lon->text() + ")");
    check(fake.lastRequest.contains("lat=42.74233") && fake.lastRequest.contains("lon=-84.39700"), "the lookup works from the degrees and minutes of the boxes");

    // a box that is left again does not change, and does not ask for a place
    place->setText("My Town, XX");
    emit place->editingFinished();
    emit lat->editingFinished();
    emit lon->editingFinished();
    QTest::qWait(300);
    check(fake.hits == hitsBefore + 1 && place->text() == "My Town, XX", "leaving the boxes unchanged looks nothing up and keeps a typed place");
    gpsCoordFormat = COORDDECIMAL;
    txw->positionFormatChanged();
    emit lat->editingFinished();
    QTest::qWait(300);
    check(lat->text() == "42.74233" && lon->text() == "-84.39700" && fake.hits == hitsBefore + 1 && place->text() == "My Town, XX",
          "converting the format keeps the place of the same position (" + lat->text() + ", " + lon->text() + ")");
    gpsCoordFormat = COORDDEGMIN;
    txw->positionFormatChanged();
    check(lat->text() == "42 44.54N" && lon->text() == "084 23.82W", "(and back)");
    gpsPlaceLookup = false;

    // the GPS button
    feeding = true;
    sentence = nmea("GPGGA,123519,4736.372,N,12219.926,W,1,08,0.9,545.4,M,46.9,M,,");
    followOnce();
    checkWait([&] {return lat->text() == "47 36.37N" && lon->text() == "122 19.93W";}, [&] {return "the GPS button writes degrees and minutes (" + lat->text() + ", " + lon->text() + ")";});
    sentence = nmea("GPGGA,123519,3351.000,S,00712.000,W,1,08,0.9,545.4,M,46.9,M,,");
    followOnce();
    checkWait([&] {return lat->text() == "33 51.00S" && lon->text() == "007 12.00W";}, [&] {return "with the hemisphere letters and the longitude padded (" + lat->text() + ", " + lon->text() + ")";});
    feeding = false;

    // the macros print it
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
      check(tpl::saveFile(templatesPath + "/gpsformat.qtempl", d, &err), "(template with %a %b saved)");
      galleryWidgetPtr->changedMatrix(imageViewer::TEMPLATETHUMB);
      txw->setupTemplatesComboBox();
      QTest::qWait(200);
      QComboBox *combo = txw->findChild<QComboBox *>("templatesComboBox");
      check(combo && combo->findText("gpsformat") >= 0, "(the template is in the TX list)");
      if (!combo) return 1;
      combo->setCurrentIndex(combo->findText("gpsformat"));
      lat->setText("");
      lon->setText("");
      txw->slotGetParams();
      const QImage empty = tx->getDisplayedImage()->copy();
      edit("47.6062", "-122.3321");
      check(*tx->getDisplayedImage() != empty, "and the template shows the position (the picture changed)");
      check(tx->latitude == "47 36.37N" && tx->longitude == "122 19.93W", "from the values that the macros expand (" + tx->latitude + ", " + tx->longitude + ")");
    }

    // not remembered: the boxes start empty at the next start (issue #149)
    txw->writeSettings();
    {
      QSettings qs;
      check(!qs.contains("TX/latitude") && !qs.contains("TX/longitude") && !qs.contains("TX/place"), "the position is not saved with the TX settings");
    }
    gpsCoordFormat = COORDDEGMIN;
    txw->positionFormatChanged();
    check(lat->text() == "47 36.37N", "(back in degrees and minutes)");
    gpsCoordFormat = COORDDECIMAL;
    txw->positionFormatChanged();
    check(lat->text() == "47.60617" && lon->text() == "-122.33217", "(and decimal degrees: " + lat->text() + ", " + lon->text() + ")");
    lat->setText("");
    lon->setText("");
    place->setText("");
    txw->slotGetParams();
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
        QCheckBox *placeBox = page->findChild<QCheckBox *>("placeCheckBox");
        QPushButton *testButton = page->findChild<QPushButton *>("testPushButton");
        QLabel *result = page->findChild<QLabel *>("resultLabel");
        QComboBox *formatBox = page->findChild<QComboBox *>("formatComboBox");
        check(portBox && baudBox && enableBox && placeBox && testButton && result && formatBox, "with port, baud, position format, enable, place lookup, Test button and result");
        check(page->findChild<QCheckBox *>("beaconRefreshCheckBox") == nullptr, "and no beacon option: the GPS button on the Transmit tab decides (#148)");
        if (portBox && baudBox && enableBox && placeBox && testButton && result && formatBox)
          {
            check(formatBox->count() == 2 && formatBox->currentIndex() == COORDDECIMAL, "the position format offers two formats and starts with decimal degrees");
            {
              // the page must not get wider for the long names in the list (measured by hiding the row, not by eye)
              const int with = page->minimumSizeHint().width();
              for (const char *name : {"formatLabel", "formatComboBox"}) page->findChild<QWidget *>(name)->hide();
              const int without = page->minimumSizeHint().width();
              for (const char *name : {"formatLabel", "formatComboBox"}) page->findChild<QWidget *>(name)->show();
              check(with == without && formatBox->minimumSizeHint().width() < 300, QString("the position format row does not make the page wider (%1 px with, %2 px without)").arg(with).arg(without));
            }
            // settings round trip: the page holds the values, writeSettings() saves them, readSettings() loads them
            QSettings qs;
            enableBox->setChecked(true);
            portBox->setCurrentText(slave);
            baudBox->setCurrentText("9600");
            placeBox->setChecked(true);
            formatBox->setCurrentIndex(COORDDEGMIN);
            configDialogPtr->writeSettings();
            check(qs.value("GPS/enabled").toBool() && qs.value("GPS/port").toString() == slave && qs.value("GPS/baud").toInt() == 9600
                  && qs.value("GPS/placeLookup").toBool() == true
                  && qs.value("GPS/coordinateFormat").toInt() == COORDDEGMIN, "the settings are written");
            check(gpsEnabled && gpsPort == slave && gpsBaud == 9600 && gpsPlaceLookup && gpsCoordFormat == COORDDEGMIN, "and are what the program uses");
            gpsEnabled = false;
            gpsPort.clear();
            gpsBaud = 0;
            gpsPlaceLookup = false;
            gpsCoordFormat = COORDDECIMAL;
            configDialogPtr->readSettings();
            check(gpsEnabled && gpsPort == slave && gpsBaud == 9600 && gpsPlaceLookup && gpsCoordFormat == COORDDEGMIN, "and read back");
            check(portBox->currentText() == slave && baudBox->currentText() == "9600" && enableBox->isChecked() && placeBox->isChecked()
                  && formatBox->currentIndex() == COORDDEGMIN, "and shown on the page");
            qs.setValue("GPS/coordinateFormat", 7);   // not a format of this program
            configDialogPtr->readSettings();
            check(gpsCoordFormat == COORDDECIMAL && formatBox->currentIndex() == COORDDECIMAL, "a format that does not exist is read as decimal degrees");
            qs.setValue("GPS/beaconRefresh", true);   // the key of the old beacon option, as an earlier version left it
            configDialogPtr->readSettings();
            check(!qs.contains("GPS/beaconRefresh"), "the saved value of the old beacon option is dropped");

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

            // with the GPS button on, Test takes the port for a moment and gives it back
            txw->gpsTracking = true;
            gpsEnabled = true;
            gpsPort = slave;
            txw->gpsSettingsChanged();
            checkWait([&] {return green();}, [&] {return QString("(the GPS button is on and green)");});
            result->clear();
            QTest::mouseClick(testButton, Qt::LeftButton);
            checkWait([&] {return result->text().startsWith("Position:");}, [&] {return "Test works while the GPS is followed: " + result->text();});
            const int seenBefore = positionsSeen;
            checkWait([&] {return positionsSeen > seenBefore && green() && gpsReaderPtr->tracking();}, [&] {return QString("and the tracking goes on afterwards");});
            QTest::mouseClick(gpsButton, Qt::LeftButton);
            check(!txw->gpsTracking && !gpsReaderPtr->tracking(), "(off)");

            // the format chosen on the page is the one Test shows, whether or not it is saved
            formatBox->setCurrentIndex(COORDDEGMIN);
            QTest::mouseClick(testButton, Qt::LeftButton);
            checkWait([&] {return result->text().startsWith("Position:") && result->text().contains("N");}, [&] {return "Test shows degrees and minutes: " + result->text();});
            check(result->text().contains("47 36.37N") && result->text().contains("122 19.93W") && gpsCoordFormat == COORDDECIMAL, "with the position in them, and nothing is saved by it");
            formatBox->setCurrentIndex(COORDDECIMAL);

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

  // Last: quitting stops the application's threads.
  printf("\n== quitting with a GPS port that the CAT port has taken (#127)\n");
  {
    // The quit writes every page of the Configuration dialog. A GPS page that finds its port taken by the CAT or PTT port must not
    // open a message box then: nobody can see or answer it (its parent is the hidden dialog), and while it is open the program does not end.
    QWidget *page = configDialogPtr->findChild<QWidget *>("gpsWidget");
    QComboBox *portBox = page ? page->findChild<QComboBox *>("portComboBox") : nullptr;
    QCheckBox *enableBox = page ? page->findChild<QCheckBox *>("enableCheckBox") : nullptr;
    QPushButton *testButton = page ? page->findChild<QPushButton *>("testPushButton") : nullptr;
    QLabel *result = page ? page->findChild<QLabel *>("resultLabel") : nullptr;
    check(portBox && enableBox && testButton && result, "(the GPS page)");
    if (!(portBox && enableBox && testButton && result)) return 1;
    rigConfig *rig = configDialogPtr->findChild<rigConfig *>("catWidget");
    QCheckBox *catEnable = rig ? rig->findChild<QCheckBox *>("enableCATCheckBox") : nullptr;
    QComboBox *catBox = rig ? rig->findChild<QComboBox *>("serialPortComboBox") : nullptr;
    check(catEnable && catBox, "(the CAT page)");
    if (!(catEnable && catBox)) return 1;
    catBox->setCurrentText(slave);
    catEnable->setChecked(true);   // (setChecked does not click: the rig is not started)
    rig->getParams();
    enableBox->setChecked(true);   // (the same: the conflict is not noticed here, as when the port is chosen after the box was ticked)
    portBox->setCurrentText(slave);
    boxes = 0;
    emit portBox->activated(portBox->currentIndex());   // the port is picked from the list while the box is ticked
    checkWait([&] {return boxes > 0 && !enableBox->isChecked();}, [&] {return "picking the CAT port for an enabled GPS is refused: " + lastBox.section('\n', 0, 0);});
    enableBox->setChecked(true);   // (ticked again, for the quit below)
    feeding = false;
    boxes = 0;
    lastBox.clear();
    QTest::mouseClick(testButton, Qt::LeftButton);
    checkWait([&] {return testButton->isEnabled();}, [&] {return "the Test button is usable again: " + result->text();}, 12000);
    check(result->text().contains("different from the CAT serial port"), "Test says that the port belongs to CAT: " + result->text());
    check(boxes == 0, "(and does so without a message box)");

    confirmClose = false;
    boxes = 0;
    lastBox.clear();
    QMetaObject::invokeMethod(mainWindowPtr, "slotExit", Qt::DirectConnection);
    check(boxes == 0, "quitting opens no message box: " + lastBox.section('\n', 0, 0));
    check(!gpsEnabled, "and leaves the GPS disabled, as it cannot work on that port");
    QSettings qs;
    check(!qs.value("GPS/enabled").toBool(), "which is what is saved");
  }

  QObject::disconnect(positionConn);   // the lambda refers to a variable of main
  printf("\n%d passed, %d failed\n", passed, failed);
  fflush(stdout);
  _exit(failed ? 1 : 0);   // the application's threads are not shut down by this probe
}
