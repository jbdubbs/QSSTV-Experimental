#include "templateeditor.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QPainter>
#include "mmsstvimport.h"
#include "templaterender.h"
#include <QCloseEvent>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <cmath>
#include <functional>
#include "editoricons.h"
#include "layerspanel.h"
#include "macropalette.h"
#include "propertiespanel.h"
#include "sizepreviewpanel.h"
#include "templatepresets.h"
#include "templatevars.h"

namespace
{
const int maxRecent = 8;
}

TemplateEditor::TemplateEditor(QWidget *parent) : QMainWindow(parent)
{
  setObjectName(QStringLiteral("TemplateEditor"));
  setWindowTitle(tr("Template Editor"));
  setAttribute(Qt::WA_QuitOnClose, false);
  setDockNestingEnabled(true);

  view = new EditorCanvas(&doc, this);
  view->setMacroVars(tpl::sampleVars());

  // the canvas under a banner that says when the template was imported from the old format
  QWidget *center = new QWidget;
  QVBoxLayout *lay = new QVBoxLayout(center);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(0);
  banner = new QWidget;
  QHBoxLayout *bl = new QHBoxLayout(banner);
  bl->setContentsMargins(8, 4, 8, 4);
  bannerLabel = new QLabel;
  bannerLabel->setWordWrap(true);
  QPushButton *bannerClose = new QPushButton(tr("Dismiss"));
  bl->addWidget(bannerLabel, 1);
  bl->addWidget(bannerClose);
  banner->setAutoFillBackground(true);
  QPalette bp = banner->palette();
  bp.setColor(QPalette::Window, QColor(0xff, 0xf0, 0xc0));
  bp.setColor(QPalette::WindowText, QColor(0x40, 0x30, 0x00));
  banner->setPalette(bp);
  banner->hide();
  lay->addWidget(banner);
  // the old stretch layout squashes text on modes of another shape: say so and offer the other layout
  layoutBanner = new QWidget;
  layoutBanner->setObjectName(QStringLiteral("layoutBanner"));
  QHBoxLayout *lb = new QHBoxLayout(layoutBanner);
  lb->setContentsMargins(8, 4, 8, 4);
  layoutBannerLabel = new QLabel(tr("This template is stretched over the picture, so text and shapes look squashed on a mode of another shape. “Keep proportions” lets text keep its shape and items stick to the edges."));
  layoutBannerLabel->setWordWrap(true);
  QPushButton *keepButton = new QPushButton(tr("Keep proportions"));
  keepButton->setToolTip(tr("Switch to the layout that keeps proportions; every item is anchored to the nearest edge (you can change that in the Properties panel)"));
  QPushButton *layoutDismiss = new QPushButton(tr("Dismiss"));
  lb->addWidget(layoutBannerLabel, 1);
  lb->addWidget(keepButton);
  lb->addWidget(layoutDismiss);
  layoutBanner->setAutoFillBackground(true);
  QPalette lp = layoutBanner->palette();
  lp.setColor(QPalette::Window, QColor(0xe0, 0xee, 0xff));
  lp.setColor(QPalette::WindowText, QColor(0x10, 0x20, 0x40));
  layoutBanner->setPalette(lp);
  layoutBanner->hide();
  lay->addWidget(layoutBanner);
  connect(keepButton, &QPushButton::clicked, this, [this]() { applyCanvasSettings(doc.doc().canvas.size().toSize(), false, tpl::TemplateDocument::Anchored); });
  connect(layoutDismiss, &QPushButton::clicked, this, [this]() {
    layoutBannerDismissed = true;
    layoutBanner->hide();
  });
  lay->addWidget(view, 1);
  setCentralWidget(center);
  connect(bannerClose, &QPushButton::clicked, this, [this]() {
    bannerDismissed = true;   // stays away until another template is loaded
    banner->hide();
  });

  properties = new PropertiesPanel(&doc, view, this);
  layers = new LayersPanel(&doc, view, this);
  macros = new MacroPalette(this);
  sizePreview = new SizePreviewPanel(&doc, this);
  connect(sizePreview, &SizePreviewPanel::keepProportionsRequested, this, [this]() {
    applyCanvasSettings(doc.doc().canvas.size().toSize(), false, tpl::TemplateDocument::Anchored);
  });

  createActions();
  createMenus();
  createToolBars();
  createDocks();

  cursorLabel = new QLabel;
  cursorLabel->setMinimumWidth(90);
  selectionLabel = new QLabel;
  selectionLabel->setMinimumWidth(80);
  sizeLabel = new QLabel;
  zoomLabel = new QLabel;
  zoomLabel->setMinimumWidth(50);
  for (QLabel *l : {selectionLabel, cursorLabel, sizeLabel, zoomLabel}) statusBar()->addPermanentWidget(l);
  statusBar()->showMessage(tr("Pick a tool on the left, or click an item to edit it."));

  connect(view, &EditorCanvas::selectionChanged, this, &TemplateEditor::updateActions);
  connect(view, &EditorCanvas::selectionChanged, this, &TemplateEditor::updateStatus);
  connect(view, &EditorCanvas::zoomChanged, this, &TemplateEditor::updateStatus);
  connect(view, &EditorCanvas::toolChanged, this, &TemplateEditor::toolChanged);
  connect(view, &EditorCanvas::cursorMoved, this, &TemplateEditor::cursorMoved);
  connect(view, &EditorCanvas::contextMenuRequested, this, &TemplateEditor::showContextMenu);
  connect(view, &EditorCanvas::statusMessage, this, [this](const QString &m) { statusBar()->showMessage(m, 6000); });
  connect(&doc, &EditorDocument::canvasChanged, this, &TemplateEditor::updateStatus);
  connect(&doc, &EditorDocument::reset, this, &TemplateEditor::updateStatus);
  connect(&doc, &EditorDocument::fileChanged, this, &TemplateEditor::updateTitle);
  connect(&doc, &EditorDocument::reset, this, [this]() {
    bannerDismissed = false;
    layoutBannerDismissed = false;
    updateTitle();
  });
  connect(&doc, &EditorDocument::canvasChanged, this, &TemplateEditor::updateLayoutBanner);
  connect(doc.undoStack(), &QUndoStack::cleanChanged, this, &TemplateEditor::updateTitle);
  connect(doc.undoStack(), &QUndoStack::indexChanged, this, &TemplateEditor::updateActions);
  connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &TemplateEditor::updateActions);
  connect(macros, &MacroPalette::macroActivated, this, &TemplateEditor::macroChosen);

  resize(1100, 720);
  readSettings();
  tpl::TemplateDocument blank;
  blank.canvas = QRectF(QPointF(0, 0), QSizeF(defaultSize));
  doc.load(blank, QString());
  refreshIcons();
  updateActions();
  updateStatus();
  updateTitle();
}

TemplateEditor::~TemplateEditor()
{
  writeSettings();
}

// --- actions ---------------------------------------------------------------------------------------------------
QAction *TemplateEditor::makeAction(const QString &text, const QString &icon, const QKeySequence &shortcut, const QString &tip)
{
  QAction *a = new QAction(text, this);
  if (!icon.isEmpty()) a->setProperty("iconName", icon);
  if (!shortcut.isEmpty()) a->setShortcut(shortcut);
  a->setStatusTip(tip);
  QString t = text;
  t.remove(QLatin1Char('&'));
  a->setToolTip(tip.isEmpty() ? t : (shortcut.isEmpty() ? tip : tip + QStringLiteral(" (") + shortcut.toString(QKeySequence::NativeText) + QStringLiteral(")")));
  return a;
}

QAction *TemplateEditor::makeAction(const QString &text, const QString &icon, QKeySequence::StandardKey key, const QString &tip)
{
  QAction *a = makeAction(text, icon, QKeySequence(key), tip);
  a->setShortcuts(QKeySequence::keyBindings(key));
  return a;
}

void TemplateEditor::createActions()
{
  newAct = makeAction(tr("&New…"), QStringLiteral("new"), QKeySequence::New, tr("Start a new template"));
  openAct = makeAction(tr("&Open…"), QStringLiteral("open"), QKeySequence::Open, tr("Open a template (.qtempl, or an old .templ file)"));
  importMmsstvAct = makeAction(tr("Import MMSSTV template…"), QString(), QKeySequence(), tr("Convert an MMSSTV .mtm template into editable native objects"));
  importMmsstvAct->setObjectName(QStringLiteral("importMmsstvAct"));
  importMmsstvBatchAct = makeAction(tr("Import MMSSTV templates (batch)…"), QString(), QKeySequence(), tr("Convert several .mtm files into the templates folder"));
  importMmsstvBatchAct->setObjectName(QStringLiteral("importMmsstvBatchAct"));
  connect(importMmsstvAct, &QAction::triggered, this, [this]() {
    const QString file = QFileDialog::getOpenFileName(this, tr("Import MMSSTV template"), templatesDir, tr("MMSSTV templates (*.mtm *.MTM);;All files (*)"));
    if (!file.isEmpty()) importMmsstvFile(file);
  });
  connect(importMmsstvBatchAct, &QAction::triggered, this, &TemplateEditor::importMmsstvBatch);
  saveAct = makeAction(tr("&Save"), QStringLiteral("save"), QKeySequence::Save, tr("Save the template"));
  saveAsAct = makeAction(tr("Save &As…"), QStringLiteral("saveas"), QKeySequence::SaveAs, tr("Save the template under a new name"));
  revertAct = makeAction(tr("&Revert"), QStringLiteral("revert"), QKeySequence(), tr("Throw away the changes and load the saved file again"));
  closeAct = makeAction(tr("&Close"), QString(), QKeySequence(Qt::CTRL | Qt::Key_W), tr("Close the editor"));
  connect(newAct, &QAction::triggered, this, &TemplateEditor::newTemplateDialog);
  connect(openAct, &QAction::triggered, this, [this]() {
    const QString start = doc.fileName().isEmpty() ? templatesDir : QFileInfo(doc.fileName()).absolutePath();
    const QString fn = QFileDialog::getOpenFileName(this, tr("Open template"), start, tr("Templates (*.qtempl *.templ);;All files (*)"));
    if (!fn.isEmpty()) openFile(fn);
  });
  connect(saveAct, &QAction::triggered, this, &TemplateEditor::save);
  connect(saveAsAct, &QAction::triggered, this, &TemplateEditor::saveAs);
  connect(revertAct, &QAction::triggered, this, &TemplateEditor::revert);
  connect(closeAct, &QAction::triggered, this, &QWidget::close);

  undoAct = doc.undoStack()->createUndoAction(this, tr("&Undo"));
  redoAct = doc.undoStack()->createRedoAction(this, tr("&Redo"));
  undoAct->setProperty("iconName", QStringLiteral("undo"));
  redoAct->setProperty("iconName", QStringLiteral("redo"));
  undoAct->setShortcuts(QKeySequence::Undo);
  redoAct->setShortcuts(QList<QKeySequence>() << QKeySequence(QKeySequence::Redo) << QKeySequence(Qt::CTRL | Qt::Key_Y));
  undoAct->setToolTip(tr("Undo the last change (Ctrl+Z)"));
  redoAct->setToolTip(tr("Redo the change that was undone (Ctrl+Shift+Z)"));

  cutAct = makeAction(tr("Cu&t"), QStringLiteral("cut"), QKeySequence::Cut, tr("Cut the selected items"));
  copyAct = makeAction(tr("&Copy"), QStringLiteral("copy"), QKeySequence::Copy, tr("Copy the selected items"));
  pasteAct = makeAction(tr("&Paste"), QStringLiteral("paste"), QKeySequence::Paste, tr("Paste items, a picture or text from the clipboard"));
  duplicateAct = makeAction(tr("D&uplicate"), QStringLiteral("duplicate"), QKeySequence(Qt::CTRL | Qt::Key_D), tr("Make a copy of the selected items next to them"));
  deleteAct = makeAction(tr("&Delete"), QStringLiteral("delete"), QKeySequence::Delete, tr("Delete the selected items"));
  selectAllAct = makeAction(tr("Select &All"), QStringLiteral("selectall"), QKeySequence::SelectAll, tr("Select every item"));
  clearAllAct = makeAction(tr("Delete All &Items…"), QString(), QKeySequence(), tr("Delete every item (can be undone)"));
  connect(cutAct, &QAction::triggered, view, &EditorCanvas::cutSelection);
  connect(copyAct, &QAction::triggered, view, &EditorCanvas::copySelection);
  connect(pasteAct, &QAction::triggered, view, &EditorCanvas::paste);
  connect(duplicateAct, &QAction::triggered, view, &EditorCanvas::duplicateSelection);
  connect(deleteAct, &QAction::triggered, view, &EditorCanvas::deleteSelection);
  connect(selectAllAct, &QAction::triggered, view, &EditorCanvas::selectAll);
  connect(clearAllAct, &QAction::triggered, this, [this]() {
    if (doc.doc().items.isEmpty()) return;
    if (QMessageBox::question(this, tr("Delete all items"), tr("Delete all %1 items of this template?\n\nYou can undo this with Edit > Undo.").arg(doc.doc().items.size())) == QMessageBox::Yes)
      doc.clearItems(tr("Delete all items"));
  });

  toolGroup = new QActionGroup(this);
  auto tool = [this](const QString &text, const QString &icon, const QKeySequence &key, const QString &tip, EditorCanvas::Tool t) {
    QAction *a = makeAction(text, icon, key, tip);
    a->setCheckable(true);
    toolGroup->addAction(a);
    connect(a, &QAction::triggered, this, [this, t]() { view->setTool(t); });
    return a;
  };
  selectTool = tool(tr("&Select"), QStringLiteral("select"), QKeySequence(Qt::Key_V), tr("Select and move items. Drag the handles to resize or rotate"), EditorCanvas::ToolSelect);
  textTool = tool(tr("&Text"), QStringLiteral("text"), QKeySequence(Qt::Key_T), tr("Add text: click where it should go, or drag a box for text that wraps and shrinks to fit"), EditorCanvas::ToolText);
  rectTool = tool(tr("&Rectangle"), QStringLiteral("rect"), QKeySequence(Qt::Key_R), tr("Add a rectangle: drag on the canvas"), EditorCanvas::ToolRect);
  ellipseTool = tool(tr("&Ellipse"), QStringLiteral("ellipse"), QKeySequence(Qt::Key_E), tr("Add an ellipse: drag on the canvas"), EditorCanvas::ToolEllipse);
  lineTool = tool(tr("&Line"), QStringLiteral("line"), QKeySequence(Qt::Key_L), tr("Add a line: drag on the canvas"), EditorCanvas::ToolLine);
  replayTool = tool(tr("Re&play window"), QStringLiteral("replay"), QKeySequence(Qt::Key_P),
                    tr("Add the replay window: drag on the canvas. When the picture is sent, the picture received last (the preview on the Transmit tab) is shown there; here it is a hatched square"), EditorCanvas::ToolReplay);
  selectTool->setChecked(true);
  imageAct = makeAction(tr("&Picture…"), QStringLiteral("image"), QKeySequence(Qt::CTRL | Qt::Key_I), tr("Add a picture (a logo, for example) from a file. You can also drop picture files on the canvas"));
  connect(imageAct, &QAction::triggered, this, &TemplateEditor::insertImage);

  const struct { const char *text; const char *icon; EditorCanvas::AlignMode mode; const char *tip; } aligns[] = {
    {QT_TR_NOOP("Align &Left"), "align-left", EditorCanvas::AlignLeft, QT_TR_NOOP("Align the left edges (of the selection, or of the canvas for a single item)")},
    {QT_TR_NOOP("Align Horizontal &Center"), "align-hcenter", EditorCanvas::AlignHCenter, QT_TR_NOOP("Center horizontally")},
    {QT_TR_NOOP("Align &Right"), "align-right", EditorCanvas::AlignRight, QT_TR_NOOP("Align the right edges")},
    {QT_TR_NOOP("Align &Top"), "align-top", EditorCanvas::AlignTop, QT_TR_NOOP("Align the top edges")},
    {QT_TR_NOOP("Align &Vertical Center"), "align-vcenter", EditorCanvas::AlignVCenter, QT_TR_NOOP("Center vertically")},
    {QT_TR_NOOP("Align &Bottom"), "align-bottom", EditorCanvas::AlignBottom, QT_TR_NOOP("Align the bottom edges")}};
  for (int i = 0; i < 6; i++)
    {
      alignActs[i] = makeAction(tr(aligns[i].text), QString::fromLatin1(aligns[i].icon), QKeySequence(), tr(aligns[i].tip));
      const EditorCanvas::AlignMode m = aligns[i].mode;
      connect(alignActs[i], &QAction::triggered, this, [this, m]() { view->align(m); });
    }
  distributeH = makeAction(tr("Distribute &Horizontally"), QStringLiteral("distribute-h"), QKeySequence(), tr("Space three or more items evenly from left to right"));
  distributeV = makeAction(tr("Distribute Verti&cally"), QStringLiteral("distribute-v"), QKeySequence(), tr("Space three or more items evenly from top to bottom"));
  connect(distributeH, &QAction::triggered, this, [this]() { view->distribute(Qt::Horizontal); });
  connect(distributeV, &QAction::triggered, this, [this]() { view->distribute(Qt::Vertical); });

  frontAct = makeAction(tr("Bring to &Front"), QStringLiteral("front"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketRight), tr("Move the selected items in front of all others"));
  forwardAct = makeAction(tr("Bring F&orward"), QStringLiteral("forward"), QKeySequence(Qt::CTRL | Qt::Key_BracketRight), tr("Move the selected items one step forward"));
  backwardAct = makeAction(tr("Send &Backward"), QStringLiteral("backward"), QKeySequence(Qt::CTRL | Qt::Key_BracketLeft), tr("Move the selected items one step back"));
  backAct = makeAction(tr("Send to Ba&ck"), QStringLiteral("back"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketLeft), tr("Move the selected items behind all others"));
  lockAct = makeAction(tr("Loc&k"), QStringLiteral("lock"), QKeySequence(Qt::CTRL | Qt::Key_L), tr("Lock or unlock the selected items. A locked item cannot be moved or clicked on the canvas; right-click it to select it again"));
  connect(frontAct, &QAction::triggered, view, &EditorCanvas::bringToFront);
  connect(forwardAct, &QAction::triggered, view, &EditorCanvas::bringForward);
  connect(backwardAct, &QAction::triggered, view, &EditorCanvas::sendBackward);
  connect(backAct, &QAction::triggered, view, &EditorCanvas::sendToBack);
  connect(lockAct, &QAction::triggered, view, &EditorCanvas::toggleLock);

  zoomInAct = makeAction(tr("Zoom &In"), QStringLiteral("zoom-in"), QKeySequence::ZoomIn, tr("Zoom in (Ctrl + mouse wheel)"));
  zoomOutAct = makeAction(tr("Zoom &Out"), QStringLiteral("zoom-out"), QKeySequence::ZoomOut, tr("Zoom out (Ctrl + mouse wheel)"));
  zoomFitAct = makeAction(tr("&Fit in Window"), QStringLiteral("zoom-fit"), QKeySequence(Qt::CTRL | Qt::Key_0), tr("Fit the whole canvas in the window"));
  zoom100Act = makeAction(tr("&Actual Size"), QStringLiteral("zoom-100"), QKeySequence(Qt::CTRL | Qt::Key_1), tr("Show the canvas at 100 %"));
  connect(zoomInAct, &QAction::triggered, view, &EditorCanvas::zoomIn);
  connect(zoomOutAct, &QAction::triggered, view, &EditorCanvas::zoomOut);
  connect(zoomFitAct, &QAction::triggered, view, &EditorCanvas::zoomFit);
  connect(zoom100Act, &QAction::triggered, view, &EditorCanvas::zoomActual);

  auto toggle = [this](const QString &text, const QString &icon, const QString &tip, bool on) {
    QAction *a = makeAction(text, icon, QKeySequence(), tip);
    a->setCheckable(true);
    a->setChecked(on);
    return a;
  };
  gridAct = toggle(tr("Show &Grid"), QStringLiteral("grid"), tr("Show a grid over the canvas"), false);
  snapGridAct = toggle(tr("Snap to Gri&d"), QStringLiteral("grid"), tr("Snap moving items and new shapes to the grid (hold Alt to switch snapping off while dragging)"), false);
  snapObjectsAct = toggle(tr("Snap to &Objects"), QStringLiteral("snap"), tr("Snap moving items to the edges and centers of the canvas and of other items"), true);
  previewMacrosAct = toggle(tr("Preview &Macros"), QStringLiteral("macro"), tr("Show sample values instead of %c, %m and the other macros"), false);
  previewPictureAct = toggle(tr("Show Picture &Under Template"), QStringLiteral("eye"), tr("Show the current Transmit picture behind the template (only a guide, never saved)"), false);
  previewPictureAct->setEnabled(false);
  guideCenterAct = toggle(tr("&Center Lines"), QStringLiteral("guides"), tr("Show lines through the middle of the canvas"), false);
  guide5Act = toggle(tr("&5 % Margin"), QStringLiteral("guides"), tr("Show a frame 5 % in from each edge, a margin to keep text inside"), false);
  guide10Act = toggle(tr("&10 % Margin"), QStringLiteral("guides"), tr("Show a frame 10 % in from each edge"), false);
  for (QAction *a : {guideCenterAct, guide5Act, guide10Act}) connect(a, &QAction::toggled, this, &TemplateEditor::applyGuides);
  connect(gridAct, &QAction::toggled, view, &EditorCanvas::setShowGrid);
  connect(snapGridAct, &QAction::toggled, view, &EditorCanvas::setSnapToGrid);
  connect(snapObjectsAct, &QAction::toggled, view, &EditorCanvas::setSnapToObjects);
  connect(previewMacrosAct, &QAction::toggled, view, &EditorCanvas::setExpandMacros);
  connect(previewPictureAct, &QAction::toggled, view, &EditorCanvas::setShowPreview);
  view->setSnapToObjects(true);
  canvasSizeAct = makeAction(tr("Canvas &Size…"), QStringLiteral("canvas-size"), QKeySequence(), tr("Change the size of the canvas"));
  connect(canvasSizeAct, &QAction::triggered, this, &TemplateEditor::changeCanvasSize);
  helpAct = makeAction(tr("Template Editor &Help"), QStringLiteral("help"), QKeySequence::HelpContents, tr("Open the Users Guide page about the template editor"));
  connect(helpAct, &QAction::triggered, this, &TemplateEditor::helpRequested);

  macros->setPreviewAction(previewMacrosAct);
}

void TemplateEditor::createMenus()
{
  QMenu *file = menuBar()->addMenu(tr("&File"));
  file->addAction(newAct);
  file->addAction(openAct);
  file->addAction(importMmsstvAct);
  file->addAction(importMmsstvBatchAct);
  recentMenu = file->addMenu(tr("Open &Recent"));
  for (int i = 0; i < maxRecent; i++)
    {
      QAction *a = new QAction(this);
      a->setVisible(false);
      connect(a, &QAction::triggered, this, [this, a]() { openFile(a->data().toString()); });
      recentActs << a;
      recentMenu->addAction(a);
    }
  file->addSeparator();
  file->addAction(saveAct);
  file->addAction(saveAsAct);
  file->addAction(revertAct);
  file->addSeparator();
  file->addAction(closeAct);

  QMenu *edit = menuBar()->addMenu(tr("&Edit"));
  edit->addAction(undoAct);
  edit->addAction(redoAct);
  edit->addSeparator();
  edit->addAction(cutAct);
  edit->addAction(copyAct);
  edit->addAction(pasteAct);
  edit->addAction(duplicateAct);
  edit->addAction(deleteAct);
  edit->addSeparator();
  edit->addAction(selectAllAct);
  edit->addAction(clearAllAct);

  QMenu *insert = menuBar()->addMenu(tr("&Insert"));
  insert->addAction(textTool);
  insert->addAction(rectTool);
  insert->addAction(ellipseTool);
  insert->addAction(lineTool);
  insert->addAction(imageAct);
  insert->addAction(replayTool);
  insert->addSeparator();
  QMenu *barsMenu = insert->addMenu(tr("Color &Bar"));
  barsMenu->setIcon(QIcon());
  const QStringList barStyles = templatePresets::barNames();
  for (bool bottom : {true, false})
    {
      QMenu *m = barsMenu->addMenu(bottom ? tr("At the &Bottom") : tr("At the &Top"));
      for (int i = 0; i < barStyles.size(); i++)
        connect(m->addAction(barStyles.at(i)), &QAction::triggered, this, [this, i, bottom]() { insertBar(i, bottom); });
    }

  QMenu *arrange = menuBar()->addMenu(tr("&Arrange"));
  arrange->addAction(frontAct);
  arrange->addAction(forwardAct);
  arrange->addAction(backwardAct);
  arrange->addAction(backAct);
  arrange->addSeparator();
  QMenu *align = arrange->addMenu(tr("&Align"));
  for (QAction *a : alignActs) align->addAction(a);
  align->addSeparator();
  align->addAction(distributeH);
  align->addAction(distributeV);
  arrange->addAction(lockAct);
  arrange->addSeparator();
  arrange->addAction(canvasSizeAct);

  QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
  viewMenu->addAction(zoomInAct);
  viewMenu->addAction(zoomOutAct);
  viewMenu->addAction(zoomFitAct);
  viewMenu->addAction(zoom100Act);
  viewMenu->addSeparator();
  viewMenu->addAction(gridAct);
  viewMenu->addAction(snapGridAct);
  viewMenu->addAction(snapObjectsAct);
  QMenu *guidesMenu = viewMenu->addMenu(tr("G&uides"));
  guidesMenu->addAction(guideCenterAct);
  guidesMenu->addAction(guide5Act);
  guidesMenu->addAction(guide10Act);
  viewMenu->addSeparator();
  viewMenu->addAction(previewMacrosAct);
  viewMenu->addAction(previewPictureAct);
  viewMenu->addSeparator();
  viewMenu->setObjectName(QStringLiteral("viewMenu"));

  QMenu *help = menuBar()->addMenu(tr("&Help"));
  help->addAction(helpAct);

  contextMenu = new QMenu(this);
  contextMenu->addAction(cutAct);
  contextMenu->addAction(copyAct);
  contextMenu->addAction(pasteAct);
  contextMenu->addAction(duplicateAct);
  contextMenu->addAction(deleteAct);
  contextMenu->addSeparator();
  QMenu *order = contextMenu->addMenu(tr("Arrange"));
  order->addAction(frontAct);
  order->addAction(forwardAct);
  order->addAction(backwardAct);
  order->addAction(backAct);
  contextMenu->addAction(lockAct);
}

void TemplateEditor::createToolBars()
{
  QToolBar *main = addToolBar(tr("File and edit"));
  main->setObjectName(QStringLiteral("fileToolBar"));
  main->addAction(newAct);
  main->addAction(openAct);
  main->addAction(saveAct);
  main->addSeparator();
  main->addAction(undoAct);
  main->addAction(redoAct);
  main->addSeparator();
  main->addAction(cutAct);
  main->addAction(copyAct);
  main->addAction(pasteAct);
  main->addAction(deleteAct);

  QToolBar *tools = new QToolBar(tr("Tools"), this);
  tools->setObjectName(QStringLiteral("toolsToolBar"));
  tools->setOrientation(Qt::Vertical);
  tools->setIconSize(QSize(24, 24));
  addToolBar(Qt::LeftToolBarArea, tools);
  tools->addAction(selectTool);
  tools->addAction(textTool);
  tools->addAction(rectTool);
  tools->addAction(ellipseTool);
  tools->addAction(lineTool);
  tools->addAction(imageAct);
  tools->addAction(replayTool);

  QToolBar *arrange = addToolBar(tr("Arrange"));
  arrange->setObjectName(QStringLiteral("arrangeToolBar"));
  for (QAction *a : alignActs) arrange->addAction(a);
  arrange->addAction(distributeH);
  arrange->addAction(distributeV);
  arrange->addSeparator();
  arrange->addAction(frontAct);
  arrange->addAction(forwardAct);
  arrange->addAction(backwardAct);
  arrange->addAction(backAct);
  arrange->addAction(lockAct);

  QToolBar *viewBar = addToolBar(tr("View"));
  viewBar->setObjectName(QStringLiteral("viewToolBar"));
  viewBar->addAction(zoomInAct);
  viewBar->addAction(zoomOutAct);
  viewBar->addAction(zoomFitAct);
  viewBar->addAction(zoom100Act);
  viewBar->addSeparator();
  viewBar->addAction(gridAct);
  viewBar->addAction(snapObjectsAct);
  viewBar->addAction(previewMacrosAct);
  viewBar->addAction(previewPictureAct);
  viewBar->addAction(canvasSizeAct);
}

void TemplateEditor::createDocks()
{
  QDockWidget *propDock = new QDockWidget(tr("Properties"), this);
  propDock->setObjectName(QStringLiteral("propertiesDock"));
  propDock->setWidget(properties);
  addDockWidget(Qt::RightDockWidgetArea, propDock);
  QDockWidget *layerDock = new QDockWidget(tr("Layers"), this);
  layerDock->setObjectName(QStringLiteral("layersDock"));
  layerDock->setWidget(layers);
  addDockWidget(Qt::RightDockWidgetArea, layerDock);
  QDockWidget *macroDock = new QDockWidget(tr("Macros"), this);
  macroDock->setObjectName(QStringLiteral("macrosDock"));
  macroDock->setWidget(macros);
  addDockWidget(Qt::RightDockWidgetArea, macroDock);
  tabifyDockWidget(layerDock, macroDock);
  QDockWidget *sizeDock = new QDockWidget(tr("Size preview"), this);
  sizeDock->setObjectName(QStringLiteral("sizePreviewDock"));
  sizeDock->setWidget(sizePreview);
  addDockWidget(Qt::RightDockWidgetArea, sizeDock);
  tabifyDockWidget(macroDock, sizeDock);
  layerDock->raise();
  if (QMenu *vm = menuBar()->findChild<QMenu *>(QStringLiteral("viewMenu")))
    for (QDockWidget *d : {propDock, layerDock, macroDock, sizeDock}) vm->addAction(d->toggleViewAction());
  resizeDocks({propDock, layerDock}, {500, 200}, Qt::Vertical);
  resizeDocks({propDock}, {320}, Qt::Horizontal);
}

void TemplateEditor::refreshIcons()
{
  for (QAction *a : findChildren<QAction *>())
    if (a->property("iconName").isValid()) a->setIcon(editorIcons::icon(a->property("iconName").toString(), palette()));
  closeAct->setIcon(QIcon());
}

void TemplateEditor::changeEvent(QEvent *e)
{
  QMainWindow::changeEvent(e);
  if (e->type() == QEvent::PaletteChange) refreshIcons();
}

// --- state shown to the user -------------------------------------------------------------------------------------
void TemplateEditor::updateTitle()
{
  const QString name = doc.fileName().isEmpty() ? tr("Untitled") : QFileInfo(doc.fileName()).fileName();
  setWindowTitle(QStringLiteral("%1[*] — %2").arg(name, tr("Template Editor")));
  setWindowModified(doc.isModified());
  const bool legacy = doc.isLegacyImport();
  if (legacy)
    bannerLabel->setText(tr("This template was imported from the old .templ format. Saving writes a new .qtempl file and leaves %1 untouched; the template list then shows the new file.").arg(name));
  banner->setVisible(legacy && !doc.fileName().isEmpty() && !bannerDismissed);
  updateLayoutBanner();
  updateActions();
}

void TemplateEditor::updateLayoutBanner()
{
  layoutBanner->setVisible(doc.doc().layout == tpl::TemplateDocument::Stretch && !doc.fileName().isEmpty() && !layoutBannerDismissed);
}

void TemplateEditor::updateActions()
{
  const int n = view->selection().size();
  const bool has = n > 0;
  for (QAction *a : {cutAct, copyAct, deleteAct, duplicateAct, frontAct, forwardAct, backwardAct, backAct, lockAct}) a->setEnabled(has);
  // Lock unlocks only when nothing selected is unlocked (see EditorCanvas::toggleLock)
  bool allLocked = has;
  for (const tpl::Element &e : view->selectedElements()) allLocked &= e.locked;
  lockAct->setText(allLocked ? tr("Un&lock") : tr("Loc&k"));
  for (QAction *a : alignActs) a->setEnabled(has);
  distributeH->setEnabled(n >= 3);
  distributeV->setEnabled(n >= 3);
  pasteAct->setEnabled(view->canPaste());
  clearAllAct->setEnabled(!doc.doc().items.isEmpty());
  revertAct->setEnabled(!doc.fileName().isEmpty() && doc.isModified());
  saveAct->setEnabled(doc.isModified() || doc.fileName().isEmpty() || doc.isLegacyImport());
}

void TemplateEditor::updateStatus()
{
  zoomLabel->setText(QStringLiteral("%1 %").arg(qRound(view->zoom() * 100)));
  const QRectF c = doc.doc().canvas;
  sizeLabel->setText(QStringLiteral("%1 × %2").arg(qRound(c.width())).arg(qRound(c.height())));
  const int n = view->selection().size();
  selectionLabel->setText(n == 0 ? QString() : (n == 1 ? view->describe(view->selectedElements().first()) : tr("%1 items").arg(n)));
}

void TemplateEditor::cursorMoved(const QPointF &pos, bool inside)
{
  cursorLabel->setText(inside ? QStringLiteral("%1, %2").arg(qRound(pos.x())).arg(qRound(pos.y())) : QString());
}

void TemplateEditor::toolChanged(int tool)
{
  QAction *a[] = {selectTool, textTool, rectTool, ellipseTool, lineTool, replayTool};
  if (tool >= 0 && tool < 6)
    {
      a[tool]->setChecked(true);
      statusBar()->showMessage(a[tool]->statusTip());
    }
}

void TemplateEditor::showContextMenu(const QPoint &globalPos)
{
  updateActions();
  contextMenu->exec(globalPos);
}

// --- inputs from the application ------------------------------------------------------------------------------
void TemplateEditor::setSizePresets(const QVector<SizePreset> &presets)
{
  sizePresets = presets;
  sizePreview->setSizes(sizePresets, defaultSize);
}

void TemplateEditor::setDefaultCanvasSize(const QSize &size)
{
  defaultSize = size;
  sizePreview->setSizes(sizePresets, defaultSize);
}

void TemplateEditor::setSampleValues(const QHash<QChar, QString> &values)
{
  view->setMacroVars(values);
  macros->setValues(values);
  sizePreview->setSampleValues(values);
}

void TemplateEditor::insertBar(int style, bool atBottom)
{
  QUndoStack *stack = doc.undoStack();
  stack->beginMacro(tr("Add color bar"));
  const QStringList added = doc.addItems(QVector<tpl::Element>() << templatePresets::makeBar(style, atBottom, doc.doc().canvas), tr("Add color bar"));
  if (!added.isEmpty())
    {
      // a bar is a background: text and pictures stay in front of it
      QStringList order = added;
      for (const QString &id : doc.ids())
        if (!added.contains(id)) order << id;
      doc.reorder(order, tr("Send to back"));
    }
  stack->endMacro();
}

void TemplateEditor::applyGuides()
{
  view->setGuides(guideCenterAct->isChecked(), guide5Act->isChecked(), guide10Act->isChecked());
}

void TemplateEditor::setPreviewPicture(const QImage &img)
{
  previewPicture = img;
  view->setPreviewImage(img);
  sizePreview->setPicture(img);
  previewPictureAct->setEnabled(!img.isNull());
  if (img.isNull()) previewPictureAct->setChecked(false);
}

// --- files ---------------------------------------------------------------------------------------------------------
TemplateEditor::Choice TemplateEditor::askUnsaved()
{
  if (unsavedHandler) return unsavedHandler();
  const QString name = doc.fileName().isEmpty() ? tr("Untitled") : QFileInfo(doc.fileName()).fileName();
  QMessageBox box(QMessageBox::Question, tr("Template Editor"), tr("The template \"%1\" has unsaved changes.").arg(name), QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
  box.setInformativeText(tr("Do you want to save them?"));
  box.setDefaultButton(QMessageBox::Save);
  switch (box.exec())
    {
    case QMessageBox::Save: return Save;
    case QMessageBox::Discard: return Discard;
    default: return Cancel;
    }
}

void TemplateEditor::problem(const QString &title, const QString &text)
{
  if (messageHandler) messageHandler(title, text);
  else QMessageBox::warning(this, title, text);
}

bool TemplateEditor::maybeSave()
{
  view->commitTextEdit();   // a text still being typed counts
  if (!doc.isModified()) return true;
  switch (askUnsaved())
    {
    case Save: return save();
    case Discard: return true;
    default: return false;
    }
}

bool TemplateEditor::openFile(const QString &fileName)
{
  if (!maybeSave()) return false;
  const tpl::LoadResult r = tpl::loadFile(fileName);
  if (!r.ok)
    {
      problem(tr("Open template"), tr("%1 cannot be opened:\n%2").arg(QDir::toNativeSeparators(fileName), r.error));
      return false;
    }
  doc.load(r.doc, fileName);
  if (!r.warnings.isEmpty()) statusBar()->showMessage(r.warnings.first(), 8000);
  addRecent(fileName);
  view->zoomFit();
  return true;
}

bool TemplateEditor::importMmsstvFile(const QString &fileName)
{
  const tpl::LoadResult r = Mmsstv::loadTemplate(fileName);
  if (!r.ok) { problem(tr("Import MMSSTV template"), r.error); return false; }
  QDialog preview(this);
  preview.setObjectName(QStringLiteral("mmsstvImportPreview"));
  preview.setWindowTitle(tr("Preview MMSSTV import — %1").arg(QFileInfo(fileName).fileName()));
  QVBoxLayout *layout = new QVBoxLayout(&preview);
  QLabel *image = new QLabel;
  const QImage rendered = tpl::renderThumbnail(r.doc);
  image->setPixmap(QPixmap::fromImage(rendered.scaled(640, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
  QPixmap checker(16, 16); checker.fill(QColor(244,244,244));
  { QPainter p(&checker); p.fillRect(0,0,8,8,QColor(220,220,220)); p.fillRect(8,8,8,8,QColor(220,220,220)); }
  QPalette palette = image->palette(); palette.setBrush(QPalette::Window, QBrush(checker));
  image->setPalette(palette); image->setAutoFillBackground(true);
  layout->addWidget(image, 0, Qt::AlignCenter);
  layout->addWidget(new QLabel(tr("%1 × %2 pixels; %3 objects. Save creates a new .qtempl file.")
    .arg(r.doc.canvas.width()).arg(r.doc.canvas.height()).arg(r.doc.items.size())));
  QPlainTextEdit *warnings = new QPlainTextEdit;
  warnings->setReadOnly(true);
  warnings->setPlainText(r.warnings.join(QLatin1Char('\n')));
  warnings->setVisible(!r.warnings.isEmpty());
  layout->addWidget(warnings);
  QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  connect(buttons, &QDialogButtonBox::accepted, &preview, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &preview, &QDialog::reject);
  layout->addWidget(buttons);
  if (preview.exec() != QDialog::Accepted || !maybeSave()) return false;
  doc.load(r.doc, QString());
  doc.undoStack()->resetClean(); // imported content has not been saved, even before the first edit
  view->zoomFit();
  return true;
}

void TemplateEditor::importMmsstvBatch()
{
  const QStringList sources = QFileDialog::getOpenFileNames(this, tr("Import MMSSTV templates"), templatesDir,
    tr("MMSSTV templates (*.mtm *.MTM);;All files (*)"));
  if (sources.isEmpty()) return;
  const QString destination = QFileDialog::getExistingDirectory(this, tr("Destination for converted templates"), templatesDir);
  if (destination.isEmpty()) return;
  if (QMessageBox::question(this, tr("Import MMSSTV templates"),
      tr("Convert %1 files to .qtempl in %2? Existing files are preserved. Conversion warnings will be listed afterwards; check the imported layouts before transmission.")
      .arg(sources.size()).arg(QDir::toNativeSeparators(destination))) != QMessageBox::Yes) return;
  QStringList report;
  int succeeded = 0;
  for (const auto &r : Mmsstv::importTemplates(sources, destination)) {
    if (r.target.isEmpty()) report << tr("%1: FAILED — %2").arg(r.source, r.error);
    else { ++succeeded; report << tr("%1 → %2").arg(r.source, r.target); emit templateSaved(r.target); }
    report << r.warnings;
    report << QString();
  }
  QDialog summary(this);
  summary.setWindowTitle(tr("MMSSTV import: %1 of %2 converted").arg(succeeded).arg(sources.size()));
  QVBoxLayout *layout = new QVBoxLayout(&summary);
  QPlainTextEdit *text = new QPlainTextEdit(report.join(QLatin1Char('\n')));
  text->setReadOnly(true); layout->addWidget(text);
  QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
  connect(buttons, &QDialogButtonBox::rejected, &summary, &QDialog::reject);
  layout->addWidget(buttons); summary.resize(760, 480); summary.exec();
}

bool TemplateEditor::newTemplate(const QSize &size, tpl::TemplateDocument::Layout layout, int starter)
{
  if (!maybeSave()) return false;
  tpl::TemplateDocument d = templatePresets::makeStarter(starter, size);
  d.layout = layout;
  doc.load(d, QString());
  view->zoomFit();
  return true;
}

bool TemplateEditor::newTemplateDialog()
{
  CanvasSizeDialog dlg(sizePresets, defaultSize, false, tr("New template"), this);
  dlg.setLayoutOption(true);
  dlg.setStarters(templatePresets::starterNames());
  if (dlg.exec() != QDialog::Accepted) return false;
  return newTemplate(dlg.canvasSize(), dlg.keepProportions() ? tpl::TemplateDocument::Anchored : tpl::TemplateDocument::Stretch, dlg.starter());
}

void TemplateEditor::revert()
{
  if (doc.fileName().isEmpty()) return;
  if (doc.isModified() && QMessageBox::question(this, tr("Revert"), tr("Throw away your changes and load %1 again?").arg(QFileInfo(doc.fileName()).fileName())) != QMessageBox::Yes) return;
  const tpl::LoadResult r = tpl::loadFile(doc.fileName());
  if (!r.ok)
    {
      problem(tr("Revert"), r.error);
      return;
    }
  doc.load(r.doc, doc.fileName());
}

QString TemplateEditor::defaultFileName() const
{
  const QString dir = doc.fileName().isEmpty() ? (templatesDir.isEmpty() ? QDir::homePath() : templatesDir) : QFileInfo(doc.fileName()).absolutePath();
  const QString base = doc.fileName().isEmpty() ? tr("template") : QFileInfo(doc.fileName()).completeBaseName();
  return QDir(dir).filePath(base + QLatin1Char('.') + QLatin1String(tpl::nativeExtension));
}

bool TemplateEditor::save()
{
  if (doc.fileName().isEmpty() || doc.isLegacyImport()) return saveAs();
  return saveTo(doc.fileName());
}

bool TemplateEditor::saveAs()
{
  QFileDialog dlg(this, tr("Save template"), defaultFileName());
  dlg.setAcceptMode(QFileDialog::AcceptSave);
  dlg.setNameFilter(tr("Templates (*.qtempl)"));
  dlg.setDefaultSuffix(QLatin1String(tpl::nativeExtension));
  dlg.selectFile(QFileInfo(defaultFileName()).fileName());
  if (dlg.exec() != QDialog::Accepted || dlg.selectedFiles().isEmpty()) return false;
  QString fn = dlg.selectedFiles().first();
  if (!fn.endsWith(QLatin1Char('.') + QLatin1String(tpl::nativeExtension), Qt::CaseInsensitive))
    {
      // a name typed with another extension (".templ"): this editor only writes its own format
      if (fn.endsWith(QLatin1String(".templ"), Qt::CaseInsensitive)) fn.chop(6);
      fn += QLatin1Char('.') + QLatin1String(tpl::nativeExtension);
    }
  return saveTo(fn);
}

bool TemplateEditor::saveTo(const QString &fileName)
{
  view->commitTextEdit();
  QString err;
  if (!tpl::saveFile(fileName, doc.doc(), &err))
    {
      problem(tr("Save template"), tr("%1 could not be saved:\n%2").arg(QDir::toNativeSeparators(fileName), err));
      return false;
    }
  const bool wasLegacy = doc.isLegacyImport();
  const QString oldFile = doc.fileName();
  doc.setFile(fileName, false);
  doc.markSaved();
  addRecent(fileName);
  if (wasLegacy && !oldFile.isEmpty() && QFileInfo(oldFile).canonicalFilePath() != QFileInfo(fileName).canonicalFilePath())
    statusBar()->showMessage(tr("Saved %1. The old file %2 is left as it was; the template list shows the new one.").arg(QFileInfo(fileName).fileName(), QFileInfo(oldFile).fileName()), 8000);
  else
    statusBar()->showMessage(tr("Saved %1").arg(QDir::toNativeSeparators(fileName)), 5000);
  emit templateSaved(fileName);
  return true;
}

void TemplateEditor::closeEvent(QCloseEvent *e)
{
  if (!maybeSave())
    {
      e->ignore();
      return;
    }
  writeSettings();
  e->accept();
}

// --- tools that need a dialog ---------------------------------------------------------------------------------
void TemplateEditor::insertImage()
{
  QStringList exts;
  for (const QByteArray &f : QImageReader::supportedImageFormats()) exts << QStringLiteral("*.") + QString::fromLatin1(f);
  const QString filter = tr("Pictures (%1);;All files (*)").arg(exts.join(QLatin1Char(' ')));
  const QString fn = QFileDialog::getOpenFileName(this, tr("Add picture"), QString(), filter);
  if (!fn.isEmpty()) view->insertImageFile(fn);
}

void TemplateEditor::changeCanvasSize()
{
  const QRectF old = doc.doc().canvas;
  CanvasSizeDialog dlg(sizePresets, old.size().toSize(), true, tr("Canvas size"), this);
  dlg.setLayoutOption(doc.doc().layout == tpl::TemplateDocument::Anchored);
  if (dlg.exec() != QDialog::Accepted) return;
  applyCanvasSettings(dlg.canvasSize(), dlg.scaleItems(), dlg.keepProportions() ? tpl::TemplateDocument::Anchored : tpl::TemplateDocument::Stretch);
}

void TemplateEditor::applyCanvasSize(const QSize &size, bool scaleItems)
{
  applyCanvasSettings(size, scaleItems, doc.doc().layout);
}

void TemplateEditor::applyCanvasSettings(const QSize &size, bool scaleItems, tpl::TemplateDocument::Layout layout)
{
  const QRectF old = doc.doc().canvas;
  const QRectF now(old.topLeft(), QSizeF(size));
  // refuse what the document would refuse, before an undo macro is opened for nothing
  const bool validSize = size.width() >= 1 && size.height() >= 1 && size.width() <= tpl::limits::maxCoord && size.height() <= tpl::limits::maxCoord;
  const bool sizeChanged = validSize && !(now == old || size == old.size().toSize());
  const bool layoutChanged = layout != doc.doc().layout;
  if (!sizeChanged && !layoutChanged) return;
  const QRectF newCanvas = sizeChanged ? now : old;
  QUndoStack *stack = doc.undoStack();
  stack->beginMacro(sizeChanged ? tr("Change canvas size") : tr("Change layout"));
  doc.setCanvas(newCanvas, layout, sizeChanged ? tr("Canvas size") : tr("Layout"));
  if (sizeChanged && scaleItems && old.width() > 0 && old.height() > 0)
    {
      const double sx = now.width() / old.width(), sy = now.height() / old.height();
      QVector<tpl::Element> scaled = doc.doc().items;
      for (tpl::Element &e : scaled) EditorDocument::scaleElement(e, old.topLeft(), sx, sy);
      doc.modifyItems(scaled, tr("Scale items"), QString(), false);   // the anchors keep their meaning when everything scales together
    }
  if (layoutChanged && layout == tpl::TemplateDocument::Anchored)
    {
      QVector<tpl::Element> anchoredItems = doc.doc().items;
      for (tpl::Element &e : anchoredItems) tpl::autoAnchor(e, newCanvas);
      doc.modifyItems(anchoredItems, tr("Set anchors"), QString(), false);
    }
  stack->endMacro();
  view->zoomFit();
}

void TemplateEditor::macroChosen(const QString &macro)
{
  if (!properties->insertMacro(macro)) view->addMacro(macro);
}

// --- recent files and settings ------------------------------------------------------------------------------------
void TemplateEditor::addRecent(const QString &fileName)
{
  QSettings s;
  s.beginGroup(QStringLiteral("TemplateEditor"));
  QStringList recent = s.value(QStringLiteral("recent")).toStringList();
  recent.removeAll(fileName);
  recent.prepend(fileName);
  while (recent.size() > maxRecent) recent.removeLast();
  s.setValue(QStringLiteral("recent"), recent);
  s.endGroup();
  updateRecentMenu();
}

void TemplateEditor::updateRecentMenu()
{
  QSettings s;
  const QStringList recent = s.value(QStringLiteral("TemplateEditor/recent")).toStringList();
  int shown = 0;
  for (const QString &f : recent)
    {
      if (shown >= maxRecent) break;
      if (!QFileInfo::exists(f)) continue;
      recentActs[shown]->setText(QStringLiteral("&%1 %2").arg(shown + 1).arg(QFileInfo(f).fileName()));
      recentActs[shown]->setData(f);
      recentActs[shown]->setToolTip(f);
      recentActs[shown]->setVisible(true);
      shown++;
    }
  for (int i = shown; i < maxRecent; i++) recentActs[i]->setVisible(false);
  recentMenu->setEnabled(shown > 0);
}

void TemplateEditor::readSettings()
{
  QSettings s;
  s.beginGroup(QStringLiteral("TemplateEditor"));
  restoreGeometry(s.value(QStringLiteral("geometry")).toByteArray());
  restoreState(s.value(QStringLiteral("state")).toByteArray());
  gridAct->setChecked(s.value(QStringLiteral("grid"), false).toBool());
  snapGridAct->setChecked(s.value(QStringLiteral("snapGrid"), false).toBool());
  snapObjectsAct->setChecked(s.value(QStringLiteral("snapObjects"), true).toBool());
  guideCenterAct->setChecked(s.value(QStringLiteral("guideCenter"), false).toBool());
  guide5Act->setChecked(s.value(QStringLiteral("guide5"), false).toBool());
  guide10Act->setChecked(s.value(QStringLiteral("guide10"), false).toBool());
  view->setGridSize(s.value(QStringLiteral("gridSize"), 16).toInt());
  s.endGroup();
  updateRecentMenu();
}

void TemplateEditor::writeSettings()
{
  QSettings s;
  s.beginGroup(QStringLiteral("TemplateEditor"));
  s.setValue(QStringLiteral("geometry"), saveGeometry());
  s.setValue(QStringLiteral("state"), saveState());
  s.setValue(QStringLiteral("grid"), gridAct->isChecked());
  s.setValue(QStringLiteral("snapGrid"), snapGridAct->isChecked());
  s.setValue(QStringLiteral("snapObjects"), snapObjectsAct->isChecked());
  s.setValue(QStringLiteral("guideCenter"), guideCenterAct->isChecked());
  s.setValue(QStringLiteral("guide5"), guide5Act->isChecked());
  s.setValue(QStringLiteral("guide10"), guide10Act->isChecked());
  s.setValue(QStringLiteral("gridSize"), view->gridSize());
  s.endGroup();
}
