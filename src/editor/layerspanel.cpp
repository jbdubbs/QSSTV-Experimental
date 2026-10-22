#include "layerspanel.h"

#include <QAbstractItemModel>
#include <QEvent>
#include <QHBoxLayout>
#include <QListWidget>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>
#include "editoricons.h"

namespace
{
QString iconName(tpl::ItemType t)
{
  switch (t)
    {
    case tpl::ItemType::Text: return QStringLiteral("text");
    case tpl::ItemType::Rect: return QStringLiteral("rect");
    case tpl::ItemType::Ellipse: return QStringLiteral("ellipse");
    case tpl::ItemType::Line: return QStringLiteral("line");
    case tpl::ItemType::Image: return QStringLiteral("image");
    case tpl::ItemType::Replay: return QStringLiteral("replay");
    }
  return QString();
}
}  // namespace

LayersPanel::LayersPanel(EditorDocument *document, EditorCanvas *editorCanvas, QWidget *parent)
  : QWidget(parent), doc(document), canvas(editorCanvas)
{
  list = new QListWidget;
  list->setSelectionMode(QAbstractItemView::ExtendedSelection);
  list->setDragDropMode(QAbstractItemView::InternalMove);
  list->setDefaultDropAction(Qt::MoveAction);
  list->setIconSize(QSize(18, 18));
  list->setToolTip(tr("Items from front (top) to back (bottom). Drag to change the stacking order."));

  auto button = [this](const QString &icon, const QString &tip, auto slot) {
    QToolButton *b = new QToolButton;
    b->setIcon(editorIcons::icon(icon, palette()));
    b->setProperty("iconName", icon);
    b->setToolTip(tip);
    b->setAutoRaise(true);
    connect(b, &QToolButton::clicked, canvas, slot);
    return b;
  };
  QHBoxLayout *buttons = new QHBoxLayout;
  buttons->addWidget(button(QStringLiteral("front"), tr("Bring to front"), &EditorCanvas::bringToFront));
  buttons->addWidget(button(QStringLiteral("forward"), tr("Bring forward"), &EditorCanvas::bringForward));
  buttons->addWidget(button(QStringLiteral("backward"), tr("Send backward"), &EditorCanvas::sendBackward));
  buttons->addWidget(button(QStringLiteral("back"), tr("Send to back"), &EditorCanvas::sendToBack));
  buttons->addStretch();
  buttons->addWidget(button(QStringLiteral("lock"), tr("Lock or unlock the selected items"), &EditorCanvas::toggleLock));
  buttons->addWidget(button(QStringLiteral("delete"), tr("Delete the selected items"), &EditorCanvas::deleteSelection));

  QVBoxLayout *lay = new QVBoxLayout(this);
  lay->setContentsMargins(4, 4, 4, 4);
  lay->addWidget(list, 1);
  lay->addLayout(buttons);

  rebuildTimer.setSingleShot(true);
  connect(&rebuildTimer, &QTimer::timeout, this, &LayersPanel::rebuild);
  auto later = [this]() { rebuildTimer.start(0); };
  connect(doc, &EditorDocument::reset, this, later);
  connect(doc, &EditorDocument::itemInserted, this, later);
  connect(doc, &EditorDocument::itemRemoved, this, later);
  connect(doc, &EditorDocument::itemChanged, this, later);
  connect(doc, &EditorDocument::orderChanged, this, later);
  connect(canvas, &EditorCanvas::selectionChanged, this, &LayersPanel::canvasSelectionChanged);
  connect(list, &QListWidget::itemSelectionChanged, this, &LayersPanel::listSelectionChanged);
  connect(list->model(), &QAbstractItemModel::rowsMoved, this, &LayersPanel::rowsMoved, Qt::QueuedConnection);
  rebuild();
}

void LayersPanel::changeEvent(QEvent *e)
{
  QWidget::changeEvent(e);
  if (e->type() == QEvent::PaletteChange)
    {
      for (QToolButton *b : findChildren<QToolButton *>())
        if (b->property("iconName").isValid()) b->setIcon(editorIcons::icon(b->property("iconName").toString(), palette()));
      rowSignature.clear();   // icons change with the palette
      rebuildTimer.start(0);
    }
}

void LayersPanel::rebuild()
{
  // an item changes on every mouse move of a drag: only rebuild when a row would look different
  QStringList signature;
  for (const tpl::Element &e : doc->doc().items) signature << e.id + QLatin1Char('|') + canvas->describe(e) + (e.locked ? QStringLiteral("|L") : QString());
  if (signature == rowSignature && list->count() == signature.size()) return;
  rowSignature = signature;
  syncing = true;
  const int scroll = list->verticalScrollBar() ? list->verticalScrollBar()->value() : 0;
  list->clear();
  const QVector<tpl::Element> &all = doc->doc().items;
  for (int i = all.size() - 1; i >= 0; i--)
    {
      const tpl::Element &e = all[i];
      QListWidgetItem *it = new QListWidgetItem(editorIcons::icon(iconName(e.type), palette()), canvas->describe(e));
      it->setData(Qt::UserRole, e.id);
      if (e.locked)
        {
          it->setText(it->text() + QStringLiteral("  🔒"));
          it->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
        }
      list->addItem(it);
    }
  if (list->verticalScrollBar()) list->verticalScrollBar()->setValue(scroll);
  syncing = false;
  canvasSelectionChanged();
}

void LayersPanel::canvasSelectionChanged()
{
  if (syncing) return;
  syncing = true;
  const QStringList sel = canvas->selection();
  list->clearSelection();
  for (int i = 0; i < list->count(); i++)
    if (sel.contains(list->item(i)->data(Qt::UserRole).toString())) list->item(i)->setSelected(true);
  syncing = false;
}

void LayersPanel::listSelectionChanged()
{
  if (syncing) return;
  QStringList ids;
  for (QListWidgetItem *it : list->selectedItems()) ids << it->data(Qt::UserRole).toString();
  syncing = true;
  canvas->setSelection(ids);
  syncing = false;
}

void LayersPanel::rowsMoved()
{
  // the list shows front to back, the document stores back to front
  QStringList order;
  for (int i = list->count() - 1; i >= 0; i--) order << list->item(i)->data(Qt::UserRole).toString();
  doc->reorder(order, tr("Change stacking order"));
  rowSignature.clear();
  rebuildTimer.start(0);   // also puts the list right when the drop changed nothing
}
