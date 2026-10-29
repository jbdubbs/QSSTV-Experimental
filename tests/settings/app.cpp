// The Transmit and Receive tab settings in the REAL application objects (main window, TX tab, RX tab), offscreen (issue #149):
//   app <scratch-dir> set|check
// "set" changes the settings the way a user does, waits for the program to save them and ends WITHOUT a clean quit (_exit: no
// closeEvent, no writeSettings(), like a Windows shutdown with the program open, End Task or a crash); "check" is the next start.
// Built and run by tests/settings/app.sh against an existing build-cmake tree (it links the application's object files).
#include <QtTest>
#include <QtWidgets>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
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
// the TX tab keeps its members private; the test reads them
#define private public
#include "txwidget.h"
#undef private

static int passed = 0, failed = 0;
static void check(bool ok, const QString &what)
{
  printf("  %s %s\n", ok ? "ok  " : "FAIL", qPrintable(what));
  ok ? passed++ : failed++;
}

static const esstvMode kMode = PD290;   // larger than 512x400, so the grid box is enabled

// an item of the box that neither of the two profiles has: the setting must come from the user, not from a profile
static int differentFrom(QComboBox *c, int a, int b)
{
  for (int i = 0; i < c->count(); i++)
    if (i != a && i != b) return i;
  return 0;
}

static void pick(QComboBox *c, int i)
{
  c->setCurrentIndex(i);
  emit c->activated(i);   // what a click in the list does
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
  if (argc < 3) return 2;
  const QString scratch = argv[1];
  const bool setPhase = QString(argv[2]) == "set";
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
  mainWindowPtr->startRunning(false);   // no sound card receiver
  QTest::qWait(300);

  txWidget *txw = txWidgetPtr;
  rxWidget *rxw = rxWidgetPtr;
  imageViewer *tx = txw->getImageViewerPtr();
  auto txBox = [&](const char *n) {return txw->findChild<QComboBox *>(n);};
  auto txCheck = [&](const char *n) {return txw->findChild<QCheckBox *>(n);};
  auto txLine = [&](const char *n) {return txw->findChild<QLineEdit *>(n);};
  auto txText = [&](const char *n) {return txw->findChild<QPlainTextEdit *>(n);};
  QComboBox *mode = txBox("sstvModeComboBox"), *framing = txBox("sstvResizeComboBox"), *drmFraming = txBox("drmResizeComboBox");
  QComboBox *grid = txBox("sstvGridComboBox"), *drmGrid = txBox("drmGridComboBox"), *profile = txBox("drmProfileComboBox");
  QComboBox *qam = txBox("drmTxQAMComboBox"), *bandwidth = txBox("drmTxBandwidthComboBox"), *encoding = txBox("drmEncodingComboBox");
  QCheckBox *vox = txCheck("voxCheckBox"), *cw = txCheck("cwCheckBox"), *hybrid = txCheck("hybridCheckBox"), *useTemplate = txCheck("templateCheckBox");
  QTabWidget *txTabs = txw->findChild<QTabWidget *>("settingsTableWidget");
  QLineEdit *toCall = txLine("toCallLineEdit"), *op = txLine("operatorLineEdit"), *rsv = txLine("rsvLineEdit");
  QLineEdit *lat = txLine("latLineEdit"), *lon = txLine("lonLineEdit"), *place = txLine("placeLineEdit");
  QPlainTextEdit *cx = txText("xPlainTextEdit"), *cy = txText("yPlainTextEdit"), *cz = txText("zPlainTextEdit");
  QComboBox *rxMode = rxw->findChild<QComboBox *>("sstvModeComboBox"), *sensitivity = rxw->findChild<QComboBox *>("sensitivityComboBox");
  QComboBox *rxFormat = rxw->findChild<QComboBox *>("defaultImageFormatComboBox");
  QCheckBox *autoSave = rxw->findChild<QCheckBox *>("autoSaveCheckBox"), *autoSlant = rxw->findChild<QCheckBox *>("autoSlantAdjustCheckBox");
  QSpinBox *complete = rxw->findChild<QSpinBox *>("completeSpinBox");
  QSlider *sizeSlider = txw->findChild<QSlider *>("sizeSlider");

  printf("== the tabs\n");
  check(mode && framing && drmFraming && grid && drmGrid && profile && qam && bandwidth && encoding && vox && cw && hybrid && useTemplate && txTabs,
        "the Transmit tab has the boxes");
  check(toCall && op && rsv && lat && lon && place && cx && cy && cz, "and the boxes of one QSO");
  check(rxMode && sensitivity && rxFormat && autoSave && autoSlant && complete, "the Receive tab has the boxes");
  if (!(mode && framing && drmFraming && grid && drmGrid && profile && qam && bandwidth && encoding && vox && cw && hybrid && useTemplate && txTabs
        && toCall && op && rsv && lat && lon && place && cx && cy && cz && rxMode && sensitivity && rxFormat && autoSave && autoSlant && complete && sizeSlider))
    finish();
  check(profile->count() >= 2 && qam->count() >= 3 && bandwidth->count() >= 3 && sensitivity->count() == 4, "(the boxes have the items the test uses)");

  const int qamPick = differentFrom(qam, drmPFArray[0].params.qam, drmPFArray[1].params.qam);
  const int bwPick = differentFrom(bandwidth, drmPFArray[0].params.bandwith, drmPFArray[1].params.bandwith);
  const int rxModePick = 5, rxFormatPick = rxFormat->count() - 1;
  const int encodingPick = avifWriteSupported() ? imageViewer::ENCAVIF : imageViewer::ENCJPEG2000;

  if (setPhase)
    {
      printf("\n== the settings are changed\n");
      check(!QSettings().contains("TX/sstvModeIndexTx"), "(a first run: nothing is saved yet)");
      pick(mode, kMode);
      pick(framing, 2);   // Fit
      pick(grid, 3);      // Grid 2x2
      vox->setChecked(true);
      cw->setChecked(true);
      hybrid->setChecked(true);
      useTemplate->setChecked(true);
      txTabs->setCurrentIndex(TRXDRM);
      pick(profile, 1);
      pick(qam, qamPick);
      pick(bandwidth, bwPick);
      pick(encoding, encodingPick);
      sizeSlider->setMaximum(60000);   // the range follows the picture; there is none here
      sizeSlider->setValue(12000);
      // what belongs to one QSO
      toCall->setText("N0CALL");
      op->setText("Pat");
      rsv->setText("595");
      lat->setText("47 36.37N");
      lon->setText("122 19.93W");
      place->setText("Seattle, WA");
      cx->setPlainText("one");
      cy->setPlainText("two");
      cz->setPlainText("three");
      for (QLineEdit *e : {toCall, op, rsv, lat, lon, place}) emit e->editingFinished();
      for (QPlainTextEdit *e : {cx, cy, cz}) e->setPlainText(e->toPlainText());
      txw->slotGetParams();
      // the receiver
      sensitivity->setCurrentIndex(3);
      autoSave->click();
      autoSlant->click();
      complete->setValue(60);
      rxFormat->setCurrentIndex(rxFormatPick);
      rxMode->setCurrentIndex(rxModePick);

      QTest::qWait(2500);   // the program saves a moment after the last change
      printf("\n== saved without a quit\n");
      QSettings qs;
      check(qs.value("TX/sstvModeIndexTx").toInt() == kMode, "the SSTV mode");
      check(qs.value("TX/framing").toInt() == 2 && qs.value("TX/gridLayout").toInt() == 3, "the framing and the grid");
      check(qs.value("TX/drmProfile").toInt() == 1 && qs.value("TX/drmQAM").toInt() == qamPick && qs.value("TX/drmBandWith").toInt() == bwPick, "the DRM profile and its options");
      check(qs.value("TX/drmImageEncoding").toInt() == encodingPick && qs.value("TX/compressedSize").toInt() == 12000,
            QString("the DRM encoding and size (%1 / %2, %3 / 12000)").arg(qs.value("TX/drmImageEncoding").toInt()).arg(encodingPick).arg(qs.value("TX/compressedSize").toInt()));
      check(qs.value("TX/useVOX").toBool() && qs.value("TX/useCW").toBool() && qs.value("TX/useHybrid").toBool() && qs.value("TX/useTemplate").toBool(), "VOX, CW, Hybrid and Template");
      check(qs.value("MAIN/transmissionModeIndex").toInt() == TRXDRM, "the SSTV / DRM tab");
      check(qs.value("RX/sensitivity").toInt() == 3 && !qs.value("RX/autoSave").toBool() && qs.value("RX/autoSlantAdjust").toBool(), "the receiver: sensitivity, auto save, auto slant");
      check(qs.value("RX/minCompletion").toInt() == 60 && qs.value("RX/defaultImageFormat").toString() == rxFormat->itemText(rxFormatPick)
            && qs.value("RX/sstvModeIndexRx").toInt() == rxModePick, "the receiver: save if complete, picture format, mode");
      check(!qs.contains("TX/latitude") && !qs.contains("TX/longitude") && !qs.contains("TX/place"), "not the position or the place");
      finish();
    }

  printf("\n== the next start\n");
  check(sstvModeIndexTx == kMode && mode->currentIndex() == kMode, QString("the SSTV mode is back (%1)").arg(mode->currentText()));
  check(framing->currentIndex() == 2 && drmFraming->currentIndex() == 2 && tx->fitMode() == 2, "the framing, in both boxes and in the picture");
  check(grid->currentIndex() == 3 && txw->gridLayout == 3, "the grid");
  check(vox->isChecked() && cw->isChecked() && hybrid->isChecked() && useTemplate->isChecked(), "VOX, CW, Hybrid and Template");
  check(txTabs->currentIndex() == TRXDRM && transmissionModeIndex == TRXDRM, "the DRM tab");
  check(profile->currentIndex() == 1, "the DRM profile");
  check(qam->currentIndex() == qamPick && bandwidth->currentIndex() == bwPick,
        QString("the DRM options are not replaced by profile 0 (QAM item %1, bandwidth item %2)").arg(qam->currentIndex()).arg(bandwidth->currentIndex()));
  check(encoding->currentIndex() == encodingPick && txw->compressedSize == 12000, "the DRM encoding and size");
  check(toCall->text().isEmpty() && op->text().isEmpty() && rsv->text().isEmpty(), "To, Op and RSV are empty");
  check(lat->text().isEmpty() && lon->text().isEmpty() && place->text().isEmpty(), "Lat, Long and Place are empty");
  check(cx->toPlainText().isEmpty() && cy->toPlainText().isEmpty() && cz->toPlainText().isEmpty(), "X, Y and Z are empty");
  check(sensitivity->currentIndex() == 3 && !autoSave->isChecked() && autoSlant->isChecked(), "the receiver: sensitivity, auto save, auto slant");
  check(complete->value() == 60 && rxFormat->currentIndex() == rxFormatPick && rxMode->currentIndex() == rxModePick, "the receiver: save if complete, picture format, mode");
  finish();
}
