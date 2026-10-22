#include "editorcommands.h"

#include <algorithm>

// ---------------------------------------------------------------------------------------------------------------
AddItemsCommand::AddItemsCommand(EditorDocument *doc, const QVector<tpl::Element> &items, const QString &text)
  : doc(doc), items(items)
{
  for (const tpl::Element &e : items) ids.append(e.id);
  setText(text);
}

void AddItemsCommand::redo()
{
  for (const tpl::Element &e : items) doc->insertRaw(doc->doc().items.size(), e);
  doc->hintSelection(ids);
}

void AddItemsCommand::undo()
{
  for (int i = ids.size() - 1; i >= 0; i--) doc->removeRaw(ids[i]);
}

// ---------------------------------------------------------------------------------------------------------------
RemoveItemsCommand::RemoveItemsCommand(EditorDocument *doc, const QStringList &ids, const QString &text) : doc(doc)
{
  setText(text);
  for (int i = 0; i < doc->doc().items.size(); i++)
    {
      const tpl::Element &e = doc->doc().items[i];
      if (ids.contains(e.id))
        {
          positions.append(i);
          items.append(e);
        }
    }
}

void RemoveItemsCommand::redo()
{
  for (int i = items.size() - 1; i >= 0; i--) doc->removeRaw(items[i].id);
  doc->hintSelection(QStringList());
}

void RemoveItemsCommand::undo()
{
  QStringList restored;
  for (int i = 0; i < items.size(); i++)
    {
      doc->insertRaw(positions[i], items[i]);
      restored.append(items[i].id);
    }
  doc->hintSelection(restored);
}

// ---------------------------------------------------------------------------------------------------------------
ModifyItemsCommand::ModifyItemsCommand(EditorDocument *doc, const QVector<tpl::Element> &before, const QVector<tpl::Element> &after,
                                       const QString &text, const QString &mergeKey)
  : doc(doc), before(before), after(after), mergeKey(mergeKey), epoch(doc->mergeEpoch())
{
  setText(text);
}

QStringList ModifyItemsCommand::idList() const
{
  QStringList l;
  for (const tpl::Element &e : after) l.append(e.id);
  return l;
}

void ModifyItemsCommand::redo()
{
  for (const tpl::Element &e : after) doc->replaceRaw(e);
}

void ModifyItemsCommand::undo()
{
  for (const tpl::Element &e : before) doc->replaceRaw(e);
}

bool ModifyItemsCommand::mergeWith(const QUndoCommand *other)
{
  const ModifyItemsCommand *o = dynamic_cast<const ModifyItemsCommand *>(other);
  if (!o || mergeKey.isEmpty() || o->mergeKey != mergeKey || o->epoch != epoch || o->idList() != idList()) return false;
  after = o->after;
  return true;
}

// ---------------------------------------------------------------------------------------------------------------
ReorderCommand::ReorderCommand(EditorDocument *doc, const QStringList &newOrder, const QString &text)
  : doc(doc), before(doc->ids()), after(newOrder)
{
  setText(text);
}

void ReorderCommand::redo()
{
  doc->setOrderRaw(after);
}

void ReorderCommand::undo()
{
  doc->setOrderRaw(before);
}

// ---------------------------------------------------------------------------------------------------------------
CanvasCommand::CanvasCommand(EditorDocument *doc, const QRectF &canvas, tpl::TemplateDocument::Layout layout, const QString &text)
  : doc(doc), beforeRect(doc->doc().canvas), afterRect(canvas), beforeLayout(doc->doc().layout), afterLayout(layout),
    epoch(doc->mergeEpoch())
{
  setText(text);
}

void CanvasCommand::redo()
{
  doc->setCanvasRaw(afterRect, afterLayout);
}

void CanvasCommand::undo()
{
  doc->setCanvasRaw(beforeRect, beforeLayout);
}

bool CanvasCommand::mergeWith(const QUndoCommand *other)
{
  const CanvasCommand *o = dynamic_cast<const CanvasCommand *>(other);
  if (!o || o->epoch != epoch) return false;
  afterRect = o->afterRect;
  afterLayout = o->afterLayout;
  return true;
}
