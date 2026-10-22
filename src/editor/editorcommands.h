/***************************************************************************
 *   Undo commands of the template editor (see EditorDocument).             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/
#ifndef EDITORCOMMANDS_H
#define EDITORCOMMANDS_H

#include <QUndoCommand>
#include "editordocument.h"

class AddItemsCommand : public QUndoCommand
{
public:
  AddItemsCommand(EditorDocument *doc, const QVector<tpl::Element> &items, const QString &text);
  void redo() override;
  void undo() override;
private:
  EditorDocument *doc;
  QVector<tpl::Element> items;
  QStringList ids;
};

class RemoveItemsCommand : public QUndoCommand
{
public:
  RemoveItemsCommand(EditorDocument *doc, const QStringList &ids, const QString &text);
  void redo() override;
  void undo() override;
private:
  EditorDocument *doc;
  QVector<int> positions;                //!< paint-order positions, ascending
  QVector<tpl::Element> items;           //!< parallel to positions
};

class ModifyItemsCommand : public QUndoCommand
{
public:
  ModifyItemsCommand(EditorDocument *doc, const QVector<tpl::Element> &before, const QVector<tpl::Element> &after,
                     const QString &text, const QString &mergeKey);
  void redo() override;
  void undo() override;
  int id() const override { return mergeKey.isEmpty() ? -1 : int(qHash(mergeKey) & 0x3fffffff); }
  bool mergeWith(const QUndoCommand *other) override;
private:
  EditorDocument *doc;
  QVector<tpl::Element> before, after;
  QString mergeKey;
  int epoch;
  QStringList idList() const;
};

class ReorderCommand : public QUndoCommand
{
public:
  ReorderCommand(EditorDocument *doc, const QStringList &newOrder, const QString &text);
  void redo() override;
  void undo() override;
private:
  EditorDocument *doc;
  QStringList before, after;
};

class CanvasCommand : public QUndoCommand
{
public:
  CanvasCommand(EditorDocument *doc, const QRectF &canvas, tpl::TemplateDocument::Layout layout, const QString &text);
  void redo() override;
  void undo() override;
  int id() const override { return 0x40000001; }
  bool mergeWith(const QUndoCommand *other) override;
private:
  EditorDocument *doc;
  QRectF beforeRect, afterRect;
  tpl::TemplateDocument::Layout beforeLayout, afterLayout;
  int epoch;
};

#endif  // EDITORCOMMANDS_H
