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

  printf("\n%d passed, %d failed\n", passed, failed);
  fflush(stdout);
  _exit(failed ? 1 : 0);   // the application's threads are not shut down by this probe
}
