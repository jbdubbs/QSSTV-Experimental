// Screenshots of the Qt Quick interface (src/qml), offscreen, with the real controllers and the real (hidden) main window behind them:
//   shot <scratch-dir> <out-dir> <width> <height> <scene>...
// scenes: idle (listening, nothing yet) receiving (a picture half drawn, Martin 1) done (the picture complete) options (the Receive options
// sheet open) digital (the DRM mode) stopped (receiver stopped) quiet (the level too low) loud tx gallery (placeholders).
// Each scene is saved as <out-dir>/qml-<w>x<h>-<scene>.png. Built and run by tests/qmlui/app.sh (links the objects of the build-qml tree).
// Environment: QSSTV_QML_THEME=light|dark, QSSTV_QML_FONTSCALE=1.3 are read by the interface itself.
#include <QtTest>
#include <QtWidgets>
#include <QtQuick>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <functional>
#include "appglobal.h"
#include "configparams.h"
#include "dispatcher.h"
// mainWindow::writeSettings() is private: the settings-position scene runs it, as the program does when a system box covers it on Android
#define private public
#include "mainwindow.h"
#undef private
#include "uimode.h"
#include "qmlui.h"
#include "appmodel.h"
#include "rxcontroller.h"
#include "txcontroller.h"
#include "txwidget.h"
#include "soundbase.h"
#include "rigcontrol.h"
#include "sstvparam.h"
#include "imageviewer.h"
#include "rxwidget.h"
#include "spectrumwidget.h"
#include "gallerymodel.h"
#include "gallerywidget.h"
#include "settingscontroller.h"
#include "touchtemplateeditor.h"
#include "calibrationcontroller.h"
#include "drmprofilescontroller.h"
#include "frequencycontroller.h"
#include "soundconfig.h"
#include "operatorconfig.h"
#include "directoriesconfig.h"
#include "gpsconfig.h"
#include "gpsreader.h"
#include "placelookup.h"
#include "../gps/fakeservice.h"

static QQuickWindow *theWindow()
{
  for (QWindow *w : QGuiApplication::allWindows())
    if (QQuickWindow *q = qobject_cast<QQuickWindow *>(w)) return q;
  return nullptr;
}

// an item by objectName, through the visual tree (the QObject tree of QML items is not the one that holds them)
static QQuickItem *findItem(QQuickItem *root, const QString &name)
{
  if (root->objectName() == name) return root;
  for (QQuickItem *c : root->childItems())
    if (QQuickItem *f = findItem(c, name)) return f;
  return nullptr;
}

// the item that is shown: the narrow and the wide layout of a screen both hold the item, and only one of them is visible
static QQuickItem *findShown(QQuickItem *root, const QString &name)
{
  if (!root->isVisible()) return nullptr;
  if (root->objectName() == name) return root;
  for (QQuickItem *c : root->childItems())
    if (QQuickItem *f = findShown(c, name)) return f;
  return nullptr;
}

// is an item that is shown holding exactly this text (a Text, a chip, a pill)?
static bool shownText(QQuickItem *root, const QString &word)
{
  if (!root->isVisible()) return false;
  if (root->property("text").isValid() && root->property("text").toString() == word) return true;
  for (QQuickItem *c : root->childItems())
    if (shownText(c, word)) return true;
  return false;
}

static void feedWaterfall(int frames)
{
  // what the receiver sends the spectrum display: tones of an SSTV line and some noise
  std::vector<double> samples(8192);
  for (int frame = 0; frame < frames; frame++)
    {
      for (size_t i = 0; i < samples.size(); i++)
        {
          const double f = 1500.0 + 700.0 * sin(frame / 7.0);   // a tone that wanders between 800 and 2200 Hz
          samples[i] = 3000 * sin(2 * M_PI * f * i / 12000.0) + 1200 * sin(2 * M_PI * 1200.0 * i / 12000.0) + (rand() % 2001 - 1000);
        }
      mainWindowPtr->spectrumFramePtr->realFFT(samples.data());
    }
}

static QImage testPicture(int w, int h, int lines)
{
  QImage im(w, h, QImage::Format_ARGB32_Premultiplied);
  im.fill(QColor(16, 20, 28));
  QPainter p(&im);
  QLinearGradient g(0, 0, 0, h * 0.62);
  g.setColorAt(0, QColor("#6ec1ff"));
  g.setColorAt(1, QColor("#d9f0ff"));
  p.fillRect(QRectF(0, 0, w, h * 0.7), g);
  p.setBrush(QColor("#fff6c8"));
  p.setPen(Qt::NoPen);
  p.drawEllipse(QPointF(w * 0.3, h * 0.25), h * 0.1, h * 0.1);
  p.setBrush(QColor("#2f7d3a"));
  QPainterPath hill;
  hill.moveTo(0, h * 0.72);
  hill.cubicTo(w * 0.25, h * 0.5, w * 0.5, h * 0.7, w, h * 0.55);
  hill.lineTo(w, h);
  hill.lineTo(0, h);
  p.drawPath(hill);
  p.end();
  // only the lines received so far are there
  for (int y = lines; y < h; y++) memset(im.scanLine(y), 0, size_t(im.bytesPerLine()));
  return im;
}

// 2:1, a green column at the left edge and a magenta one at the right on blue, a white square in the middle: how a mode of another shape
// frames it (Stretch, Crop, Fit or Free) shows in what is left of the columns and of the blue (#177)
static QImage framingPicture()
{
  QImage im(400, 200, QImage::Format_ARGB32);
  im.fill(QColor("#2060c0"));
  QPainter p(&im);
  p.fillRect(0, 0, 40, 200, QColor("#20c040"));
  p.fillRect(360, 0, 40, 200, QColor("#e020c0"));
  p.fillRect(190, 90, 20, 20, Qt::white);
  p.end();
  return im;
}

// Scroll the page (the Flickable around an item) so that the item is in view, as a finger would; an earlier scene may have left it scrolled
static void revealItem(QQuickItem *item)
{
  for (QQuickItem *p = item->parentItem(); p; p = p->parentItem())
    if (QString::fromLatin1(p->metaObject()->className()) == "QQuickFlickable" && p->property("contentHeight").toDouble() > p->height())
      {
        QQuickItem *ci = qvariant_cast<QQuickItem *>(p->property("contentItem"));
        const qreal y = item->mapToItem(ci, QPointF(0, 0)).y();
        p->setProperty("contentY", qBound(0.0, y - 8, p->property("contentHeight").toDouble() - p->height()));
        QTest::qWait(150);
        for (int i = 0; i < 40 && (p->property("moving").toBool() || p->property("flicking").toBool()); i++) QTest::qWait(50);   // a press on a page that is still moving only stops it
        return;
      }
}

// pictures in the gallery folders, of today, yesterday, last week and last year, so that the grid has its day headings
static void makeGallery(const QString &scratch)
{
  rxSSTVImagesPath = scratch + "/g/rx_sstv/";
  rxDRMImagesPath = scratch + "/g/rx_drm/";
  txSSTVImagesPath = scratch + "/g/tx_sstv/";
  txDRMImagesPath = scratch + "/g/tx_drm/";
  txStockImagesPath = scratch + "/g/tx_stock/";
  templatesPath = scratch + "/g/templates/";
  for (const QString &d : {rxSSTVImagesPath, rxDRMImagesPath, txSSTVImagesPath, txDRMImagesPath, txStockImagesPath, templatesPath}) QDir().mkpath(d);
  struct F {QString dir, name; int hoursAgo; int lines;};
  const std::vector<F> files = {
    {rxSSTVImagesPath, "m1", 1, 256}, {rxSSTVImagesPath, "s1", 3, 240}, {rxSSTVImagesPath, "pd120", 5, 256}, {rxSSTVImagesPath, "r36", 7, 200},
    {rxSSTVImagesPath, "m1", 27, 256}, {rxSSTVImagesPath, "pd90", 30, 256}, {rxSSTVImagesPath, "s2", 24 * 5, 256}, {rxSSTVImagesPath, "m2", 24 * 5 + 2, 128},
    {rxSSTVImagesPath, "r72", 24 * 40, 256}, {rxDRMImagesPath, "drm", 2, 256}, {rxDRMImagesPath, "drm", 28, 256},
    {txSSTVImagesPath, "tx", 4, 256}, {txSSTVImagesPath, "tx", 26, 256}, {txDRMImagesPath, "tx", 50, 256},
    {txStockImagesPath, "stock-a", 24 * 300, 256}, {txStockImagesPath, "stock-b", 24 * 300, 256}};
  int n = 0;
  for (const F &f : files)
    {
      const QDateTime when = QDateTime::currentDateTimeUtc().addSecs(-3600 * f.hoursAgo);
      const QString file = QString("%1%2_%3_%4.png").arg(f.dir, f.name, when.toString("yyyyMMdd_HHmmss")).arg(n++);
      QImage im = testPicture(320, 256, f.lines);
      QPainter p(&im);
      p.setPen(Qt::white);
      p.setFont(QFont("sans", 40, QFont::Bold));
      p.drawText(im.rect(), Qt::AlignCenter, QString::number(n));
      p.end();
      im.save(file);
      QFile fl(file);
      if (fl.open(QIODevice::ReadWrite)) {fl.setFileTime(when, QFileDevice::FileModificationTime); fl.close();}
    }
  QFile::copy(QStringLiteral(SRC_TEMPLATE_FIXTURE), templatesPath + "synthetic.templ");
}

// What the guidelines ask for, looked for in the shown tree (QSSTV_QML_AUDIT=1): a place that takes a tap smaller than 48 x 48, text smaller than
// 12 sp, text that does not fit its space (elided)
static void auditTree(QQuickItem *item, const QString &scene)
{
  if (!item->isVisible() || item->opacity() <= 0.01) return;
  const QString cls = QString::fromLatin1(item->metaObject()->className());
  bool tapTarget = false;
  double margin = 0;
  for (QObject *c : item->children())
    if (QString::fromLatin1(c->metaObject()->className()).contains("TapHandler")) {tapTarget = true; margin = qMax(margin, c->property("margin").toDouble());}
  if (tapTarget && item->width() > 0 && item->height() > 0 && (item->width() + 2 * margin < 47.5 || item->height() + 2 * margin < 47.5))
    {
      QQuickItem *p = item->parentItem();
      fprintf(stderr, "AUDIT %s TARGET %.0fx%.0f %s/%s in %s\n", qPrintable(scene), item->width(), item->height(), qPrintable(cls),
              qPrintable(item->objectName()), p ? p->metaObject()->className() : "-");
    }
  if (cls.startsWith("QQuickText"))
    {
      const QString text = item->property("text").toString();
      const QFont f = item->property("font").value<QFont>();
      if (!text.isEmpty() && f.pixelSize() > 0 && f.pixelSize() < 12)
        fprintf(stderr, "AUDIT %s SMALLTEXT %dpx \"%s\"\n", qPrintable(scene), f.pixelSize(), qPrintable(text.left(30)));
      if (!text.isEmpty() && item->property("truncated").toBool())
        fprintf(stderr, "AUDIT %s ELIDED \"%s\" width %.0f\n", qPrintable(scene), qPrintable(text.left(40)), item->width());
    }
  for (QQuickItem *c : item->childItems()) auditTree(c, scene);
}

int main(int argc, char **argv)
{
  setvbuf(stdout, nullptr, _IOLBF, 0);
  if (argc < 6) {fprintf(stderr, "usage: shot scratch outdir width height scene...\n"); return 2;}
  const QString scratch = argv[1], outDir = argv[2];
  const int width = atoi(argv[3]), height = atoi(argv[4]);
  qputenv("HOME", scratch.toLocal8Bit());
  qputenv("XDG_CONFIG_HOME", (scratch + "/config").toLocal8Bit());
  qputenv("XDG_DATA_HOME", (scratch + "/data").toLocal8Bit());
  qputenv("QT_QPA_PLATFORM", "offscreen");
  qputenv("QT_QUICK_BACKEND", "software");
  qputenv("PIPEWIRE_REMOTE", "qsstv-test-no-such-server");
  qputenv("PULSE_SERVER", "unix:/nonexistent/qsstv-test");
  qputenv("QSSTV_UI", "qml");
  QCoreApplication::setOrganizationName("qsstv-qmlui");
  QCoreApplication::setApplicationName("qsstv-qmlui");
  QApplication app(argc, argv);
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, scratch + "/settings");
  {
    QSettings qs;
    qs.setValue("update/asked", true);
    qs.setValue("update/autoCheck", false);
    qs.setValue("appimage/integrationAsked", true);
    qs.setValue("FirstRun/done", true);
  }
  QDir().mkpath(outDir);
  globalInit();
  mainWindowPtr = new mainWindow;
  QTest::qWait(300);
  mainWindowPtr->init();
  mainWindowPtr->startRunning(false);   // no sound card receiver
  makeGallery(scratch);   // after the settings were read
  qmlUi::show();
  QTest::qWait(500);
  QQuickWindow *win = theWindow();
  if (!win) {fprintf(stderr, "FAIL: the Qt Quick window did not come up\n"); return 1;}
  win->resize(width, height);
  QTest::qWait(300);
  int exitCode = 0;
  int savedTxMode = -1;   // the Transmit mode before a scene that changes it (tx-chips): put back by the next scene
  bool outputDeviceChanged = false;   // a scene that names a long output device (tx-digital-sending-device): the next scene gets the old one back
  const QString savedOutputDevice = outputAudioDevice;
  bool volumeTest = false;   // a scene about the volume keys: the phone's volume and the device of the keys are made up
  bool placeSceneDirty = false;   // a scene about the city and state lookup: the next scene gets the lookup off, the boxes empty and the real service back
  std::unique_ptr<fakeService> placeFake;
  const QUrl realPlaceService = placeLookupPtr->service();
  const QString realGpsPort = gpsPort;
  for (int i = 5; i < argc; i++)
    {
      const QString scene = argv[i];
      AppModel *app_ = AppModel::instance();
      RxController *rx = RxController::instance();
      // nothing is left open from the scene before
      win->setProperty("aboutOpen", false);
      for (const char *screenName : {"receiveScreen", "transmitScreen"})
        if (QQuickItem *s = findItem(win->contentItem(), screenName))
          for (const char *prop : {"optionsOpen", "modesOpen", "detailsOpen", "digitalOpen", "toolsOpen", "templatesOpen", "open"})
            if (s->property(prop).isValid()) s->setProperty(prop, false);
      // the sheet of starting points for a template (tpl-starters) is a popup over every screen: it must not stay for the scenes that follow
      std::function<void(QQuickItem *)> closeStarters = [&](QQuickItem *it) {
        if (QString::fromLatin1(it->metaObject()->className()).startsWith("StartersSheet")) it->setProperty("open", false);
        for (QQuickItem *c : it->childItems()) closeStarters(c);
      };
      closeStarters(win->contentItem());
      // the list of the framing pill (tx-framing-menu) is a popup too
      if (findShown(win->contentItem(), "framingStretch")) {QTest::keyClick(win, Qt::Key_Escape); QTest::qWait(300);}
      if (QQuickItem *box = findItem(win->contentItem(), "problemDialog")) box->setProperty("open", false);   // a scene that showed one does not leave it over the next
      if (QQuickItem *box = findItem(win->contentItem(), "speakerDialog")) box->setProperty("open", false);
      if (placeSceneDirty)
        {
          SettingsController *sc0 = SettingsController::instance();
          sc0->setGpsOn(false);
          sc0->setPlaceLookup(false);
          placeLookupPtr->setServiceUrl(realPlaceService);
          placeFake.reset();
          gpsPort = realGpsPort;
          for (const char *box : {"latLineEdit", "lonLineEdit", "placeLineEdit"})
            if (QLineEdit *e = txWidgetPtr->findChild<QLineEdit *>(box)) e->setText(QString());
          txWidgetPtr->slotGetParams();
          placeSceneDirty = false;
        }
      TxController::instance()->setTestSending(-1);
      if (savedTxMode >= 0 && scene != "tx-chips") {TxController::instance()->setMode(savedTxMode); savedTxMode = -1;}
      if (outputDeviceChanged && scene != "tx-digital-sending-device")
        {
          outputAudioDevice = savedOutputDevice;
          outputDeviceChanged = false;
          qmlUi::notifyRouteChanged();
        }
      // the scenes about the volume keys (#182): a phone volume and a device the keys act on in place of the phone's; the others have none
      {
        const bool volumeScene = scene.contains("volume") || scene.contains("mismatch");
        if (volumeScene)
          {
            const bool mismatch = scene.contains("mismatch");
            outputAudioDevice = mismatch ? QStringLiteral("Phone speaker") : QStringLiteral("default");   // chosen by hand: the keys are on Bluetooth; default: they follow
            volumeTest = true;
            const bool deviceScene = scene == "tx-volume-device";   // a long name of the device the keys act on (#203)
            app_->setTestVolume(true, deviceScene ? 60 : 70, mismatch ? QStringLiteral("Bluetooth: Pixel Buds") : deviceScene ? QStringLiteral("Bluetooth: Anker SoundCore") : QStringLiteral("Phone speaker"));
            qmlUi::notifyRouteChanged();
          }
        else if (volumeTest)
          {
            volumeTest = false;
            app_->setTestVolume(false);
            outputAudioDevice = savedOutputDevice;
            qmlUi::notifyRouteChanged();
          }
      }
      rx->setMode(scene == "rx-mode-fixed" || scene == "rx-modes-fixed" ? int(M1) : -1);   // Auto, except where the scene is about a chosen mode
      if (QQuickItem *v = findItem(win->contentItem(), "viewerScreen")) v->setProperty("infoOpen", false);
      GalleryModel::instance()->closeViewer();
      if (TouchTemplateEditor::instance()->isOpen()) TouchTemplateEditor::instance()->close();
      CalibrationController::instance()->leave();
      {
        // the frequencies and the DRM profiles of the scene: none, unless it is about them
        FrequencyController *fc = FrequencyController::instance();
        const bool freqScene = scene.startsWith("freq") || scene.contains("-freq");
        fc->setPickerOpen(false);
        fc->setTestList(freqScene && scene != "freq-empty");
        fc->setTestRadio(freqScene && scene != "freq-noradio", QStringLiteral("14.230 MHz USB"));
        DrmProfilesController::instance()->setTestProfiles(false);
      }
      rxClock = txClock = BASESAMPLERATE;
      {QSettings qs; qs.remove("Calibration/when"); qs.remove("Calibration/how"); qs.remove("Calibration/device");}
      AppModel::instance()->setSettingsOpen(false);
      AppModel::instance()->setMicDenied(scene == "mic-denied");
      AppModel::instance()->setFirstRun(scene.startsWith("firstrun"));
      if (QQuickItem *fr = findItem(win->contentItem(), "firstRunScreen")) fr->setProperty("step", scene == "firstrun-2" ? 1 : scene == "firstrun-3" ? 2 : 0);
      GalleryModel::instance()->clearSelection();
      app_->setScreen(0);
      app_->setDigital(scene == "digital" || scene.startsWith("tx-digital"));
      // the receiver does not run here: tell the screens what a running one would
      rx->setTestListening(scene == "stopped" ? 0 : 1);
      rx->setTestLevel(scene == "quiet" ? 0.1 : scene == "loud" ? 0.97 : 0.55);
      feedWaterfall(140);   // more rows than the waterfall keeps (128): it is full
      if (scene == "receiving" || scene == "done" || scene == "options")
        {
          const int w = 320, h = 256;
          rxWidgetPtr->getImageViewerPtr()->createImage(QSize(w, h), QColor(16, 20, 28), true);
          const int lines = scene == "receiving" ? 118 : h;
          *rxWidgetPtr->getImageViewerPtr()->getDisplayedImage() = testPicture(w, h, lines);
          qmlUi::rxImageStarted(w, h);
          qmlUi::rxStatus(false, "Receiving Martin 1");
          qmlUi::rxLine(lines);
          qmlUi::rxSync(20);
          if (scene == "done") qmlUi::rxStatus(false, "Saved");
        }
      else if (scene.startsWith("tx"))
        {
          app_->setScreen(1);
          TxController *tx = TxController::instance();
          tx->setUseTemplate(false);   // no template to begin with: the scenes after tx-template do not inherit its choice
          const bool framingScene = scene.contains("-framing");
          if (!framingScene) {tx->resetFraming(); tx->setFraming(0);}   // Stretch and the whole picture: the scenes after tx-framing do not inherit its choice (#177)
          if (scene != "tx")   // tx-picture, tx-modes, tx-sending, tx-details: a picture is loaded
            {
              const QString picFile = scratch + "/tx-test.png";
              (framingScene ? framingPicture() : testPicture(320, 256, 256)).save(picFile);
              txWidgetPtr->setImage(picFile);
              QTest::qWait(300);
            }
          if (scene == "tx-sending" || scene == "tx-route-follows" || scene == "tx-sending-mismatch" || scene.startsWith("tx-digital-sending")) tx->setTestSending(0.38);
          if (scene == "tx-digital-sending-device")
            {
              // a long name of a sound device (a USB radio interface): the chip that names it must not run out of the window either (#175)
              outputAudioDevice = "USB: Digirig Lite audio interface (USB-C)";
              outputDeviceChanged = true;
              qmlUi::notifyRouteChanged();
            }
          if (scene == "tx-sending-tool") tx->setTestSending(0.4, "Test tone, 1000 Hz", 5);
          if (scene == "tx-sending-sample") tx->setTestSending(0.4, "Test picture, B/W 8", 12);   // Settings > Test sound (#185)
          if (scene == "tx-chips")
            {
              savedTxMode = tx->mode();
              tx->setMode(int(M2));   // not a favourite: the extra chip appears next to the four
              QTest::qWait(300);
            }
          if (scene == "tx-mode")
            {
              // a mode chosen on the page (a chip, or a row of "All modes") takes effect at once (#170): the page's own combo box emits
              // no activated() for a change made in code, so the controller has to run what a choice there does
              const int from = tx->mode(), to = from == int(PD120) ? int(R36) : int(PD120);
              const QString timeBefore = tx->timeText();
              QSignalSpy changed(tx, &TxController::modeChanged);
              tx->setMode(to);
              auto fail = [&](const char *what) {fprintf(stderr, "FAIL: tx-mode: %s\n", what); exitCode = 1;};
              if (int(sstvModeIndexTx) != to) fail("the page's mode is still the old one");
              if (tx->mode() != to) fail("the controller's mode is still the old one");
              if (tx->modeName() != QString(SSTVTable[to].name)) fail("the mode name is not the new mode's");
              if (changed.isEmpty()) fail("no modeChanged");
              if (tx->timeText().isEmpty() || tx->timeText() == timeBefore) fail("the time of the new mode was not worked out");
              printf("  mode  %d -> %d, time \"%s\" -> \"%s\"\n", from, to, qPrintable(timeBefore), qPrintable(tx->timeText()));
              QTest::qWait(300);
            }
        }
      else if (scene.startsWith("settings") || scene == "speaker-many")
        {
          app_->setScreen(0);
          AppModel::instance()->setSettingsOpen(true);
          QQuickItem *st = findItem(win->contentItem(), "settingsScreen");
          const int page = scene == "settings-operator" ? 1 : scene == "settings-pictures" || scene == "settings-format" ? 2 : scene == "settings-position" || scene.startsWith("settings-location") ? 3 : scene == "settings-advanced" ? 4 : scene == "settings-audio" || scene == "settings-audio-mismatch" || scene == "settings-mic" || scene == "speaker-many" ? 5 : scene == "settings-radios" ? 6 : scene == "settings-receive" ? 7 : scene == "settings-cw" ? 8 : scene == "settings-appearance" ? 16 : 0;
          if (st) st->setProperty("page", page);
          if (scene == "settings-position")
            {
              // The system box that asks for the phone's position covers the program: Android makes it Inactive and mainWindow::writeSettings() runs.
              // It must not undo the switch that was just turned on (#187: the classic GPS page, hidden, was written back from its old check box, and the
              // position was off again once the box was answered). The same call, here, without the state change.
              auto fail = [&](const char *what) {fprintf(stderr, "FAIL: settings-position: %s\n", what); exitCode = 1;};
              SettingsController *sc = SettingsController::instance();
              const QString port0 = gpsPort;
              gpsPort = "/dev/qsstv-test-no-such-port";   // never a real serial port of this computer
              sc->setGpsOn(true);
              if (!sc->gpsOn()) fail("the switch did not turn the position on");
              mainWindowPtr->writeSettings();
              if (!sc->gpsOn()) fail("the position is off after the program wrote its settings (a system box was over it)");
              if (!gpsEnabled) fail("the GPS is off in the settings of the program after they were written");
              if (!QSettings().value("GPS/enabled").toBool()) fail("GPS/enabled in the settings file is off after they were written");
              sc->setGpsOn(false);   // the picture is of the page as it starts
              // the switch of the page shows the setting, whatever was tapped before: a tap asks for the other value, and a change that the page did
              // not make (here: the same write as above, gone wrong) is shown at once (a tapped switch used to lose its binding to the setting)
              QTest::qWait(300);
              QQuickItem *sw = st ? findShown(st, "rowSwitch") : nullptr;
              if (!sw) fail("the first switch of the page was not found");
              else
                {
                  QTest::mouseClick(win, Qt::LeftButton, {}, sw->mapToScene(QPointF(sw->width() / 2, sw->height() / 2)).toPoint());
                  QTest::qWait(300);
                  if (!sc->gpsOn()) fail("tapping the switch did not turn the position on");
                  if (!sw->property("checked").toBool()) fail("the switch is not on after the tap");
                  sc->setGpsOn(false);   // behind the page's back
                  QTest::qWait(100);
                  if (sw->property("checked").toBool()) fail("the switch shows on, but the setting went off (a tapped switch does not follow it)");
                }
              sc->setGpsOn(false);
              gpsPort = port0;
            }
          if (scene == "settings-location" || scene == "settings-location-failed")
            {
              // "Location now" (#188): flipping "Look up city and state" on asks for the place at once, and the row shows what the lookup does and finds.
              // The place service is a server of this harness; the position is what the phone's location source reports.
              auto fail = [&](const QString &what) {fprintf(stderr, "FAIL: %s: %s\n", qPrintable(scene), qPrintable(what)); exitCode = 1;};
              SettingsController *sc = SettingsController::instance();
              auto onPage = [&](const QString &word) {return st && shownText(st, word);};
              auto waitFor = [&](const std::function<bool()> &cond) {for (int t = 0; t < 40 && !cond(); t++) QTest::qWait(100); return cond();};
              placeFake = std::make_unique<fakeService>();
              const bool second = scene == "settings-location-failed";   // the lookup keeps its last answer for any position within a kilometre: each scene has a city of its own
              const double lat0 = second ? 46.8721 : 47.6062, lon0 = second ? -113.9940 : -122.3321;
              const QString city = second ? "Missoula, MT" : "Seattle, WA";
              placeFake->body = second ? missoula : seattle;
              placeLookupPtr->setServiceUrl(placeFake->url());
              gpsPort = "/dev/qsstv-test-no-such-port";   // never a real serial port of this computer
              placeSceneDirty = true;
              sc->setGpsOn(true);
              sc->setPlaceLookup(true);
              QTest::qWait(300);
              if (!onPage("Location now")) fail("the row is not on the page while the lookup is on");
              if (sc->placeSummary() != "Waiting for the position") fail("no position yet, the row says: " + sc->placeSummary());
              if (placeFake->hits != 0) fail("a lookup went out without a position");
              sc->setPlaceLookup(false);
              QTest::qWait(300);
              if (onPage("Location now")) fail("the row is on the page while the lookup is off");
              emit gpsReaderPtr->positionUpdated(lat0, lon0);   // a position, with the lookup off: nothing is asked
              QTest::qWait(300);
              if (placeFake->hits != 0) fail("a lookup went out with the lookup off");
              sc->setPlaceLookup(true);   // the flip: the place of the position that is there is asked for at once
              if (txWidgetPtr->placeLookupState() != txWidget::PLACELOOKING) fail("switching the lookup on did not start one");
              if (sc->placeSummary() != "Looking up the city and state") fail("while it looks, the row says: " + sc->placeSummary());
              if (!waitFor([&] {return sc->placeSummary() == city;})) fail("the row never shows the place: " + sc->placeSummary());
              if (placeFake->hits != 1) fail(QString("%1 requests for the first lookup").arg(placeFake->hits));
              if (scene == "settings-location-failed")
                {
                  placeFake->status = 500;
                  emit gpsReaderPtr->positionUpdated(10.5, 20.5);   // another area, and the service fails
                  if (!waitFor([&] {return txWidgetPtr->placeLookupState() == txWidget::PLACEFAILED;})) fail("the lookup did not fail");
                  if (!sc->placeSummary().startsWith("Could not look it up") || !sc->placeSummary().contains("500")) fail("a failed lookup says: " + sc->placeSummary());
                  emit gpsReaderPtr->positionUpdated(lat0, lon0);   // the next answer replaces the failure (a minute later in real life)
                  placeFake->status = 200;
                  txWidgetPtr->lookupPlaceNow();
                  if (!waitFor([&] {return sc->placeSummary() == city;})) fail("the failure stays after a lookup that works: " + sc->placeSummary());
                  placeFake->status = 500;
                  emit gpsReaderPtr->positionUpdated(10.5, 20.5);
                  waitFor([&] {return txWidgetPtr->placeLookupState() == txWidget::PLACEFAILED;});
                }
              QTest::qWait(300);
              if (!onPage("Location now") || !onPage(sc->placeSummary())) fail("the row on the page does not show: " + sc->placeSummary());
            }
          if (scene == "settings-appearance")
            {
              // the look is kept in the settings file at once, and comes back (the classic pages must not undo it)
              AppModel *am = AppModel::instance();
              const int t0 = am->theme();
              const bool k0 = am->tintPictures();
              am->setTheme(t0 == 3 ? 4 : 3);
              am->setTintPictures(!k0);
              QSettings qs;
              if (qs.value("Appearance/theme").toInt() != am->theme() || qs.value("Appearance/tintPictures").toBool() != am->tintPictures())
                {fprintf(stderr, "FAIL: the look is not in the settings file\n"); exitCode = 1;}
              am->setTheme(t0);
              am->setTintPictures(k0);
              if (QSettings().value("Appearance/theme").toInt() != t0) {fprintf(stderr, "FAIL: the look did not come back\n"); exitCode = 1;}
            }
          if (scene == "settings-format" && st) QMetaObject::invokeMethod(st, "openFormat");
          if (scene == "settings-mic" && st) QMetaObject::invokeMethod(st, "openMic");
        }
      else if (scene.startsWith("cal-"))
        {
          // the calibration pages: no sound device and no network here, the page is shown in a state of the controller
          CalibrationController *cal = CalibrationController::instance();
          AppModel::instance()->setSettingsOpen(true);
          QQuickItem *st = findItem(win->contentItem(), "settingsScreen");
          if (scene == "cal-overview-done" || scene == "cal-reset")
            {
              rxClock = txClock = BASESAMPLERATE * (1.0 + 12.9e-6);
              QSettings qs;
              qs.setValue("Calibration/when", QDate(2026, 11, 7).toString(Qt::ISODate));
              qs.setValue("Calibration/how", 1);
              qs.setValue("Calibration/device", QStringLiteral("phone microphone and speaker"));
              emit cal->clocksChanged();
            }
          emit cal->clocksChanged();
          int page = 9, method = 0, phase = 0;
          if (scene.startsWith("cal-ntp")) {page = 10; method = 1; phase = scene == "cal-ntp-early" ? 1 : scene == "cal-ntp-good" ? 2 : scene == "cal-ntp-problem" ? 3 : 0;}
          else if (scene.startsWith("cal-wwv")) {page = 11; method = 2; phase = scene == "cal-wwv-measuring" ? 2 : 0;}
          else if (scene.startsWith("cal-sstv")) {page = 12; method = 3; cal->setSstvRole(scene.startsWith("cal-sstv-send") ? 1 : 0); phase = scene == "cal-sstv-got" ? 2 : scene == "cal-sstv-sending" ? 1 : 0;}
          if (st) st->setProperty("page", page);
          if (method) cal->setTestState(method, phase);
          if (scene == "cal-reset" && st) {QTest::qWait(300); if (QQuickItem *ov = findItem(win->contentItem(), "calibrationOverview")) QMetaObject::invokeMethod(ov, "openReset");}
        }
      else if (scene.startsWith("drm-profile") || scene.startsWith("freq"))
        {
          AppModel::instance()->setSettingsOpen(true);
          QQuickItem *st = findItem(win->contentItem(), "settingsScreen");
          int page = 13;
          if (scene.startsWith("drm-profile"))
            {
              DrmProfilesController::instance()->setTestProfiles(scene != "drm-profiles-factory");
              if (scene == "drm-profile") page = 14;
            }
          else page = 15;
          if (st) st->setProperty("page", page);
          QTest::qWait(300);
          // what the pages keep must be in the settings file at once (the classic pages write it back at quit, and must not undo it)
          {
            QSettings qs;
            if (scene == "drm-profiles" && (qs.value("DRMPROFILE/drmPF1Name").toString() != "Robust" || qs.value("DRMPROFILE/drmPF3QAM").toInt() != 2))
              {fprintf(stderr, "FAIL: the DRM profiles are not in the settings file\n"); exitCode = 1;}
            if (scene == "freq" && (qs.value("FREQSELECT/frequencyList").toStringList().size() != 6 || qs.value("FREQSELECT/modeList").toStringList().count("DRM") != 1))
              {fprintf(stderr, "FAIL: the frequencies are not in the settings file\n"); exitCode = 1;}
          }
          if (scene == "freq-edit" || scene == "freq-new")
            if (QQuickItem *fp = findItem(win->contentItem(), "frequenciesPage"))
              {
                if (scene == "freq-edit") QMetaObject::invokeMethod(fp, "openEdit", Q_ARG(QVariant, 0));
                else QMetaObject::invokeMethod(fp, "openNew");
              }
        }
      else if (scene.startsWith("tpl-"))
        {
          // the template editor: the call sign bar with a replay window, the text of the bar chosen
          TouchTemplateEditor *te = TouchTemplateEditor::instance();
          if (scene == "tpl-starters") {app_->setScreen(2); GalleryModel *g = GalleryModel::instance(); g->setTab(2); g->rescan();
                                        QTest::qWait(500);
                                        if (QQuickItem *gs = findItem(win->contentItem(), "galleryScreen")) QMetaObject::invokeMethod(gs, "harnessStarters");}
          else
            {
              te->newTemplate(3);
              for (const QVariant &l : te->layers())
                if (l.toMap().value("kind").toString() == "text") {te->select(l.toMap().value("id").toString()); break;}
              QTest::qWait(300);
              QQuickItem *ed = findItem(win->contentItem(), "templateEditorScreen");
              const QString what = scene.mid(4);
              if (what == "font")
                {
                  // the font of a text (#196): the list is the device's, a choice is one step to undo, it is saved and read back, a name this device
                  // lacks stays in the item and is reported as not installed
                  auto fail = [&](const QString &why) {fprintf(stderr, "FAIL: tpl-font: %s\n", qPrintable(why)); exitCode = 1;};
                  auto family = [&]() {return te->property("sel").toMap().value("fontFamily").toString();};
                  auto selectText = [&]() {for (const QVariant &l : te->layers()) if (l.toMap().value("kind").toString() == "text") {te->select(l.toMap().value("id").toString()); return;}};
                  QStringList fams;
                  bool otherSeen = false, orderOk = true;   // the fonts for other scripts come after all the others
                  for (const QVariant &v : te->fontFamilies())
                    {
                      const QVariantMap m = v.toMap();
                      fams << m.value("name").toString();
                      if (m.value("other").toBool()) otherSeen = true;
                      else if (otherSeen) orderOk = false;
                    }
                  if (!orderOk) fail("a font for another script comes before one for Latin");
                  QStringList seen;
                  for (const QString &f : fams) {if (seen.contains(f, Qt::CaseInsensitive)) fail("the list has " + f + " twice"); seen << f;}
                  if (fams.isEmpty()) fail("the list of fonts is empty");
                  else
                    {
                      const QString before = family();
                      QString pick = fams.first();
                      for (const QString &f : fams) if (f.compare(before, Qt::CaseInsensitive) != 0) {pick = f; break;}
                      te->setProp("fontFamily", pick);
                      if (family() != pick || !te->property("sel").toMap().value("fontInstalled").toBool()) fail("the chosen font " + pick + " is not in the item");
                      te->undo();
                      if (family() != before) fail("undo did not bring back " + before + " (got " + family() + ")");
                      te->redo();
                      if (family() != pick) fail("redo did not bring back " + pick);
                      te->setProp("fontFamily", "  No Such Font 196  ");
                      if (family() != "No Such Font 196" || te->property("sel").toMap().value("fontInstalled").toBool()) fail("a font this device lacks was not kept and reported");
                      te->setProp("fontFamily", pick);
                      if (!te->save("fonttest")) fail("save: " + te->lastError());
                      else
                        {
                          const QString file = templatesPath + "fonttest.qtempl";
                          if (!te->openFile(file)) fail("the saved template does not open");
                          else
                            {
                              selectText();
                              if (family() != pick) fail("the font did not come back from the file (" + family() + " instead of " + pick + ")");
                            }
                          QFile::remove(file);
                        }
                      printf("  font  %d fonts, %s -> %s, undo/redo, unknown name kept, saved and read back\n", int(fams.size()), qPrintable(before), qPrintable(pick));
                    }
                  te->newTemplate(3);   // the screenshot: a new template, the chosen font in its text
                  selectText();
                  if (!fams.isEmpty()) te->setProp("fontFamily", fams.at(fams.size() / 3));
                }
              if (what == "font-missing")
                {
                  // a font this device lacks: the chip is amber and the list says so (#196)
                  te->setProp("fontFamily", "Gone Font 196");
                  if (te->property("sel").toMap().value("fontInstalled").toBool()) {fprintf(stderr, "FAIL: tpl-font-missing: a font that does not exist is reported as installed\n"); exitCode = 1;}
                }
              if (what == "oldtext")
                {
                  // a template from before the stroke had a side: its text has a stroke centered on the edge of the letters. Opening it moves the
                  // stroke outside, as one undo step, with a note (#202)
                  auto fail = [&](const QString &why) {fprintf(stderr, "FAIL: tpl-oldtext: %s\n", qPrintable(why)); exitCode = 1;};
                  auto whites = [&](const tpl::TemplateDocument &doc) {
                    QImage im(320, 256, QImage::Format_ARGB32_Premultiplied);
                    im.fill(Qt::transparent);
                    QPainter p(&im);
                    tpl::paintDocument(p, doc, QRectF(0, 0, 320, 256), te->renderOptions());
                    p.end();
                    int n = 0;
                    for (int y = 0; y < im.height(); y++)
                      for (int x = 0; x < im.width(); x++)
                        if (qAlpha(im.pixel(x, y)) > 250 && qRed(im.pixel(x, y)) > 250) n++;
                    return n;
                  };
                  tpl::TemplateDocument src;
                  src.canvas = QRectF(0, 0, 320, 256);
                  tpl::Element t;
                  t.type = tpl::ItemType::Text;
                  t.id = "text-1";
                  t.text = "WA1";
                  t.pos = QPointF(30, 150);
                  t.fill = Qt::white;
                  t.outline.color = Qt::black;
                  t.outline.width = 8;
                  t.outline.join = Qt::RoundJoin;
                  t.font.family = "Roboto";
                  t.font.pointSize = 60;
                  t.font.weight = 700;
                  const QString path = templatesPath + "oldtext.qtempl";
                  {
                    QFile f(path);
                    if (!f.open(QIODevice::WriteOnly)) fail("cannot write the template");
                    src.items << t;
                    f.write(tpl::toJson(src));
                  }
                  tpl::TemplateDocument bare = src;
                  bare.items[0].outline.style = Qt::NoPen;
                  const int letters = whites(bare), centered = whites(src);
                  if (centered >= letters) fail(QString("a centered stroke does not eat into the letters (%1 white pixels, %2 without a stroke)").arg(centered).arg(letters));
                  QSignalSpy note(te, &TouchTemplateEditor::openNote);
                  if (!te->openFile(path)) fail("the template does not open: " + te->lastError());
                  if (note.count() != 1) fail("no note about the strokes that moved");
                  if (!te->property("dirty").toBool() || !te->property("canUndo").toBool()) fail("the template is not marked as changed, with a step to undo");
                  const int outside = whites(te->document());
                  if (qAbs(outside - letters) > letters / 50) fail(QString("the letters did not keep their size: %1 white pixels, %2 without a stroke").arg(outside).arg(letters));
                  te->undo();
                  if (te->property("dirty").toBool() || whites(te->document()) != centered) fail("undo did not bring back the centered stroke");
                  te->redo();
                  if (!te->property("dirty").toBool() || whites(te->document()) != outside) fail("redo did not move it again");
                  tpl::TemplateDocument fine = src;
                  fine.items[0].outline.position = tpl::StrokePos::Outside;
                  {
                    QFile f(templatesPath + "fine.qtempl");
                    if (!f.open(QIODevice::WriteOnly)) fail("cannot write the template");
                    f.write(tpl::toJson(fine));
                  }
                  if (!te->openFile(templatesPath + "fine.qtempl") || te->property("dirty").toBool() || note.count() != 1) fail("a template with nothing to move opens changed, or with a note");
                  QFile::remove(templatesPath + "fine.qtempl");
                  te->openFile(path);   // the screenshot: the template with the text chosen
                  QFile::remove(path);
                  te->select("text-1");
                  printf("  oldtext  letters %d white pixels, centered stroke %d, outside %d, note, undo, redo\n", letters, centered, outside);
                }
              if (what.startsWith("fill") || what == "color")
                {
                  // the color bar of the Add sheet: a rectangle with a fade. The Style tab changes its fill, its gradient, its outline and its shadow (#201)
                  te->newTemplate(0);
                  te->addItem("bar", 160, 230);
                  auto fail = [&](const QString &why) {fprintf(stderr, "FAIL: tpl-%s: %s\n", qPrintable(what), qPrintable(why)); exitCode = 1;};
                  auto sel = [&]() {return te->property("sel").toMap();};
                  auto stops = [&]() {return sel().value("gradientStops").toList();};
                  auto stopColor = [&](int i) {return stops().at(i).toMap().value("color").toString();};
                  auto stopPos = [&](int i) {return stops().at(i).toMap().value("pos").toDouble();};
                  // what the canvas draws at a point of the bar: the template laid over nothing
                  auto pixel = [&](int x, int y) {
                    QImage im(320, 256, QImage::Format_ARGB32_Premultiplied);
                    im.fill(0);
                    QPainter p(&im);
                    tpl::paintDocument(p, te->document(), QRectF(0, 0, 320, 256), te->renderOptions());
                    p.end();
                    return QColor::fromRgba(im.pixel(x, y));
                  };
                  if (what == "fill")
                    {
                      if (sel().value("fillKind").toString() != "linear" || stops().size() < 2) fail("a color bar is not a linear gradient of two stops or more");
                      const QVariantList was = stops();
                      const double a0 = pixel(160, 224).alphaF(), a1 = pixel(160, 254).alphaF();
                      if (qAbs(a0 - a1) < 0.3) fail(QString("the bar does not fade (alpha %1 at the inner edge, %2 at the outer)").arg(a0).arg(a1));
                      te->setProp("fillKind", "solid");
                      if (sel().value("fillKind").toString() != "solid") fail("Solid did not switch the gradient off");
                      const QColor solid = pixel(160, 238);
                      if (solid.alpha() < 150) fail("the solid bar is as faint as the faded end of the gradient (alpha " + QString::number(solid.alpha()) + ")");
                      te->setProp("fill", "#00ff00");   // a swatch: the color only
                      const QColor green = pixel(160, 238);
                      if (green.green() < 200 || green.red() > 40) fail(QString("the solid fill is not green: %1,%2,%3").arg(green.red()).arg(green.green()).arg(green.blue()));
                      if (!qFuzzyCompare(pixel(160, 224).alphaF() + 1, pixel(160, 254).alphaF() + 1)) fail("a solid bar fades");
                      if (qAbs(QColor(sel().value("fill").toString()).alpha() - solid.alpha()) > 2) fail("a swatch changed the opacity of the fill");
                      te->setProp("fill", "#4000ff00");   // the chooser: with its own opacity (a=0x40)
                      if (QColor(sel().value("fill").toString()).alpha() != 0x40) fail("the chooser's opacity was not taken");
                      te->setProp("fill", "#ff00ff00");   // fully opaque must be taken too, not mistaken for a swatch
                      if (QColor(sel().value("fill").toString()).alpha() != 255) fail("an opaque color from the chooser kept the old opacity");
                      te->setProp("fillKind", "linear");
                      if (stops() != was) fail("the gradient did not come back as it was");
                      te->undo();   // back to solid
                      if (sel().value("fillKind").toString() != "solid") fail("undo of Linear did not bring back Solid");
                      te->redo();
                      // the stops
                      te->setGradientStopColor(0, "#ff112233");
                      if (stopColor(0) != "#ff112233") fail("a color of a stop: " + stopColor(0));
                      te->setGradientStopColor(0, "#ff4455");   // a swatch keeps the opacity of the stop
                      if (QColor(stopColor(0)).alpha() != 255 || QColor(stopColor(0)).red() != 0xff) fail("a swatch for a stop: " + stopColor(0));
                      te->setGradientStopPos(0, 0.95);
                      if (stopPos(0) > stopPos(1) + 1e-9) fail("a stop moved past its neighbor");
                      te->setGradientStopPos(0, 0.0);
                      const int n0 = stops().size();
                      const int added = te->addGradientStop(0);
                      if (stops().size() != n0 + 1 || added != 1 || !(stopPos(1) > stopPos(0) && stopPos(1) < stopPos(2))) fail("Add did not put a color between the first and the next");
                      const int last = te->addGradientStop(stops().size() - 1);
                      if (last != stops().size() - 2) fail("Add at the last stop did not go before it");
                      te->removeGradientStop(last);
                      te->removeGradientStop(1);
                      if (stops().size() != n0) fail("Remove did not bring back the number of stops");
                      te->removeGradientStop(0);
                      if (stops().size() != 2) fail("Remove went below two stops");
                      const QString first = stopColor(0), second = stopColor(1);
                      te->reverseGradient();
                      if (stopColor(0) != second || stopColor(1) != first || qAbs(stopPos(0) - (1 - stopPos(1))) > 1e-9) fail("Reverse");
                      te->setProp("gradientAngle", 400);
                      if (sel().value("gradientAngle").toInt() != 359) fail("the angle is not kept to 359");
                      te->setProp("gradientAngle", 90);
                      te->setProp("fillKind", "radial");
                      if (sel().value("fillKind").toString() != "radial") fail("Radial");
                      te->setProp("fillKind", "conical");
                      if (sel().value("fillKind").toString() != "conical") fail("Conical");
                      te->setProp("fillKind", "linear");
                      // the text: a gradient replaces a picture in the letters; a line has no gradient
                      // the outline: switching it on gives a line that can be seen
                      if (sel().value("outlineOn").toBool()) fail("a color bar starts with an outline");
                      te->setProp("outlineOn", true);
                      if (!sel().value("outlineOn").toBool() || sel().value("outlineWidth").toDouble() < 3) fail("an outline switched on is a hairline");
                      te->setProp("outlineWidth", 1);
                      te->setProp("outlineColor", "#ff0000");
                      if (sel().value("outlineWidth").toDouble() != 1) fail("the outline color changed the width that was chosen");
                      if (QColor(sel().value("outlineColor").toString()) != QColor(255, 0, 0)) fail("outline color " + sel().value("outlineColor").toString());
                      // it is a stroke: it lies OUTSIDE the edge of the bar (the top edge is at y = 220.16), not half of it inside
                      const QColor above = pixel(160, 219), below = pixel(160, 221);
                      if (above.red() < 200 || above.green() > 60 || above.alpha() < 150) fail(QString("no stroke above the top edge of the bar: %1,%2,%3 alpha %4").arg(above.red()).arg(above.green()).arg(above.blue()).arg(above.alpha()));
                      {
                        // inside the edge the picture is the bar's own: the same with the stroke switched off
                        tpl::TemplateDocument bare = te->document();
                        for (tpl::Element &it : bare.items) it.outline.style = Qt::NoPen;
                        QImage im(320, 256, QImage::Format_ARGB32_Premultiplied);
                        im.fill(0);
                        QPainter bp(&im);
                        tpl::paintDocument(bp, bare, QRectF(0, 0, 320, 256), te->renderOptions());
                        bp.end();
                        const QColor was = QColor::fromRgba(im.pixel(160, 221));
                        if (qAbs(was.red() - below.red()) > 3 || qAbs(was.green() - below.green()) > 3 || qAbs(was.alpha() - below.alpha()) > 3) fail("the stroke is on the bar, not outside it");
                      }
                      // the shadow: color, distance and blur
                      te->setProp("shadow", true);
                      te->setProp("shadowColor", "#ff0000");
                      if (QColor(sel().value("shadowColor").toString()).red() != 255 || QColor(sel().value("shadowColor").toString()).alpha() != 160) fail("shadow color " + sel().value("shadowColor").toString());
                      te->setProp("shadowColor", "#80102030");
                      if (QColor(sel().value("shadowColor").toString()).alpha() != 0x80) fail("the opacity of the shadow color");
                      te->setProp("shadowDx", 7); te->setProp("shadowDy", -5); te->setProp("shadowBlur", 12);
                      if (sel().value("shadowDx").toDouble() != 7 || sel().value("shadowDy").toDouble() != -5 || sel().value("shadowBlur").toDouble() != 12) fail("shadow distance and blur");
                      te->setProp("shadowBlur", 1e9); te->setProp("shadowDx", -1e9);
                      if (sel().value("shadowBlur").toDouble() > 100 || sel().value("shadowDx").toDouble() < -200) fail("the shadow is not kept in bounds");
                      te->setProp("shadowDx", 7); te->setProp("shadowBlur", 12);
                      // saved and read back
                      const QVariantMap before = sel();
                      if (!te->save("filltest")) fail("save: " + te->lastError());
                      else
                        {
                          const QString file = templatesPath + "filltest.qtempl";
                          if (!te->openFile(file)) fail("the saved template does not open");
                          else
                            {
                              for (const QVariant &l : te->layers()) if (l.toMap().value("kind").toString() == "rect") {te->select(l.toMap().value("id").toString()); break;}
                              for (const char *k : {"fillKind", "gradientAngle", "gradientStops", "shadowColor", "shadowDx", "shadowDy", "shadowBlur", "outlineColor", "outlineWidth"})
                                if (sel().value(k) != before.value(k)) fail(QString("%1 did not come back from the file").arg(k));
                            }
                          QFile::remove(file);
                        }
                      // a rectangle, new: its fill shows its own opacity in the color the editor reports (a swatch ring has to find it)
                      te->addItem("rect", 100, 100);
                      if (QColor(sel().value("fill").toString()).alpha() != 150 || QColor(sel().value("fill").toString()).red() != 0) fail("the fill of a new rectangle is read as " + sel().value("fill").toString());
                      // a text takes a gradient as well, and a line has none
                      te->addItem("text", 100, 100);
                      te->setProp("fillKind", "radial");
                      if (sel().value("fillKind").toString() != "radial" || stops().size() < 2) fail("a text did not take a gradient");
                      te->setProp("fillKind", "solid");
                      if (sel().value("fillKind").toString() != "solid") fail("a text did not go back to Solid");
                      te->addItem("line", 100, 100);
                      if (sel().value("hasFill").toBool()) fail("a line has a fill");
                      printf("  fill  solid/gradient, %d stops edited, outline, shadow, undo, saved and read back\n", n0);
                      te->newTemplate(0);
                      te->addItem("bar", 160, 230);
                    }
                  if (what == "fill-shadow") te->setProp("shadow", true);
                }
              if (ed && what != "editor" && !what.startsWith("drag")) QMetaObject::invokeMethod(ed, "harness", Q_ARG(QVariant, what == "font-missing" ? QString("font") : (what == "fill" || what == "oldtext") ? QString("style") : what == "fill-stops" ? QString("style@0.45") : what == "fill-shadow" ? QString("style@1") : what == "color" ? QString("color:stop") : what));
              if (ed && what.startsWith("drag"))
                {
                  // a finger on the handle at the bottom right of the chosen item, pulled outward: it must grow, and the finger must keep the handle
                  // (the item under it, or the move of the whole item, must not take the gesture)
                  const QVariantMap s0 = te->property("sel").toMap();
                  const double w0 = s0.value("w").toDouble() + s0.value("fontSize").toDouble();
                  QVariant pv; QMetaObject::invokeMethod(ed, "harnessPoint", Q_RETURN_ARG(QVariant, pv), Q_ARG(QVariant, 4));
                  const QPoint p0 = pv.toPointF().toPoint();
                  QTest::mousePress(win, Qt::LeftButton, Qt::NoModifier, p0);
                  for (int k = 1; k <= 10; k++) {QTest::mouseMove(win, p0 - QPoint(0, 0) + QPoint(2 * k, 6 * k)); QTest::qWait(20);}
                  QTest::mouseRelease(win, Qt::LeftButton, Qt::NoModifier, p0 + QPoint(20, 60));
                  QTest::qWait(400);
                  const QVariantMap s1 = te->property("sel").toMap();
                  const double w1 = s1.value("w").toDouble() + s1.value("fontSize").toDouble();
                  const bool moved = s1.value("x").toDouble() != s0.value("x").toDouble() || s1.value("y").toDouble() != s0.value("y").toDouble();
                  printf("  drag  size %.1f -> %.1f, corner moved: %s, can undo %d (%s)\n", w0, w1, moved ? "YES" : "no", te->property("canUndo").toBool(), w1 > w0 && !moved ? "grew" : "FAILED");
                  if (!(w1 > w0) || moved) exitCode = 1;
                }
            }
        }
      else if (scene == "saved")
        {
          const QDir dir(rxSSTVImagesPath);
          qmlUi::rxImageSaved(dir.entryInfoList(QDir::Files).first().absoluteFilePath());   // "Saved to Gallery" with View
        }
      else if (scene.startsWith("gallery"))
        {
          app_->setScreen(2);
          GalleryModel *g = GalleryModel::instance();
          g->clearSelection();
          g->closeViewer();
          g->setTab(scene == "gallery-sent" ? 1 : scene == "gallery-templates" ? 2 : 0);
          g->rescan();
          if (scene == "gallery-empty") {g->setTab(1); QDir(txSSTVImagesPath).removeRecursively(); QDir(txDRMImagesPath).removeRecursively(); g->rescan();}
          QTest::qWait(600);
          if (scene == "gallery-select") {g->startSelecting(1); g->toggle(3);}
          if (scene == "gallery-viewer" || scene == "gallery-info") g->setViewerIndex(1);
          if (scene == "gallery-delete") {QTest::qWait(500); g->setViewerIndex(1); QTest::qWait(700); g->deleteIndex(1); QTest::qWait(900);}
          if (scene == "gallery-info") {QTest::qWait(500); if (QQuickItem *v = findItem(win->contentItem(), "viewerScreen")) v->setProperty("infoOpen", true);}
        }
      QTest::qWait(400);
      QQuickItem *content = win->contentItem();
      if (scene == "level-steady")
        {
          // the picture keeps its size whatever the input level (#171: a warning line under the meter used to make the card taller and the picture smaller)
          QQuickItem *canvas = findItem(content, "pictureCanvas");
          if (!canvas) {fprintf(stderr, "FAIL: level-steady: no pictureCanvas item\n"); exitCode = 1;}
          else
            {
              qreal first = -1;
              for (qreal level : {0.55, 0.1, 0.97, 0.55})   // good, too quiet, too loud, good again
                {
                  rx->setTestLevel(level);
                  QTest::qWait(300);
                  if (first < 0) first = canvas->height();
                  printf("  level %.2f: the picture is %.0f dp high, the meter says \"%s\"\n", level, canvas->height(), qPrintable(rx->levelWord()));
                  if (!qFuzzyCompare(canvas->height() + 1, first + 1)) {fprintf(stderr, "FAIL: level-steady: the picture changed size with the level (%.0f -> %.0f)\n", first, canvas->height()); exitCode = 1;}
                }
              rx->setTestLevel(0.97);   // the screenshot: the worst case
              QTest::qWait(300);
            }
        }
      if (scene == "tx-chips")
        {
          // the favourite modes are ONE row that scrolls sideways when it is wider than the screen (#174), and the chosen mode is in view
          TxController *tx = TxController::instance();
          auto fail = [&](const char *what) {fprintf(stderr, "FAIL: tx-chips: %s\n", what); exitCode = 1;};
          QQuickItem *chips = findItem(content, "modeChips"), *other = findItem(content, "otherModeChip");
          if (!chips || !other) fail("no modeChips / otherModeChip item");
          else
            {
              // a short screen: the page scrolls up and down, bring the row into view as a finger would
              for (QQuickItem *p = chips->parentItem(); p; p = p->parentItem())
                if (QString::fromLatin1(p->metaObject()->className()) == "QQuickFlickable" && p->property("contentHeight").toDouble() > p->height())
                  {
                    QQuickItem *ci = qvariant_cast<QQuickItem *>(p->property("contentItem"));
                    const qreal y = chips->mapToItem(ci, QPointF(0, 0)).y();
                    p->setProperty("contentY", qBound(0.0, y - 60, p->property("contentHeight").toDouble() - p->height()));
                    break;
                  }
              QTest::qWait(500);   // the chosen chip slides into view
              printf("  chips: row %.0f dp high, content %.0f in a view of %.0f, x = %.0f\n", chips->height(), chips->property("contentWidth").toDouble(), chips->width(), chips->property("contentX").toDouble());
              if (!qFuzzyCompare(chips->height() + 1, 48.0 + 1)) fail("the row is not one row high");
              QQuickItem *row = nullptr;
              for (QQuickItem *c : qvariant_cast<QQuickItem *>(chips->property("contentItem"))->childItems())
                if (QString::fromLatin1(c->metaObject()->className()).startsWith("QQuickRow")) row = c;
              if (!row) fail("no Row in the scroller");
              else
                {
                  qreal y0 = -1;
                  int shown = 0;
                  for (QQuickItem *c : row->childItems())
                    if (c->isVisible() && c->width() > 0)
                      {
                        ++shown;
                        if (y0 < 0) y0 = c->y();
                        else if (!qFuzzyCompare(c->y() + 1, y0 + 1)) fail("the chips are not on one line");
                      }
                  if (shown != 5) fail("not the four favourites and the extra chip");
                }
              const qreal over = chips->property("contentWidth").toDouble() - chips->width();
              if (width <= 360 && over <= 0) fail("at this width the chips should be wider than the view (so they scroll)");
              const QPointF p = other->mapToItem(chips, QPointF(0, 0));
              if (p.x() < -0.5 || p.x() + other->width() > chips->width() + 0.5) fail("the chosen mode's chip is out of view");
              if (over > 0)
                {
                  // a drag scrolls the row, a tap still chooses a mode
                  const double x1 = chips->property("contentX").toDouble();
                  const QPoint from = chips->mapToScene(QPointF(chips->width() * 0.5, chips->height() * 0.5)).toPoint();
                  QTest::mousePress(win, Qt::LeftButton, {}, from);
                  for (int k = 1; k <= 12; k++) QTest::mouseMove(win, from + QPoint(k * 12, 0));
                  QTest::mouseRelease(win, Qt::LeftButton, {}, from + QPoint(144, 0));
                  QTest::qWait(900);
                  const double x2 = chips->property("contentX").toDouble();
                  printf("  drag right: x %.0f -> %.0f\n", x1, x2);
                  if (x1 > 0 && !(x2 < x1)) fail("a drag to the right did not scroll the row back");
                  if (tx->mode() != int(M2)) fail("a drag changed the mode");
                  QQuickItem *martin = nullptr;
                  for (QQuickItem *c : row->childItems())
                    if (c->isVisible() && c->property("text").toString() == QString(SSTVTable[M1].name)) martin = c;
                  if (!martin) fail("no Martin 1 chip");
                  else
                    {
                      const QPoint at = martin->mapToScene(QPointF(martin->width() * 0.5, martin->height() * 0.5)).toPoint();
                      QTest::mouseClick(win, Qt::LeftButton, {}, at);
                      QTest::qWait(300);
                      if (tx->mode() != int(M1)) fail("a tap on a chip did not choose its mode");
                    }
                  tx->setMode(int(M2));   // the screenshot: the extra chip again, scrolled into view
                  QTest::qWait(900);
                }
            }
        }
      if (scene == "tx-framing")
        {
          // Stretch, Crop, Fit and Free for a picture of another shape than the mode's, the zoom and move of Free by finger, and Reset (#177)
          TxController *tx = TxController::instance();
          auto fail = [&](const char *what) {fprintf(stderr, "FAIL: tx-framing: %s\n", what); exitCode = 1;};
          const QColor green("#20c040"), blue("#2060c0");
          const QColor grey = imageBackGroundColor;
          auto at = [&](double fx, double fy) {const QImage f = tx->picture(); return QColor(f.pixel(qBound(0, int(fx * f.width()), f.width() - 1), qBound(0, int(fy * f.height()), f.height() - 1)));};
          auto is = [](const QColor &a, const QColor &b) {return qAbs(a.red() - b.red()) + qAbs(a.green() - b.green()) + qAbs(a.blue() - b.blue()) < 100;};
          auto pageBox = [&]() {QComboBox *b = txWidgetPtr->findChild<QComboBox *>(QStringLiteral("sstvResizeComboBox")); return b ? b->currentIndex() : -1;};
          auto frame = [&](int mode) {tx->setFraming(mode); QTest::qWait(200); tx->picture().save(QString("%1/framing-%2x%3-%4.png").arg(outDir).arg(width).arg(height).arg(mode)); return tx->picture();};   // the frames are kept for looking at
          // the framing is a pill on the picture (#178): it names the framing, and the hint on how to place the picture is there while Free is not moved
          auto pillWord = [&]() {QQuickItem *p = findShown(content, "framingChip"); return p ? p->property("text").toString() : QString();};
          auto hintShown = [&]() {return findShown(content, "framingHint") != nullptr;};
          // the width of the white square in the middle of the picture, in the middle row of a frame
          auto whiteRun = [](const QImage &f) {
            int best = 0, run = 0;
            for (int x = 0; x < f.width(); x++)
              {
                const QColor c = QColor(f.pixel(x, f.height() / 2));
                run = (c.red() > 220 && c.green() > 220 && c.blue() > 220) ? run + 1 : 0;
                best = qMax(best, run);
              }
            return best;
          };
          int whiteBefore = 0;
          if (tx->picture().isNull()) fail("no picture to frame");
          printf("  frame %d x %d, picture 400 x 200\n", tx->picture().width(), tx->picture().height());
          if (!tx->framingChoice()) fail("no choice of framing in an analog mode");
          frame(0);   // Stretch: everything of the picture, the green column at the left edge, no bars
          if (tx->framing() != 0 || pageBox() != 0) fail("Stretch is not the page's framing");
          if (pillWord() != QStringLiteral("Stretch") || hintShown()) fail("the pill does not say Stretch, or there is a hint to place the picture");
          if (shownText(content, QStringLiteral("Framing"))) fail("the word Framing is still on the screen");
          if (!is(at(0.005, 0.5), green) || !is(at(0.5, 0.01), blue)) fail("Stretch does not fill the frame with the whole picture");
          frame(1);   // Crop: fills the frame, the edges are cut off (a 2:1 picture in a frame near 5:4)
          if (tx->framing() != 1 || pageBox() != 1) fail("Crop is not the page's framing");
          if (pillWord() != QStringLiteral("Crop") || hintShown()) fail("the pill does not say Crop, or there is a hint to place the picture");
          if (is(at(0.005, 0.5), green) || !is(at(0.5, 0.01), blue)) fail("Crop does not cut the edges and fill the frame");
          frame(2);   // Fit: the whole picture, bars above and below
          if (tx->framing() != 2 || pageBox() != 2) fail("Fit is not the page's framing");
          if (pillWord() != QStringLiteral("Fit") || hintShown()) fail("the pill does not say Fit, or there is a hint to place the picture");
          // the bars of Fit are empty (they go out black); the ones of Free are the background color of the pictures
          if (!is(at(0.005, 0.5), green) || !is(at(0.5, 0.01), QColor(Qt::black)) || !is(at(0.5, 0.99), QColor(Qt::black))) fail("Fit does not show the whole picture between bars");
          if (tx->framingFree() || tx->framingMoved()) fail("Fit is placed by hand");
          frame(3);   // Free: as Fit to begin with
          if (tx->framing() != 3 || pageBox() != 3) fail("Free is not the page's framing");
          if (!tx->framingFree() || tx->framingMoved()) fail("Free is not ready to be placed, or already moved");
          if (pillWord() != QStringLiteral("Free") || !hintShown()) fail("the pill does not say Free, or there is no hint to place the picture");
          if (findShown(content, "framingReset")) fail("a Reset pill for a picture that is not moved");
          if (!is(at(0.005, 0.5), green) || !is(at(0.5, 0.01), grey)) fail("Free does not begin with the whole picture");
          whiteBefore = whiteRun(tx->picture());
          // the picture moves with a finger: one drag with the mouse on the picture
          QQuickItem *mover = findShown(content, "pictureMover");
          if (!mover) fail("no pictureMover item");
          else
            {
              revealItem(mover);
              const QPoint from = mover->mapToScene(QPointF(mover->width() / 2, mover->height() / 2)).toPoint();
              QTest::mousePress(win, Qt::LeftButton, {}, from);
              for (int k = 1; k <= 12; k++) {QTest::mouseMove(win, from + QPoint(0, k * 8)); QTest::qWait(10);}
              QTest::mouseRelease(win, Qt::LeftButton, {}, from + QPoint(0, 96));
              QTest::qWait(500);
              printf("  drag down: moved %d, bottom bar is %s\n", tx->framingMoved(), is(at(0.5, 0.99), grey) ? "still there" : "gone");
              if (!tx->framingMoved()) fail("a drag on the picture did not move it");
              else if (is(at(0.5, 0.99), grey)) fail("the picture did not move down");
              if (hintShown()) fail("the hint to place the picture stays after the picture was moved");
              if (!findShown(content, "framingReset")) fail("no Reset pill after the picture was moved");
              tx->resetFraming();
              QTest::qWait(200);
              if (tx->framingMoved() || !is(at(0.5, 0.99), grey)) fail("Reset did not bring the whole picture back");
              // and two fingers pinch it larger: the columns at the edges go out of the frame
              QPointingDevice *touch = QTest::createTouchDevice();
              auto seq = QTest::touchEvent(win, touch);
              const int d0 = 30;
              seq.press(0, from + QPoint(-d0, 0), win).press(1, from + QPoint(d0, 0), win);
              seq.commit();
              for (int k = 1; k <= 12; k++)
                {
                  seq.move(0, from + QPoint(-d0 - k * 6, 0), win).move(1, from + QPoint(d0 + k * 6, 0), win);
                  seq.commit();
                  QTest::qWait(15);
                }
              QTest::qWait(150);
              const QImage live = tx->picture();   // while the fingers are down
              seq.release(0, from + QPoint(-d0 - 72, 0), win).release(1, from + QPoint(d0 + 72, 0), win);
              seq.commit();
              QTest::qWait(500);
              // what is seen while pinching is what the picture is when the fingers lift (it used to be the frame zoomed, and then drawn again smaller)
              const int liveWhite = whiteRun(live), finalWhite = whiteRun(tx->picture());
              printf("  pinch: the white square is %d px wide while pinching, %d px after (and %d px before the pinch)\n", liveWhite, finalWhite, whiteBefore);
              if (qAbs(liveWhite - finalWhite) > 3) fail("the picture is not the same while pinching and after the fingers lift");
              if (finalWhite < whiteBefore * 2) fail("the pinch did not zoom in");
              printf("  pinch: moved %d, the green column is %s\n", tx->framingMoved(), is(at(0.005, 0.5), green) ? "still in the frame" : "out of the frame");
              if (!tx->framingMoved() || is(at(0.005, 0.5), green)) fail("a pinch on the picture did not zoom it");
            }
          // the Reset button is in the screen while the picture is moved, and a tap on it puts the picture back
          QQuickItem *reset = findShown(content, "framingReset");
          if (!reset || !reset->isVisible()) fail("no Reset button while the picture is moved");
          else
            {
              revealItem(reset);
              QTest::mouseClick(win, Qt::LeftButton, {}, reset->mapToScene(QPointF(reset->width() / 2, reset->height() / 2)).toPoint());
              QTest::qWait(300);
              if (tx->framingMoved()) fail("the Reset button did not put the picture back");
            }
          // a zoom about a point keeps that point where it was: about one at the left edge the green column stays in the frame (about the middle it would go out)
          tx->resetFraming();
          QTest::qWait(200);
          tx->adjustFraming(2, 0.05, 0.5, 0, 0);
          QTest::qWait(200);
          if (!is(at(0.05, 0.5), green)) fail("a zoom about a point does not keep the point where it was");
          // the whole picture moves, not only what was in the frame: zoomed in the green edge is out of the frame, moved as far as it goes it is in again
          tx->resetFraming();
          QTest::qWait(200);
          tx->adjustFraming(3, 0.5, 0.5, 0, 0);
          QTest::qWait(200);
          if (is(at(0.02, 0.5), green)) fail("zoomed in, the green edge of the picture should be out of the frame");
          tx->adjustFraming(1, 0.5, 0.5, 1.0, 0);
          QTest::qWait(200);
          if (!is(at(0.02, 0.5), green)) fail("moved as far as it goes, the edge of the picture that was out of the frame does not come in");
          tx->resetFraming();
          QTest::qWait(200);
          // a tap on the framing pill lists the choices (the chosen one checked); a tap on Stretch chooses it, and the picture is not moved by the taps
          QQuickItem *pill = findShown(content, "framingChip");
          if (!pill) fail("no framing pill on the picture");
          else
            {
              revealItem(pill);
              QTest::mouseClick(win, Qt::LeftButton, {}, pill->mapToScene(QPointF(pill->width() / 2, pill->height() / 2)).toPoint());
              QTest::qWait(500);
              QQuickItem *stretch = findShown(win->contentItem(), "framingStretch"), *free = findShown(win->contentItem(), "framingFree");
              if (!stretch || !free) fail("a tap on the framing pill did not list the choices");
              else
                {
                  if (!free->property("marked").toBool() || stretch->property("marked").toBool()) fail("the list does not check the chosen framing (Free)");
                  QTest::mouseClick(win, Qt::LeftButton, {}, stretch->mapToScene(QPointF(stretch->width() / 2, stretch->height() / 2)).toPoint());
                  QTest::qWait(400);
                  if (tx->framing() != 0) fail("a tap on Stretch in the list did not choose Stretch");
                  if (pillWord() != QStringLiteral("Stretch")) fail("the pill does not follow the choice");
                  if (tx->framingMoved()) fail("the taps on the pill moved the picture");
                  if (findShown(win->contentItem(), "framingStretch")) fail("the list stays open after a choice");
                }
            }
          // while a transmission goes on the picture is not placed
          tx->setFraming(3);
          QTest::qWait(200);
          tx->setTestSending(0.3);
          tx->setFraming(1);
          if (tx->framing() != 3) fail("the framing was changed during a transmission");
          if (tx->adjustFraming(2, 0.5, 0.5, 0, 0)) fail("the picture was zoomed during a transmission");
          QTest::qWait(300);
          if (hintShown()) fail("the hint to place the picture is shown during a transmission");
          if (QQuickItem *p = findShown(content, "framingChip")) {if (p->isEnabled()) fail("the framing pill can be tapped during a transmission");}
          tx->setTestSending(-1);
          QTest::qWait(300);
          // the screenshot: Free, zoomed a little, with the hint and Reset
          if (!tx->adjustFraming(1.6, 0.5, 0.5, 0.05, 0)) fail("no zoom by program");
          QTest::qWait(400);
          if (!tx->framingMoved()) fail("not moved after a zoom");
          if (pageBox() != 3) fail("the page's framing box was left in another position");
        }
      if (scene == "tx-framing-free" || scene == "tx-framing-menu")
        {
          // Free and not moved yet: the hint is on the picture (the screenshot); tx-framing-menu leaves the list of the framing pill open (the screenshot)
          TxController *tx = TxController::instance();
          auto fail = [&](const char *what) {fprintf(stderr, "FAIL: %s: %s\n", qPrintable(scene), what); exitCode = 1;};
          tx->setFraming(3);
          QTest::qWait(400);
          QQuickItem *hint = findShown(content, "framingHint");
          if (!hint) fail("no hint to place the free picture");
          else if (hint->mapToScene(QPointF(0, 0)).x() < 0 || hint->mapToScene(QPointF(hint->width(), 0)).x() > width) fail("the hint runs out of the window");
          if (scene == "tx-framing-menu")
            {
              QQuickItem *pill = findShown(content, "framingChip");
              if (!pill) fail("no framing pill");
              else
                {
                  QTest::mouseClick(win, Qt::LeftButton, {}, pill->mapToScene(QPointF(pill->width() / 2, pill->height() / 2)).toPoint());
                  QTest::qWait(500);
                  QQuickItem *crop = findShown(win->contentItem(), "framingCrop");
                  if (!crop) fail("the list of choices is not open");
                  else if (crop->mapToScene(QPointF(0, 0)).x() < 0 || crop->mapToScene(QPointF(crop->width(), 0)).x() > width) fail("the list runs out of the window");
                }
            }
        }
      if (scene == "tx-digital-framing")
        {
          // a single digital picture is always Free: no choice, zoom and move only (#177)
          TxController *tx = TxController::instance();
          auto fail = [&](const char *what) {fprintf(stderr, "FAIL: tx-digital-framing: %s\n", what); exitCode = 1;};
          QTest::qWait(1500);   // the picture is encoded
          if (tx->framingChoice()) fail("a choice of framing for a single digital picture");
          if (tx->framing() != 3) fail("a single digital picture is not Free");
          if (!tx->framingFree()) fail("a single digital picture cannot be placed");
          if (findShown(content, "framingChip")) fail("a framing pill for a single digital picture");
          if (!findShown(content, "framingHint")) fail("no hint to place the digital picture");
          tx->setFraming(0);
          if (tx->framing() != 3) fail("the framing of a single digital picture was changed");
          const int before = tx->pictureRevision();
          if (!tx->adjustFraming(1.5, 0.5, 0.5, 0, 0)) fail("no zoom of the digital picture");
          QTest::qWait(2500);   // it is drawn and encoded again once the movement has settled
          printf("  digital zoom: moved %d, picture revision %d -> %d\n", tx->framingMoved(), before, tx->pictureRevision());
          if (!tx->framingMoved()) fail("the digital picture is not moved after a zoom");
          if (!findShown(content, "framingReset")) fail("no Reset pill for the moved digital picture");
          if (findShown(content, "framingHint")) fail("the hint to place the digital picture stays after it was moved");
          if (tx->pictureRevision() == before) fail("the digital picture was not drawn again");
        }
      if (scene == "tx-sending" || scene == "tx-sending-mismatch" || scene.startsWith("tx-digital-sending"))
        {
          // the two chips under the ring (the mode, the way the sound goes) stay inside the window with the margin of the rest of the screen:
          // side by side, centered, when both fit, one under the other, each centered, when they do not (#175)
          auto fail = [&](const char *what) {fprintf(stderr, "FAIL: %s: %s\n", qPrintable(scene), what); exitCode = 1;};
          QTest::qWait(300);
          QQuickItem *mode = findItem(content, "sendModeChip"), *route = findItem(content, "sendRouteChip");
          if (!mode || !route) fail("no sendModeChip / sendRouteChip item");
          else
            {
              const QRectF m(mode->mapToScene(QPointF(0, 0)), QSizeF(mode->width(), mode->height()));
              const QRectF r(route->mapToScene(QPointF(0, 0)), QSizeF(route->width(), route->height()));
              printf("  chips: mode x %.0f..%.0f y %.0f, route x %.0f..%.0f y %.0f, window %d wide\n", m.left(), m.right(), m.top(), r.left(), r.right(), r.top(), width);
              for (const QRectF &c : {m, r})
                if (c.left() < 24 - 0.5 || c.right() > width - 24 + 0.5) fail("a chip is outside the margins of the window");
              if (m.intersects(r)) fail("the chips overlap");
              if (r.top() > m.bottom() - 0.5)   // stacked: each one centered
                {
                  for (const QRectF &c : {m, r})
                    if (qAbs((c.left() + c.right()) / 2 - width / 2.0) > 1.0) fail("a stacked chip is not centered");
                }
              else if (qAbs(m.left() - (width - r.right())) > 2.0) fail("the chips side by side are not centered");
              // the text under the chips ends above the Stop button (the picture gives way for it on a short screen)
              QQuickItem *hint = findItem(content, "sendHint"), *stop = findItem(content, "sendStop");
              if (!hint || !stop) fail("no sendHint / sendStop item");
              else
                {
                  const qreal hintBottom = hint->mapToScene(QPointF(0, hint->height())).y(), stopTop = stop->mapToScene(QPointF(0, 0)).y();
                  printf("  hint ends y %.0f, Stop button starts y %.0f\n", hintBottom, stopTop);
                  if (hintBottom > stopTop + 0.5) fail("the text runs behind the Stop button");
                }
            }
        }
      if (scene == "tx-route-follows")
        {
          // the route chip names the output device chosen in the settings, at once: the choice goes through the settings controller and the
          // sound restart (mainWindow::restartSound), which tells the screens (AppModel::routeChanged used to be emitted by nobody)
          auto fail = [&](const char *what) {fprintf(stderr, "FAIL: tx-route-follows: %s\n", what); exitCode = 1;};
          QQuickItem *route = findItem(content, "sendRouteChip");
          if (!route) fail("no sendRouteChip item");
          else
            {
              SettingsController *sc = SettingsController::instance();
              const QString was = sc->outputDevice(), now = QStringLiteral("USB: Test radio interface");
              const QString before = route->property("text").toString();
              sc->setOutputDevice(now);
              QTest::qWait(800);   // the sound restart is queued behind the tap that asked for it
              const QString changed = route->property("text").toString();
              sc->setOutputDevice(was);
              QTest::qWait(800);
              const QString back = route->property("text").toString();
              printf("  route chip: \"%s\" -> \"%s\" -> \"%s\"\n", qPrintable(before), qPrintable(changed), qPrintable(back));
              if (changed != now) fail("the route chip did not follow the new output device");
              if (back != before) fail("the route chip did not follow the old output device back");
              // there is no sound card here: the restarts said so in a "Soundcard error" box, which must not stay over the scenes that follow
              if (QQuickItem *box = findItem(win->contentItem(), "problemDialog")) box->setProperty("open", false);
              QTest::qWait(300);
            }
        }
      if (scene == "tx-send")
        {
          // the Send button: the whole width at the foot of a phone upright; on a screen held on its side it is in the right column, under the choices, no bigger
          // than a button needs to be, and the picture column has the height it freed (#205)
          auto fail = [&](const char *what) {fprintf(stderr, "FAIL: tx-send: %s\n", what); exitCode = 1;};
          QQuickItem *t = findItem(content, "transmitScreen"), *send = findShown(content, "sendBar"), *pickers = findShown(content, "pickers");
          if (!t || !send || !pickers || send->childItems().isEmpty()) fail("no transmitScreen / sendBar / pickers item");
          else
            {
              QTest::qWait(300);
              auto box = [&](QQuickItem *it) {return QRectF(it->mapToItem(t, QPointF(0, 0)), QSizeF(it->width(), it->height()));};
              const QRectF s = box(send), p = box(pickers), b = box(send->childItems().first());
              const bool wide = width >= 600 && width > height * 1.1;
              printf("  %s: Send bar x %.0f..%.0f y %.0f..%.0f, button %.0f x %.0f dp, the pictures end at y %.0f, the screen is %.0f x %.0f\n", wide ? "wide" : "narrow",
                     s.left(), s.right(), s.top(), s.bottom(), b.width(), b.height(), p.bottom(), t->width(), t->height());
              if (qAbs(s.bottom() - t->height()) > 0.5) fail("the Send bar is not at the foot of the screen");
              if (wide)
                {
                  if (s.left() < t->width() / 2 - 0.5 || s.right() > t->width() + 0.5) fail("wide: the Send bar is not in the right column");
                  if (b.height() > 48.5) fail("wide: the Send button is bigger than a button needs to be");
                  if (p.right() > t->width() / 2 + 0.5) fail("wide: the ways to get a picture are not in the left column");
                  if (p.bottom() > t->height() - 16 + 0.5) fail("wide: the picture column runs lower than 16 dp above the foot of the screen");
                  // on a low window the height is what limits the picture, so it must use all the room under the choices' bar (on a tall one the width limits it)
                  if (height < 500 && p.bottom() < t->height() - 16 - 2) fail("wide: the picture column does not use the height the Send button left");
                }
              else if (s.width() < t->width() - 0.5 || b.height() < 55.5) fail("narrow: the Send bar is not the whole width, 56 dp high");
            }
        }
      if (scene == "tx-volume-room")
        {
          // the volume card (the device, the percentage, the slider) is in view above the Send bar without scrolling the page (#178: the framing row under the
          // picture used to push it out of view). On a tall phone that is a failure; on a small one, with large text, or a phone on its side, it is only reported
          auto fail = [&](const char *what) {fprintf(stderr, "FAIL: tx-volume-room: %s\n", what); exitCode = 1;};
          QQuickItem *slider = findShown(content, "mediaVolumeSlider"), *send = findShown(content, "sendBar");
          if (!slider || !send) fail("no mediaVolumeSlider / sendBar item");
          else
            {
              for (QQuickItem *p = slider->parentItem(); p; p = p->parentItem())   // the page at its top, as a phone shows it after a start
                if (QString::fromLatin1(p->metaObject()->className()) == "QQuickFlickable") {p->setProperty("contentY", 0); break;}
              QTest::qWait(300);
              const qreal bottom = slider->mapToScene(QPointF(0, slider->height())).y(), top = send->mapToScene(QPointF(0, 0)).y();
              const bool tallPhone = width < 600 && height >= 800 && app_->fontScale() <= 1.0;
              printf("  volume slider bottom at %.0f, Send bar top at %.0f: %.0f dp %s (window %d x %d)\n", bottom, top, qAbs(top - bottom), bottom <= top ? "to spare" : "short", width, height);
              if (tallPhone && bottom > top) fail("the volume slider is below the Send bar on a tall phone without scrolling");
            }
        }
      if (scene == "tx-volume-device")
        {
          // the head of the volume card: the name of the sound device on the left, "Media volume 60 %" on the right. A long name (a Bluetooth speaker, a USB
          // interface) is cut with "..." and does not run under the percentage, which stays whole (#203)
          auto fail = [&](const char *what) {fprintf(stderr, "FAIL: tx-volume-device: %s\n", what); exitCode = 1;};
          auto check = [&](const char *label) {
            QTest::qWait(300);
            QQuickItem *route = findShown(content, "volumeRoute"), *level = findShown(content, "volumeLevel"), *slider = findShown(content, "mediaVolumeSlider");
            if (!route || !level || !slider) {fail("no volumeRoute / volumeLevel / mediaVolumeSlider item"); return;}
            revealItem(level);
            const QString name = route->property("text").toString();
            const QRectF r(route->mapToScene(QPointF(0, 0)), QSizeF(route->width(), route->height()));
            const QRectF l(level->mapToScene(QPointF(0, 0)), QSizeF(level->width(), level->height()));
            const qreal cardLeft = slider->mapToScene(QPointF(0, 0)).x(), cardRight = cardLeft + slider->width();
            const bool cut = route->property("truncated").toBool();
            printf("  %s: \"%s\" x %.0f..%.0f, \"%s\" x %.0f..%.0f, card x %.0f..%.0f, name %s\n", label, qPrintable(name), r.left(), r.right(),
                   qPrintable(level->property("text").toString()), l.left(), l.right(), cardLeft, cardRight, cut ? "cut" : "whole");
            if (l.left() < r.right() - 0.5 && l.top() < r.bottom() && r.top() < l.bottom()) fail("the name runs under the percentage");
            if (r.left() < cardLeft - 0.5 || l.right() > cardRight + 0.5) fail("the head of the card is wider than the card");
            if (level->property("text").toString() != QStringLiteral("Media volume 60 %")) fail("the percentage is not whole");
            if (level->implicitWidth() > level->width() + 0.5) fail("the percentage is cut");
            if (name.isEmpty()) fail("no name");
            // the same name is in the chip of the top bar: it gives way to the title and the menu button, which stay whole and inside the window
            QQuickItem *chip = findShown(content, "appBarRoute"), *menu = findShown(content, "appBarMenu"), *title = findShown(content, "appBarTitle");
            if (!chip || !menu || !title) {fail("no appBarRoute / appBarMenu / appBarTitle item"); return;}
            const QRectF c(chip->mapToScene(QPointF(0, 0)), QSizeF(chip->width(), chip->height())), m(menu->mapToScene(QPointF(0, 0)), QSizeF(menu->width(), menu->height())),
                         t(title->mapToScene(QPointF(0, 0)), QSizeF(title->width(), title->height()));
            printf("  %s: top bar: title x %.0f..%.0f, chip x %.0f..%.0f, menu x %.0f..%.0f, window %d wide\n", label, t.left(), t.right(), c.left(), c.right(), m.left(), m.right(), width);
            if (m.right() > width + 0.5) fail("the menu button of the top bar is outside the window");
            if (c.right() > m.left() + 0.5) fail("the route chip of the top bar runs over the menu button");
            if (c.left() < t.right() - 0.5) fail("the route chip of the top bar runs over the title");
            if (title->property("truncated").toBool()) fail("the title of the top bar is cut before the chip");
          };
          check("Anker SoundCore");
          app_->setTestVolume(true, 60, QStringLiteral("Bluetooth: Anker SoundCore Liberty 4 NC Wireless Earbuds"));   // longer than any phone is wide
          qmlUi::notifyRouteChanged();
          check("longer name");
          if (QQuickItem *route = findShown(content, "volumeRoute"); route && !route->property("truncated").toBool()) fail("the very long name is not cut");
        }
      if (scene == "tx-volume")
        {
          // the volume card has its slider; while the picture is sent the percentage on the sending screen follows the volume keys (#182): the screen
          // behind it is covered then, and nothing used to look at the volume. Where the sound follows the system default it names the device the keys act on
          TxController *tx = TxController::instance();
          auto fail = [&](const char *what) {fprintf(stderr, "FAIL: tx-volume: %s\n", what); exitCode = 1;};
          if (!findShown(content, "mediaVolumeSlider")) fail("no volume slider on the Transmit screen");
          if (findShown(content, "volumeNote")) fail("a note about the volume keys where the sound goes where they act");
          tx->setTestSending(0.38);
          QTest::qWait(400);
          QQuickItem *route = findShown(content, "sendRouteChip");
          if (!route) fail("no sendRouteChip item");
          else
            {
              const QString first = route->property("text").toString();
              app_->setTestKeyPress(40);   // the keys were pressed
              QTest::qWait(300);
              const QString unseen = route->property("text").toString();
              app_->pollVolume();   // the program looks at the volume by itself every moment (a timer on the phone)
              QTest::qWait(300);
              const QString seen = route->property("text").toString();
              app_->setTestVolume(true, 40, QStringLiteral("Bluetooth: Pixel Buds"));   // the system default is now Bluetooth: the keys act on it, and the sound goes there
              QTest::qWait(300);
              const QString bluetooth = route->property("text").toString();
              printf("  route chip: \"%s\" -> keys pressed \"%s\" -> looked \"%s\" -> Bluetooth \"%s\"\n", qPrintable(first), qPrintable(unseen), qPrintable(seen), qPrintable(bluetooth));
              if (first != QStringLiteral("Phone speaker  70 %")) fail("the chip does not say the device and the volume");
              if (unseen != first) fail("the volume changed without anybody looking (the test is not what it says)");
              if (seen != QStringLiteral("Phone speaker  40 %")) fail("the percentage did not follow the volume keys while sending");
              if (bluetooth != QStringLiteral("Bluetooth: Pixel Buds  40 %")) fail("the system default is not named by the device the keys act on");
              if (app_->volumeMismatch()) fail("a mismatch where the sound follows the system default");
            }
        }
      if (scene == "tx-volume-mismatch")
        {
          // Phone speaker chosen by hand while the volume keys act on Bluetooth: the card says so instead of showing a slider that changes the wrong
          // device, and "Use system default" puts the sound where the keys act (#182)
          auto fail = [&](const char *what) {fprintf(stderr, "FAIL: tx-volume-mismatch: %s\n", what); exitCode = 1;};
          QQuickItem *note = findShown(content, "volumeNote"), *text = findShown(content, "volumeNoteText");
          if (!note || !text) fail("no note about the volume keys");
          else
            {
              const QString said = text->property("text").toString();
              printf("  note: \"%s\"\n", qPrintable(said));
              if (!said.contains("Bluetooth: Pixel Buds") || !said.contains("Phone speaker")) fail("the note does not name both devices");
            }
          if (findShown(content, "mediaVolumeSlider")) fail("a volume slider that changes the wrong device");
          if (!app_->volumeMismatch()) fail("no mismatch");
          QQuickItem *button = findShown(content, "volumeNoteSwitch");
          if (!button) fail("no Use system default button");
          else
            {
              QTest::qWait(300);
              revealItem(button);
              QTest::mouseClick(win, Qt::LeftButton, {}, button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint());
              QTest::qWait(900);   // the sound restart is queued behind the tap that asked for it
              printf("  after the button: output \"%s\", mismatch %d\n", qPrintable(outputAudioDevice), app_->volumeMismatch());
              if (outputAudioDevice != QStringLiteral("default")) fail("the button did not choose the system default");
              if (app_->volumeMismatch() || findShown(content, "volumeNote")) fail("the note stays after the sound follows the keys");
              if (!findShown(content, "mediaVolumeSlider")) fail("no volume slider after the sound follows the keys");
              if (app_->routeText() != QStringLiteral("Bluetooth: Pixel Buds")) fail("the route is not named by the device the keys act on");
              // there is no sound card here: the restart said so in a "Soundcard error" box, which must not stay over the scenes that follow
              if (QQuickItem *box = findItem(win->contentItem(), "problemDialog")) box->setProperty("open", false);
              QTest::qWait(300);
              // the screenshot: the mismatch again
              outputAudioDevice = QStringLiteral("Phone speaker");
              app_->setTestVolume(true, 70, QStringLiteral("Bluetooth: Pixel Buds"));
              qmlUi::notifyRouteChanged();
              QTest::qWait(300);
            }
        }
      if (scene == "tx-sending-mismatch")
        {
          // on the sending screen the percentage of the wrong device is not shown, the note is, and there is no button (a new device would restart the sound)
          auto fail = [&](const char *what) {fprintf(stderr, "FAIL: tx-sending-mismatch: %s\n", what); exitCode = 1;};
          QTest::qWait(300);
          QQuickItem *route = findShown(content, "sendRouteChip");
          if (!route) fail("no sendRouteChip item");
          else if (route->property("text").toString() != QStringLiteral("Phone speaker")) fail("the chip is not the device alone");
          if (!findShown(content, "volumeNote")) fail("no note about the volume keys while sending");
          if (findShown(content, "volumeNoteSwitch")) fail("a button that would restart the sound while sending");
        }
      if (scene == "settings-audio-mismatch")
        {
          auto fail = [&](const char *what) {fprintf(stderr, "FAIL: settings-audio-mismatch: %s\n", what); exitCode = 1;};
          QTest::qWait(300);
          QQuickItem *page = findShown(content, "settingsVolumeNote");
          if (!page) fail("no note about the volume keys on the Audio and radio page");
          else if (!findShown(page, "volumeNoteSwitch")) fail("no Use system default button there");
        }
      if (scene == "problem-ptt" || scene == "problem-ptt-short")
        {
          // a dialog that says what went wrong (#184): the PTT failure of a USB adapter is a long text, longer still with a large text size or on a short
          // screen. The dialog stays inside the window, the message scrolls, and the button that closes it is always there
          auto fail = [&](const char *what) {fprintf(stderr, "FAIL: %s: %s\n", qPrintable(scene), what); exitCode = 1;};
          const bool shortText = scene == "problem-ptt-short";
          const QString reason = shortText ? QStringLiteral("Serial PTT is switched on, but no serial port is chosen.")
                                           : QStringLiteral("The PTT serial port usb:0403:6001:0 cannot be opened (the permission was not given). Check that the adapter is plugged in and that the program may use it (the system asks), or switch Serial PTT off.");
          qmlUi::problem(QStringLiteral("PTT problem"), rigControl::pttProblemText(reason));   // the text of the program itself
          QTest::qWait(500);
          QQuickItem *card = findShown(content, "problemCard"), *ok = findShown(content, "problemOk"), *message = findShown(content, "problemMessage");
          if (!card || !ok) fail("no dialog or no OK button");
          else
            {
              const QRectF window(0, 0, win->width(), win->height());
              const QRectF c(card->mapToScene(QPointF(0, 0)), QSizeF(card->width(), card->height()));
              const QRectF o(ok->mapToScene(QPointF(0, 0)), QSizeF(ok->width(), ok->height()));
              printf("  dialog y %.0f..%.0f of a window %.0f high, OK y %.0f..%.0f%s\n", c.top(), c.bottom(), window.height(), o.top(), o.bottom(), message ? ", message scrolls" : "");
              if (c.top() < -0.5 || c.bottom() > window.height() + 0.5) fail("the dialog is outside the window");
              if (o.top() < c.top() - 0.5 || o.bottom() > c.bottom() + 0.5) fail("the OK button is outside the dialog");
              if (message)
                {
                  // the message scrolls to its last line, and the last line is inside the dialog then
                  const qreal content = message->property("contentHeight").toDouble(), shown = message->height();
                  if (shortText && app_->fontScale() <= 1.0 && content > shown + 0.5) fail("a short message that has to be scrolled");   // (with a very large text even a short one does)
                  message->setProperty("contentY", qMax(0.0, content - shown));
                  QTest::qWait(200);
                  if (message->property("contentY").toDouble() + shown < content - 1.0) fail("the end of the message cannot be reached");
                  message->setProperty("contentY", 0.0);
                  QTest::qWait(100);
                }
              // the button closes it
              QTest::mouseClick(win, Qt::LeftButton, {}, ok->mapToScene(QPointF(ok->width() / 2, ok->height() / 2)).toPoint());
              QTest::qWait(300);
              if (findShown(content, "problemCard")) fail("OK does not close the dialog");
              qmlUi::problem(QStringLiteral("PTT problem"), rigControl::pttProblemText(reason));   // for the screenshot
              QTest::qWait(300);
            }
        }
      if (scene == "speaker-many")
        {
          // a single-choice dialog with more options than fit: the options scroll, Cancel stays (#184). The Speaker dialog of the settings, with made-up devices
          auto fail = [&](const char *what) {fprintf(stderr, "FAIL: speaker-many: %s\n", what); exitCode = 1;};
          QQuickItem *dlg = findItem(content, "speakerDialog");
          if (!dlg) fail("no speaker dialog");
          else
            {
              // a few options: they all fit, nothing scrolls, and the dialog is as tall as they are
              dlg->setProperty("options", QVariantList{QStringLiteral("System default"), QStringLiteral("Phone speaker"), QStringLiteral("USB audio: Digirig")});
              dlg->setProperty("open", true);
              QTest::qWait(400);
              if (QQuickItem *few = findShown(content, "choiceList"))
                {
                  printf("  three options: %.0f high in a view of %.0f\n", few->property("contentHeight").toDouble(), few->height());
                  // (on a window so low that the dialog is held to its largest height, a few options may scroll too)
                  QQuickItem *fewCard = findShown(content, "choiceCard");
                  const bool atLargest = fewCard && fewCard->height() >= win->height() - 120 - 0.5;
                  if (!atLargest && few->property("contentHeight").toDouble() > few->height() + 0.5) fail("a few options that have to be scrolled");
                }
              else fail("no option list");
              dlg->setProperty("open", false);
              QTest::qWait(200);
              QVariantList devices;
              for (int i = 0; i < 14; i++) devices << QString("USB audio: Test interface %1 with a rather long name").arg(i + 1);
              dlg->setProperty("options", devices);
              dlg->setProperty("current", 2);
              dlg->setProperty("open", true);
              QTest::qWait(500);
              QQuickItem *card = findShown(content, "choiceCard"), *cancel = findShown(content, "choiceCancel"), *list = findShown(content, "choiceList");
              if (!card || !cancel) fail("no dialog or no Cancel button");
              else
                {
                  const QRectF c(card->mapToScene(QPointF(0, 0)), QSizeF(card->width(), card->height()));
                  const QRectF o(cancel->mapToScene(QPointF(0, 0)), QSizeF(cancel->width(), cancel->height()));
                  printf("  dialog y %.0f..%.0f of a window %d high, Cancel y %.0f..%.0f%s\n", c.top(), c.bottom(), win->height(), o.top(), o.bottom(), list ? ", options scroll" : "");
                  if (c.top() < -0.5 || c.bottom() > win->height() + 0.5) fail("the dialog is outside the window");
                  if (o.top() < c.top() - 0.5 || o.bottom() > c.bottom() + 0.5) fail("the Cancel button is outside the dialog");
                  if (list)
                    {
                      const qreal total = list->property("contentHeight").toDouble(), shown = list->height();
                      list->setProperty("contentY", qMax(0.0, total - shown));
                      QTest::qWait(200);
                      if (list->property("contentY").toDouble() + shown < total - 1.0) fail("the last option cannot be reached");
                      list->setProperty("contentY", 0.0);
                    }
                }
            }
        }
      if (scene == "tx-pointers")
        {
          // the texts that name a place of the program name one of the phone, not of the desktop (the Configuration dialog, the TX tab): the PTT problem, no sound
          // output, no sound device, and the macros of the template editor
          auto fail = [&](const char *what) {fprintf(stderr, "FAIL: tx-pointers: %s\n", what); exitCode = 1;};
          auto check = [&](const QString &what, const QString &text) {
            printf("  %s: \"%s\"\n", qPrintable(what), qPrintable(text.left(150).replace('\n', ' ')));
            if (text.contains("Configuration") || text.contains("TX tab") || text.contains("see configuration")) fail(qPrintable(what + " names a place of the desktop"));
          };
          check(QStringLiteral("PTT problem"), rigControl::pttProblemText(QStringLiteral("x")));
          check(QStringLiteral("no sound output"), txWidget::noSoundOutputText());
          check(QStringLiteral("no sound device"), soundBase::noDeviceText());
          if (!txWidget::noSoundOutputText().contains("Settings > Audio and radio") || !soundBase::noDeviceText().contains("Settings > Audio and radio")) fail("the sound texts do not name Settings > Audio and radio");
          int macros = 0;
          for (const QVariant &m : TouchTemplateEditor::instance()->macros())
            {
              check(QString("macro %%1").arg(m.toMap().value("tag").toString()), m.toMap().value("description").toString());
              macros++;
            }
          if (macros < 15) fail("the macro list is short");
        }
      if (scene == "firstrun-1")
        {
          // who is on the air (#186): First name, Last name and City are three fields, each one into its own setting (a full name used to go
          // all into the first name). The rows' own onCommitted handlers run, as when the field is left.
          auto fail = [&](const char *what) {fprintf(stderr, "FAIL: firstrun-1: %s\n", what); exitCode = 1;};
          const struct {const char *row, *text;} rows[] = {{"firstRunFirstName", "Jane"}, {"firstRunLastName", "Doe"}, {"firstRunCity", "Springfield"}};
          for (const auto &r : rows)
            {
              QQuickItem *row = findItem(win->contentItem(), r.row);
              if (!row) {fail(r.row); continue;}
              QMetaObject::invokeMethod(row, "committed", Q_ARG(QString, QString::fromLatin1(r.text)));
            }
          QTest::qWait(100);
          if (myFirstname != QStringLiteral("Jane")) fail("the first name is not in the first name");
          if (myLastname != QStringLiteral("Doe")) fail("the last name is not in the last name");
          if (myQth != QStringLiteral("Springfield")) fail("the city is not in the city");
          printf("  operator  first \"%s\", last \"%s\", city \"%s\"\n", qPrintable(myFirstname), qPrintable(myLastname), qPrintable(myQth));
          // the call sign also fills "Text sent as CW" (#192), which the CW tool and the CW at the end of a picture send, until the operator
          // writes a text of their own
          SettingsController *sc = SettingsController::instance();
          auto cwNow = [&]() {return sc->cwText();};
          auto cwFile = [&]() {return QSettings().value("CW/cwtext").toString();};
          auto callsign = [&](const char *text)
            {
              QQuickItem *row = findItem(win->contentItem(), "firstRunCallsign");
              if (!row) {fail("firstRunCallsign"); return;}
              QMetaObject::invokeMethod(row, "committed", Q_ARG(QString, QString::fromLatin1(text)));
              QTest::qWait(100);
            };
          if (!cwNow().isEmpty()) fail("the CW text is not empty before the call sign");
          callsign("w1aw");
          if (sc->callsign() != QStringLiteral("W1AW")) fail("the call sign is not in the call sign");
          if (cwNow() != QStringLiteral("W1AW") || cwFile() != QStringLiteral("W1AW")) fail("the call sign does not fill the CW text (and the settings file)");
          callsign("K1ABC");
          if (cwNow() != QStringLiteral("K1ABC")) fail("the CW text does not follow a changed call sign");
          sc->setCwText(QStringLiteral("CQ DE K1ABC"));
          callsign("N0XYZ");
          if (cwNow() != QStringLiteral("CQ DE K1ABC") || cwFile() != QStringLiteral("CQ DE K1ABC")) fail("a CW text of the operator's own was replaced");
          callsign(" ");
          if (cwNow() != QStringLiteral("CQ DE K1ABC")) fail("clearing the call sign cleared a CW text of the operator's own");
          sc->setCwText(QString());
          callsign("w1aw");
          callsign("");
          if (!cwNow().isEmpty()) fail("clearing the call sign did not clear the CW text that followed it");
          printf("  cw text   follows the call sign, keeps the operator's own text\n");
          SettingsController::instance()->setFirstName(QString());   // the screenshot, and the scenes that follow, start with empty fields
          SettingsController::instance()->setLastName(QString());
          SettingsController::instance()->setQth(QString());
          sc->setCallsign(QString());
          sc->setCwText(QString());
          QTest::qWait(100);
        }
      if (scene == "about") {win->setProperty("aboutOpen", true); QTest::qWait(500);}
      if (scene == "tx-tools")
        {
          QQuickItem *t = findItem(content, "transmitScreen");
          if (t) t->setProperty("toolsOpen", true);
          QTest::qWait(500);
        }
      if (scene == "tx-digital-details")
        {
          QQuickItem *t = findItem(content, "transmitScreen");
          if (t) t->setProperty("digitalOpen", true);
          QTest::qWait(500);
        }
      if (scene == "tx-details" || scene == "tx-template" || scene == "tx-template-none")
        {
          // the template (#176): choosing one from the list puts it on the picture and the Details row appears under it; None takes it off
          // (the row goes, the page's switch is off, the list ticks None) and the page keeps the template it had
          TxController *tx = TxController::instance();
          auto fail = [&](const char *what) {fprintf(stderr, "FAIL: %s: %s\n", qPrintable(scene), what); exitCode = 1;};
          auto ticked = [&]() {int n = 0; for (const QVariant &v : tx->templates()) if (v.toMap().value("current").toBool()) n++; return n;};
          auto detailsRowShown = [&]() {QQuickItem *r = findItem(content, "detailsRow"); return r && r->isVisible();};
          galleryWidgetPtr->changedMatrix(imageViewer::TEMPLATETHUMB);   // the page's list of templates reads the folder
          txWidgetPtr->setupTemplatesComboBox();
          tx->chooseTemplate(templatesPath + "synthetic.templ");
          QTest::qWait(300);
          if (!tx->property("useTemplate").toBool()) fail("choosing a template does not put it on the picture");
          if (tx->templateName() != QStringLiteral("synthetic")) fail("the template is not the one chosen");
          if (ticked() != 1) fail("the chosen template is not the one ticked");
          if (!detailsRowShown()) fail("no Details row while a template is on the picture");
          if (scene == "tx-template-none")
            {
              tx->setUseTemplate(false);   // what the None row of the list does
              QTest::qWait(300);
              QCheckBox *useBox = txWidgetPtr->findChild<QCheckBox *>(QStringLiteral("templateCheckBox"));
              if (tx->property("useTemplate").toBool() || !useBox || useBox->isChecked()) fail("None leaves the template on the picture");
              if (tx->templateName() != QStringLiteral("synthetic")) fail("None forgot the template of the page");
              if (detailsRowShown()) fail("a Details row with no template on the picture");
              printf("  template  synthetic -> None (the page keeps %s)\n", qPrintable(tx->templateName()));
            }
        }
      if (scene == "tx-modes" || scene == "tx-details" || scene == "tx-template" || scene == "tx-template-none")
        {
          QQuickItem *t = findItem(content, "transmitScreen");
          if (!t) fprintf(stderr, "no transmitScreen item\n");
          else if (!t->setProperty(scene == "tx-modes" ? "modesOpen" : scene.startsWith("tx-template") ? "templatesOpen" : "detailsOpen", true)) fprintf(stderr, "cannot set the sheet property\n");
          QTest::qWait(500);
        }
      if (scene.endsWith("-freq-sheet")) {FrequencyController::instance()->setPickerOpen(true); QTest::qWait(500);}
      if (scene == "options")
        {
          QQuickItem *r = findItem(content, "receiveScreen");
          if (r) r->setProperty("optionsOpen", true);
          QTest::qWait(500);
        }
      if (scene == "rx-defaults")
        {
          // a fresh profile starts with the slant correction on (#190) and saving on; a choice of the user is kept
          auto fail = [&](const char *what) {fprintf(stderr, "FAIL: rx-defaults: %s\n", what); exitCode = 1;};
          QCheckBox *box = rxWidgetPtr->findChild<QCheckBox *>("autoSlantAdjustCheckBox");
          if (!autoSlantAdjust || !rx->autoSlant()) fail("Correct slant is not on in a new profile");
          if (!box || !box->isChecked()) fail("the page's Auto Slant box is not checked");
          if (!autoSave || !rx->autoSave()) fail("saving pictures is not on");
          rx->setAutoSlant(false);
          if (autoSlantAdjust || rx->autoSlant() || (box && box->isChecked())) fail("switching it off did not hold");
          QTest::qWait(1500);   // the page writes its settings a second after the change
          if (!QSettings().contains("RX/autoSlantAdjust") || QSettings().value("RX/autoSlantAdjust").toBool()) fail("the off choice is not in the settings file");
          rx->setAutoSlant(true);
          if (!autoSlantAdjust || !rx->autoSlant()) fail("switching it on again did not hold");
          printf("  slant  on by default, off, on\n");
        }
      if (scene == "rx-mode")
        {
          // the receive mode chosen on the Mode chip's list (#173): the page's box gets it, the setting is saved, Auto comes back
          auto fail = [&](const char *what) {fprintf(stderr, "FAIL: rx-mode: %s\n", what); exitCode = 1;};
          if (rx->mode() != -1 || rx->modeName() != QStringLiteral("Auto") || int(sstvModeIndexRx) != 0) fail("not Auto to begin with");
          QSignalSpy changed(rx, &RxController::optionsChanged);
          rx->setMode(int(M1));
          if (int(sstvModeIndexRx) != int(M1) + 1) fail("the receiver's mode is still the old one");
          if (rx->mode() != int(M1)) fail("the controller's mode is still the old one");
          if (rx->modeName() != QString(SSTVTable[M1].name)) fail("the mode name is not the new mode's");
          if (changed.isEmpty()) fail("no optionsChanged");
          rx->setMode(-2);
          rx->setMode(int(NUMSSTVMODES));
          if (rx->mode() != int(M1)) fail("a mode out of range was taken");
          QTest::qWait(1500);   // the page writes its settings a second after the change
          if (QSettings().value("RX/sstvModeIndexRx").toInt() != int(M1) + 1) fail("the mode is not in the settings file");
          rx->setMode(-1);
          if (rx->mode() != -1 || rx->modeName() != QStringLiteral("Auto") || int(sstvModeIndexRx) != 0) fail("Auto did not come back");
          printf("  mode  Auto -> %s -> Auto\n", qPrintable(QString(SSTVTable[M1].name)));
        }
      if (scene == "rx-listen")
        {
          // the Listening / Stopped chip under the picture is the start / stop switch (#181): no button at the bottom, the waterfall reaches down
          auto fail = [&](const char *what) {fprintf(stderr, "FAIL: rx-listen: %s\n", what); exitCode = 1;};
          QQuickItem *r = findItem(content, "receiveScreen");
          if (!r) fail("no receiveScreen item");
          else
            {
              auto tap = [&](QQuickItem *it) {QTest::mouseClick(win, Qt::LeftButton, {}, it->mapToScene(QPointF(it->width() / 2, it->height() / 2)).toPoint()); QTest::qWait(300);};
              auto shown = [&](const char *name) {QQuickItem *it = findShown(r, name); return it && it->isVisible();};
              std::function<bool(QQuickItem *)> hasFab = [&](QQuickItem *it) {
                if (it->isVisible() && QString::fromLatin1(it->metaObject()->className()).startsWith("ExtendedFab")) return true;
                for (QQuickItem *c : it->childItems()) if (hasFab(c)) return true;
                return false;
              };
              qmlUi::rxLine(0);   // listening and idle (no picture coming in), whatever the scene before left
              qmlUi::rxImageEnded();
              qmlUi::rxStatus(false, "Listening");
              rx->setTestListening(1);
              QTest::qWait(300);
              if (hasFab(r)) fail("there is still a button at the bottom of Receive");
              if (!shown("listeningChip") || shown("stoppedChip")) fail("idle: the Listening chip is not the only one shown");
              if (QQuickItem *chip = findItem(r, "listeningChip")) tap(chip);
              if (rx->listening() || !shown("stoppedChip") || shown("listeningChip")) fail("a tap on Listening did not stop (Stopped should show)");
              if (QQuickItem *chip = findItem(r, "stoppedChip")) tap(chip);
              if (!rx->listening() || !shown("listeningChip") || shown("stoppedChip")) fail("a tap on Stopped did not start (Listening should show)");
              // a picture coming in: the chips that say what it is take the row, and the switch is not offered
              rxWidgetPtr->getImageViewerPtr()->createImage(QSize(320, 256), QColor(16, 20, 28), true);
              qmlUi::rxImageStarted(320, 256);
              qmlUi::rxStatus(false, "Receiving Martin 1");
              qmlUi::rxLine(100);
              QTest::qWait(300);
              if (!rx->receiving()) fail("the test picture is not being received");
              if (shown("listeningChip") || shown("stoppedChip")) fail("the switch is offered while a picture comes in");
              qmlUi::rxLine(0);
              qmlUi::rxImageEnded();
              qmlUi::rxStatus(false, "Listening");
              QTest::qWait(300);
              // the waterfall ends 16 dp above the bar (narrow) or level with the foot of the chips under the picture (wide)
              QQuickItem *strip = findItem(r, "spectrumStrip");
              if (!strip) fail("no spectrumStrip item");
              else
                {
                  const qreal bottom = strip->mapToItem(r, QPointF(0, strip->height())).y(), room = r->height() - bottom;
                  printf("  waterfall %.0f x %.0f dp, its foot %.1f dp above the foot of the screen\n", strip->width(), strip->height(), room);
                  if (room < 15.5) fail("the waterfall runs lower than 16 dp above the foot of the screen");
                  if (room > 17.5) fail("the waterfall does not reach down to 16 dp above the foot of the screen");
                  // the chips: under the picture on a phone upright; at the top of the right column, above the level meter, on a screen held on its side, where the
                  // picture is as high as the column and ends where the waterfall ends (#206)
                  QQuickItem *canvas = findItem(r, "pictureCanvas"), *meter = findItem(r, "levelMeter"), *chip = findShown(r, "listeningChip");
                  if (!canvas || !meter || !chip) fail("no pictureCanvas / levelMeter / listeningChip item");
                  else
                    {
                      auto box = [&](QQuickItem *it) {return QRectF(it->mapToItem(r, QPointF(0, 0)), QSizeF(it->width(), it->height()));};
                      const QRectF c = box(canvas), m = box(meter), k = box(chip);
                      const bool wide = width >= 600 && width > height * 1.1;
                      printf("  %s: picture y %.0f..%.0f, chips x %.0f y %.0f..%.0f, level meter y %.0f..%.0f, waterfall foot %.1f\n", wide ? "wide" : "narrow", c.top(), c.bottom(),
                             k.left(), k.top(), k.bottom(), m.top(), m.bottom(), bottom);
                      if (wide)
                        {
                          if (k.left() < c.right()) fail("wide: the chips are not in the right column");
                          if (k.bottom() > m.top()) fail("wide: the chips are not above the level meter");
                          if (qAbs(c.bottom() - bottom) > 0.5) fail("wide: the picture does not end where the waterfall ends");
                        }
                      else if (k.top() < c.bottom()) fail("narrow: the chips are not under the picture");
                    }
                }
            }
        }
      if (scene == "rx-mode-fixed" || scene == "rx-modes" || scene == "rx-modes-fixed")
        {
          qmlUi::rxLine(0);   // listening and idle (no picture coming in), whatever the scene before left
          qmlUi::rxImageEnded();
          qmlUi::rxStatus(false, "Listening");
          QQuickItem *r = findItem(content, "receiveScreen");
          if (scene != "rx-mode-fixed" && (!r || !r->setProperty("modesOpen", true))) fprintf(stderr, "cannot open the list of modes\n");
          QTest::qWait(500);
        }
      if (qEnvironmentVariableIsSet("QSSTV_QML_AUDIT")) auditTree(win->contentItem(), scene);
      QImage img = win->grabWindow();
      const QString file = QString("%1/qml-%2x%3-%4.png").arg(outDir).arg(width).arg(height).arg(scene);
      if (img.isNull() || !img.save(file)) {fprintf(stderr, "FAIL: no picture for %s\n", qPrintable(scene)); exitCode = 1;}
      else printf("  ok    %s (%dx%d)\n", qPrintable(file), img.width(), img.height());
    }
  fflush(stdout);
  _exit(exitCode);   // no shutdown: the hidden pages and threads are not worth unwinding here
}
