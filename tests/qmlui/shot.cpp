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
#include "appglobal.h"
#include "configparams.h"
#include "dispatcher.h"
#include "mainwindow.h"
#include "uimode.h"
#include "qmlui.h"
#include "appmodel.h"
#include "rxcontroller.h"
#include "txcontroller.h"
#include "txwidget.h"
#include "sstvparam.h"
#include "imageviewer.h"
#include "rxwidget.h"
#include "spectrumwidget.h"
#include "gallerymodel.h"
#include "settingscontroller.h"
#include "touchtemplateeditor.h"
#include "calibrationcontroller.h"
#include "drmprofilescontroller.h"
#include "frequencycontroller.h"
#include "soundconfig.h"
#include "directoriesconfig.h"

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
      TxController::instance()->setTestSending(-1);
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
      feedWaterfall(60);
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
          if (scene != "tx")   // tx-picture, tx-modes, tx-sending, tx-details: a picture is loaded
            {
              const QString picFile = scratch + "/tx-test.png";
              testPicture(320, 256, 256).save(picFile);
              txWidgetPtr->setImage(picFile);
              QTest::qWait(300);
            }
          if (scene == "tx-sending") tx->setTestSending(0.38);
          if (scene == "tx-sending-tool") tx->setTestSending(0.4, "Test tone, 1000 Hz", 5);
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
      else if (scene.startsWith("settings"))
        {
          app_->setScreen(0);
          AppModel::instance()->setSettingsOpen(true);
          QQuickItem *st = findItem(win->contentItem(), "settingsScreen");
          const int page = scene == "settings-operator" ? 1 : scene == "settings-pictures" || scene == "settings-format" ? 2 : scene == "settings-position" ? 3 : scene == "settings-advanced" ? 4 : scene == "settings-audio" || scene == "settings-mic" ? 5 : scene == "settings-radios" ? 6 : scene == "settings-receive" ? 7 : scene == "settings-cw" ? 8 : scene == "settings-appearance" ? 16 : 0;
          if (st) st->setProperty("page", page);
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
              if (ed && what != "editor" && !what.startsWith("drag")) QMetaObject::invokeMethod(ed, "harness", Q_ARG(QVariant, what));
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
      if (scene == "tx-template") {txWidgetPtr->setupTemplatesComboBox(); QTest::qWait(500);}
      if (scene == "tx-modes" || scene == "tx-details" || scene == "tx-template")
        {
          QQuickItem *t = findItem(content, "transmitScreen");
          if (!t) fprintf(stderr, "no transmitScreen item\n");
          else if (!t->setProperty(scene == "tx-modes" ? "modesOpen" : scene == "tx-template" ? "templatesOpen" : "detailsOpen", true)) fprintf(stderr, "cannot set the sheet property\n");
          QTest::qWait(500);
        }
      if (scene.endsWith("-freq-sheet")) {FrequencyController::instance()->setPickerOpen(true); QTest::qWait(500);}
      if (scene == "options")
        {
          QQuickItem *r = findItem(content, "receiveScreen");
          if (r) r->setProperty("optionsOpen", true);
          QTest::qWait(500);
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
