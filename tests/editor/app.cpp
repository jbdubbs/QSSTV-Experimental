// Drives the template editor inside the REAL application objects (main window, TX tab, gallery, dispatcher), offscreen:
//   app <scratch-dir> <fixtures-dir>
// Built and run by tests/editor/app.sh against an existing build-cmake tree (it links the application's object files).
#include <QtTest>
#include <QtWidgets>
#include "appglobal.h"
#include "configparams.h"
#include "dispatcher.h"
#include "gallerywidget.h"
#include "imageviewer.h"
#include "mainwindow.h"
#include "sstvparam.h"
#include "templateeditor.h"
#include "txwidget.h"
#include "guiconfig.h"

static int passed = 0, failed = 0;
static void check(bool ok, const QString &what)
{
  printf("  %s %s\n", ok ? "ok  " : "FAIL", qPrintable(what));
  ok ? passed++ : failed++;
}

static QList<TemplateEditor *> editors()
{
  QList<TemplateEditor *> l;
  for (QWidget *w : QApplication::topLevelWidgets())
    if (TemplateEditor *e = qobject_cast<TemplateEditor *>(w)) l << e;
  return l;
}

int main(int argc, char **argv)
{
  const QString scratch = argv[1], fixtures = argv[2];
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
  mainWindowPtr->init();   // like main(): the window is not shown before init()
  mainWindowPtr->hide();
  mainWindowPtr->startRunning(false);   // no sound card receiver
  QTest::qWait(300);

  printf("== template editor in the running application\n");
  check(!templatesPath.isEmpty(), "templates folder: " + templatesPath);
  QDir().mkpath(templatesPath);
  // two templates in the folder: one legacy, one native
  QFile::copy(fixtures + "/synthetic.templ", templatesPath + "/synthetic.templ");
  {
    // a template that sorts in front, so the one under test is not the first entry
    tpl::TemplateDocument first;
    first.canvas = QRectF(0, 0, 320, 256);
    QString err;
    tpl::saveFile(templatesPath + "/aaa-first.qtempl", first, &err);
  }
  galleryWidgetPtr->changedMatrix(imageViewer::TEMPLATETHUMB);
  txWidgetPtr->setupTemplatesComboBox();
  QTest::qWait(200);
  QComboBox *combo = txWidgetPtr->findChild<QComboBox *>("templatesComboBox");
  check(combo && combo->count() == 2, "the TX template list shows both templates (" + QString::number(combo ? combo->count() : -1) + " entries)");

  // the Edit... button on the TX tab
  QToolButton *editButton = nullptr;
  for (QToolButton *b : txWidgetPtr->findChildren<QToolButton *>())
    if (b->text() == "Edit...") editButton = b;
  check(editButton != nullptr, "the TX tab has an Edit... button for templates");
  check(editors().isEmpty(), "no editor open yet");
  if (!editButton) return 1;
  combo->setCurrentIndex(combo->findText("synthetic"));
  editButton->click();
  QTest::qWait(300);
  check(editors().size() == 1, "clicking Edit... opens the template editor");
  if (editors().isEmpty()) return 1;
  TemplateEditor *ed = editors().first();
  check(ed->isVisible(), "and shows it");
  check(ed->document()->isLegacyImport() && ed->document()->doc().items.size() > 5, "with the selected (legacy) template loaded");
  check(ed->document()->doc().canvas.width() > 100, "at its own canvas size");

  // clicking again raises the same window instead of opening another
  editButton->click();
  QTest::qWait(200);
  check(editors().size() == 1, "clicking Edit... again does not open a second editor");

  // edit and save: gallery and TX list follow, the selection stays
  tpl::Element e = EditorCanvas::defaultElement(tpl::ItemType::Text);
  e.text = "%c de %m";
  e.pos = QPointF(10, 40);
  ed->document()->addItems(QVector<tpl::Element>() << e, "add");
  const QString target = templatesPath + "/synthetic.qtempl";
  check(ed->saveTo(target), "saving the template");
  QTest::qWait(500);
  check(QFile::exists(target), "writes the .qtempl");
  check(QFile::exists(templatesPath + "/synthetic.templ"), "and leaves the old .templ where it was");
  const QStringList names = galleryWidgetPtr->getFilenames();
  check(names == QStringList({"aaa-first", "synthetic"}), "the gallery lists the template once, the new file hiding the old: " + names.join(","));
  check(combo->currentText() == "synthetic" && combo->currentIndex() == 1, "the TX combo still has the template selected, not reset to the first (" + combo->currentText() + ")");

  // closing: no lock-out afterwards
  ed->setUnsavedHandler([]() { return TemplateEditor::Discard; });
  ed->close();
  QTest::qWait(300);
  check(editors().isEmpty(), "closing the editor leaves no window behind");
  editButton->click();
  QTest::qWait(300);
  check(editors().size() == 1 && QFileInfo(editors().first()->document()->fileName()).canonicalFilePath() == QFileInfo(target).canonicalFilePath(), "the editor opens again afterwards (the old one stayed locked after a cancelled close)");
  editors().first()->document()->undoStack()->clear();
  editors().first()->close();
  QTest::qWait(200);

  // quitting the program asks about an unsaved template first
  editButton->click();
  QTest::qWait(300);
  check(editors().size() == 1, "(editor open again)");
  if (!editors().isEmpty())
    {
      TemplateEditor *open = editors().first();
      tpl::Element te = EditorCanvas::defaultElement(tpl::ItemType::Rect);
      open->document()->addItems(QVector<tpl::Element>() << te, "add");
      open->setUnsavedHandler([]() { return TemplateEditor::Cancel; });
      check(!dispatcherPtr->closeTemplateEditor() && !editors().isEmpty(), "closing the editor for quit with unsaved changes and answering Cancel keeps it (and the program) open");
      open->setUnsavedHandler([]() { return TemplateEditor::Discard; });
      check(dispatcherPtr->closeTemplateEditor(), "answering Discard closes it");
      QTest::qWait(300);
      check(editors().isEmpty() && dispatcherPtr->closeTemplateEditor(), "and with no editor open there is nothing to ask");
    }

  // New template from the gallery popup path: callEditorEvent with an empty name opens the new-template dialog; the dialog is modal,
  // so only the event path to the dispatcher is checked here with a timer that accepts it
  QTimer::singleShot(400, []() {
    for (QWidget *w : QApplication::topLevelWidgets())
      if (CanvasSizeDialog *d = qobject_cast<CanvasSizeDialog *>(w)) d->accept();
  });
  imageViewer thumb;
  thumb.init(imageViewer::TEMPLATETHUMB);
  thumb.slotEdit();   // no file: a new template
  QTest::qWait(1200);
  check(editors().size() == 1 && editors().first()->document()->fileName().isEmpty(), "Edit on an empty template cell opens a new template");
  editors().first()->close();
  QTest::qWait(200);

  // pictures are handed to an external editor: a fake editor script that paints over the copy
  printf("== picture hand-off to an external editor\n");
  const QString pics = txStockImagesPath;
  QDir().mkpath(pics);
  QImage orig(64, 48, QImage::Format_ARGB32);
  orig.fill(Qt::blue);
  const QString origFile = pics + "/photo.png";
  orig.save(origFile);
  const QByteArray origBytes = [&]() { QFile f(origFile); if (!f.open(QIODevice::ReadOnly)) return QByteArray(); return f.readAll(); }();
  const QString script = scratch + "/fake-editor.sh";
  {
    QFile f(script);
    if (!f.open(QIODevice::WriteOnly)) return 1;
    f.write("#!/bin/sh\nsleep 1\nprintf 'P6 4 4 255 ' > \"$1.tmp\"; head -c 48 /dev/zero | tr '\\0' '\\377' >> \"$1.tmp\"; mv \"$1.tmp\" \"$1\"\n");
    f.close();
    f.setPermissions(f.permissions() | QFileDevice::ExeOwner);
  }
  externalEditorCommand = script + " %f";
  imageViewer *tx = txWidgetPtr->getImageViewerPtr();
  QString fn = origFile;
  tx->openImage(fn, true, false, false, true);
  tx->setFilename(origFile);
  QTest::qWait(500);
  tx->slotEdit();
  QTest::qWait(4500);
  const QString copy = pics + "/photo-edit.png";
  check(QFile::exists(copy) || QFile::exists(pics + "/photo-edit.ppm"), "the copy photo-edit.png exists next to the original");
  const QByteArray after = [&]() { QFile f(origFile); if (!f.open(QIODevice::ReadOnly)) return QByteArray(); return f.readAll(); }();
  check(after == origBytes, "the original is byte for byte unchanged");
  check(QFileInfo(tx->getFilename()).canonicalFilePath() == QFileInfo(copy).canonicalFilePath(), "the TX picture is now the edited copy (" + tx->getFilename() + ")");
  check(tx->getImagePtr()->width() == 4 && tx->getImagePtr()->height() == 4, "and shows what the editor saved (" + QString::number(tx->getImagePtr()->width()) + "x" + QString::number(tx->getImagePtr()->height()) + ")");

  // the same from a TX Stock gallery thumbnail: the copy goes next to the original and the gallery gets it
  {
    QImage stock(32, 24, QImage::Format_ARGB32);
    stock.fill(Qt::green);
    const QString stockFile = pics + "/stockpic.png";
    stock.save(stockFile);
    galleryWidgetPtr->changedMatrix(imageViewer::TXSTOCKTHUMB);
    QTest::qWait(300);
    imageViewer cell;
    cell.init(imageViewer::TXSTOCKTHUMB);
    cell.setFilename(stockFile);
    cell.slotEdit();
    QTest::qWait(4500);
    check(QFile::exists(pics + "/stockpic-edit.png"), "Edit picture on a TX Stock thumbnail makes stockpic-edit.png next to it");
    check(QImage(stockFile).width() == 32, "and leaves the original alone");
    check(QImage(pics + "/stockpic-edit.png").width() == 4, "the edited copy is what the editor saved");
  }

  // a received picture lands in the TX tab's preview, and that picture is what a template's replay slot shows: the TX picture follows
  printf("\n== replay slot follows the RX preview\n");
  {
    QImage base(320, 256, QImage::Format_ARGB32);
    base.fill(QColor(40, 90, 40));
    base.save(pics + "/replay-base.png");
    QImage red(160, 120, QImage::Format_ARGB32), blue(160, 120, QImage::Format_ARGB32);
    red.fill(QColor(255, 0, 0));
    blue.fill(QColor(0, 0, 255));
    red.save(pics + "/rx-red.png");
    blue.save(pics + "/rx-blue.png");
    QString fn = pics + "/replay-base.png";
    tx->openImage(fn, true, false, false, true);
    tx->setFilename(pics + "/replay-base.png");
    QCheckBox *useBox = txWidgetPtr->findChild<QCheckBox *>("templateCheckBox");
    if (useBox) useBox->setChecked(true);
    combo->setCurrentIndex(combo->findText("synthetic"));   // the template with a replay slot
    txWidgetPtr->slotGetParams();
    auto count = [](const QImage &im, QRgb c) {
      int n = 0;
      for (int y = 0; y < im.height(); y++)
        for (int x = 0; x < im.width(); x++)
          if ((im.pixel(x, y) & 0xffffff) == (c & 0xffffff)) n++;
      return n;
    };
    txWidgetPtr->setPreviewWidget(pics + "/rx-red.png");
    QTest::qWait(300);
    const QImage d1 = tx->getDisplayedImage()->copy();
    txWidgetPtr->setPreviewWidget(pics + "/rx-blue.png");
    QTest::qWait(300);
    const QImage d2 = tx->getDisplayedImage()->copy();
    check(!d1.isNull() && d1.size() == d2.size(), "the TX picture is rendered with the template (" + QString::number(d1.width()) + "x" + QString::number(d1.height()) + ")");
    check(count(d1, 0xff0000) - count(d2, 0xff0000) > 500, "the first received picture is in the replay slot (red pixels " + QString::number(count(d1, 0xff0000)) + " -> " + QString::number(count(d2, 0xff0000)) + ")");
    check(count(d2, 0x0000ff) - count(d1, 0x0000ff) > 500, "and the next one replaces it without touching anything on the TX tab (blue pixels " + QString::number(count(d1, 0x0000ff)) + " -> " + QString::number(count(d2, 0x0000ff)) + ")");
    txWidgetPtr->slotGetParams();   // an explicit render with the same preview
    check(*tx->getDisplayedImage() == d2, "which is what a fresh render gives");
    // a template without a replay slot is left alone
    combo->setCurrentIndex(combo->findText("aaa-first"));
    txWidgetPtr->slotGetParams();
    const QImage plain = tx->getDisplayedImage()->copy();
    txWidgetPtr->setPreviewWidget(pics + "/rx-red.png");
    QTest::qWait(300);
    check(*tx->getDisplayedImage() == plain && count(plain, 0xff0000) == 0, "a template without a replay slot is not affected by a new RX picture");
  }

  // a template that keeps its proportions, on two modes of different shape, through the real TX tab
  printf("\n== an anchored template on two SSTV modes\n");
  {
    tpl::TemplateDocument ad;
    ad.canvas = QRectF(0, 0, 320, 256);
    ad.layout = tpl::TemplateDocument::Anchored;
    tpl::Element bar;
    bar.type = tpl::ItemType::Rect;
    bar.pos = QPointF(0, 216);
    bar.rect = QRectF(0, 0, 320, 40);
    bar.fill = QColor(255, 0, 0);
    bar.outline.style = Qt::NoPen;
    bar.anchorH = tpl::AnchorH::Span;
    bar.anchorV = tpl::AnchorV::Bottom;
    tpl::Element dot;
    dot.type = tpl::ItemType::Ellipse;
    dot.pos = QPointF(20, 20);
    dot.rect = QRectF(0, 0, 60, 60);
    dot.fill = QColor(0, 255, 0);
    dot.outline.style = Qt::NoPen;
    ad.items << bar << dot;
    QString err;
    check(tpl::saveFile(templatesPath + "/anchortest.qtempl", ad, &err), "(template saved)");
    galleryWidgetPtr->changedMatrix(imageViewer::TEMPLATETHUMB);
    txWidgetPtr->setupTemplatesComboBox();
    QTest::qWait(200);
    combo->setCurrentIndex(combo->findText("anchortest"));
    QTest::qWait(100);
    auto colourBounds = [](const QImage &im, int r, int g, int b) {
      int x0 = im.width(), y0 = im.height(), x1 = -1, y1 = -1;
      for (int y = 0; y < im.height(); y++)
        for (int x = 0; x < im.width(); x++)
          {
            const QRgb px = im.pixel(x, y);
            if (qRed(px) > r - 40 && qRed(px) < r + 40 && qGreen(px) > g - 40 && qGreen(px) < g + 40 && qBlue(px) > b - 40 && qBlue(px) < b + 40)
              {
                x0 = qMin(x0, x);
                y0 = qMin(y0, y);
                x1 = qMax(x1, x);
                y1 = qMax(y1, y);
              }
          }
      return x1 < 0 ? QRect() : QRect(QPoint(x0, y0), QPoint(x1, y1));
    };
    QImage photo(320, 256, QImage::Format_ARGB32);
    photo.fill(QColor(20, 20, 120));
    photo.save(pics + "/anchor-photo.png");
    QString fn2 = pics + "/anchor-photo.png";
    tx->openImage(fn2, true, false, false, true);
    tx->setFilename(pics + "/anchor-photo.png");
    QCheckBox *use = txWidgetPtr->findChild<QCheckBox *>("templateCheckBox");
    if (use) use->setChecked(true);
    QComboBox *modeCombo = txWidgetPtr->findChild<QComboBox *>("sstvModeComboBox");
    auto chooseMode = [&](const QString &name) {
      for (int i = 0; modeCombo && i < modeCombo->count(); i++)
        if (modeCombo->itemText(i).contains(name))
          {
            modeCombo->setCurrentIndex(i);
            emit modeCombo->activated(i);
            return true;
          }
      return false;
    };
    check(chooseMode("Martin 1"), "(Martin 1 chosen on the TX tab)");   // 320 x 256
    QTest::qWait(200);
    const QImage m1 = tx->getDisplayedImage()->copy();
    check(m1.size() == QSize(320, 256), QString("Martin 1: the picture is 320 x 256 (%1 x %2)").arg(m1.width()).arg(m1.height()));
    const QRect red1 = colourBounds(m1, 255, 0, 0), dot1 = colourBounds(m1, 0, 255, 0);
    check(red1 == QRect(0, 216, 320, 40) && dot1.width() >= 58 && dot1.width() <= 61 && std::abs(dot1.width() - dot1.height()) <= 1, QString("the bar is at the bottom, the circle round (bar %1,%2 %3x%4, circle %5x%6)").arg(red1.x()).arg(red1.y()).arg(red1.width()).arg(red1.height()).arg(dot1.width()).arg(dot1.height()));
    check(chooseMode("Robot 36"), "(Robot 36 chosen)");   // 320 x 240
    QTest::qWait(200);
    const QImage r36 = tx->getDisplayedImage()->copy();
    check(r36.size() == QSize(320, 240), QString("Robot 36: 320 x 240 (%1 x %2)").arg(r36.width()).arg(r36.height()));
    const QRect red2 = colourBounds(r36, 255, 0, 0), dot2 = colourBounds(r36, 0, 255, 0);
    // the template is scaled by 240/256 to fit the height: the bar (anchored to span and to the bottom) still reaches both sides and the bottom
    check(red2.left() == 0 && red2.right() == 319 && red2.bottom() == 239 && red2.height() >= 36 && red2.height() <= 39, QString("the bar is still at the bottom and across the width (%1,%2 %3x%4)").arg(red2.x()).arg(red2.y()).arg(red2.width()).arg(red2.height()));
    check(dot2.width() >= 54 && dot2.width() <= 58 && std::abs(dot2.width() - dot2.height()) <= 1, QString("and the circle is still round, not squashed (%1 x %2)").arg(dot2.width()).arg(dot2.height()));
    chooseMode("Martin 1");
  }

  // the right-click menus of the gallery tabs
  printf("\n== gallery right-click menus\n");
  {
    auto entries = [](imageViewer &v) {
      QStringList l;
      for (QMenu *m : v.findChildren<QMenu *>())
        for (QAction *a : m->actions()) l << (a->isSeparator() ? QStringLiteral("-") : a->text().remove('&'));
      return l;
    };
    imageViewer cell;
    cell.init(imageViewer::TEMPLATETHUMB);
    const QStringList templatesMenu = {"New template...", "Edit template...", "-", "To TX", "View", "Properties", "-", "Load", "Print", "Copy", "-", "Delete"};
    check(entries(cell) == templatesMenu, "Templates: New and Edit first, then To TX / View / Properties, then Load / Print / Copy, Delete alone at the bottom (" + entries(cell).join(" | ") + ")");
    cell.init(imageViewer::TXSTOCKTHUMB);
    const QStringList stockMenu = {"Load", "To TX", "Edit picture...", "Print", "Delete", "View", "Properties", "-", "Copy", "Paste"};
    check(entries(cell) == stockMenu, "TX Stock: unchanged (" + entries(cell).join(" | ") + ")");
    cell.init(imageViewer::TEMPLATETHUMB);
    check(entries(cell) == templatesMenu, "switching a viewer between types leaves no stale separators or entries behind");
    cell.init(imageViewer::RXSSTVTHUMB);
    check(entries(cell) == QStringList({"Upload to FTP", "To TX", "Print", "Delete", "View", "Properties", "-", "Copy"}), "RX pictures: unchanged (" + entries(cell).join(" | ") + ")");
  }

  printf("\n%d passed, %d failed\n", passed, failed);
  fflush(stdout);
  _exit(failed ? 1 : 0);   // the application's threads are not shut down by this probe
}
