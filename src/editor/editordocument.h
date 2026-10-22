/***************************************************************************
 *   The template being edited: the document model plus its undo stack.    *
 *                                                                         *
 *   Every change a user makes goes through one of the push functions here *
 *   (add / remove / modify / reorder / canvas), each of which is one      *
 *   QUndoCommand. The "...Raw" functions are what the commands call; the *
 *   canvas also uses replaceRaw() for the live preview while a drag is    *
 *   in progress and then pushes a single command when it ends.            *
 *   Items are addressed by their id, so undo/redo never depends on a      *
 *   position or a pointer.                                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/
#ifndef EDITORDOCUMENT_H
#define EDITORDOCUMENT_H

#include <QObject>
#include <QStringList>
#include <QUndoStack>
#include "templatedoc.h"
#include "templaterender.h"

class EditorDocument : public QObject
{
  Q_OBJECT
public:
  explicit EditorDocument(QObject *parent = nullptr);

  const tpl::TemplateDocument &doc() const { return d; }
  QUndoStack *undoStack() { return &stack; }

  int indexOf(const QString &id) const;
  const tpl::Element *find(const QString &id) const;
  QStringList ids() const;                                //!< paint order, first = bottom
  QVector<tpl::Element> elements(const QStringList &ids) const;   //!< in paint order, unknown ids skipped

  /*! Replaces the document (not undoable): clears the undo stack, gives every item a unique id and re-fits the
      rectangle of text items to their text. */
  void load(const tpl::TemplateDocument &doc, const QString &fileName);
  QString fileName() const { return file; }
  bool isLegacyImport() const { return legacy; }          //!< loaded from a binary .templ: saving writes a new .qtempl
  void setFile(const QString &fileName, bool legacyImport);
  bool isModified() const { return !stack.isClean(); }
  void markSaved() { stack.setClean(); }

  /*! A fresh id of the form "text-3" that no item uses. */
  QString newId(tpl::ItemType type) const;

  // --- undoable edits -----------------------------------------------------------------------------------------
  /*! Inserts the items on top of the others (ids are made unique) and selects them. Returns the ids. */
  QStringList addItems(const QVector<tpl::Element> &items, const QString &text);
  void removeItems(const QStringList &ids, const QString &text);
  /*! Replaces the items having the ids of `after` by `after`. Items whose geometry depends on their content
      (text) are re-fitted first. A non-empty mergeKey lets consecutive edits of the same items fold into one
      undo step, until breakMerge() is called. */
  void modifyItems(const QVector<tpl::Element> &after, const QString &text, const QString &mergeKey = QString());
  /*! Same with the previous state given by the caller (the canvas took it before a drag started). */
  void commitModify(const QVector<tpl::Element> &before, const QVector<tpl::Element> &after, const QString &text);
  void reorder(const QStringList &newOrder, const QString &text);
  void setCanvas(const QRectF &canvas, tpl::TemplateDocument::Layout layout, const QString &text);
  void clearItems(const QString &text);

  /*! Consecutive merge-keyed edits stop folding together (called when the selection or the focus changes). */
  void breakMerge() { epoch++; }
  int mergeEpoch() const { return epoch; }

  /*! Moves the origin of the item's local rect without moving what is drawn: the image of `keep` (item
      coordinates) stays where it is on the canvas. Used when the rect of a text item follows its text, and by
      the resize handles. */
  static void setRectKeeping(tpl::Element &e, const QRectF &newRect, const QPointF &keep);
  /*! text items: rect = bounds of the text, the origin of the text staying put */
  static void fitTextRect(tpl::Element &e);

  // --- used by the commands and by the canvas (no undo step) --------------------------------------------------
  void insertRaw(int index, const tpl::Element &e);
  void removeRaw(const QString &id);
  void replaceRaw(const tpl::Element &e);
  void setOrderRaw(const QStringList &order);
  void setCanvasRaw(const QRectF &canvas, tpl::TemplateDocument::Layout layout);
  void hintSelection(const QStringList &ids) { emit selectionHint(ids); }

signals:
  void reset();                                           //!< the whole document was replaced
  void itemInserted(const QString &id);
  void itemRemoved(const QString &id);
  void itemChanged(const QString &id);
  void orderChanged();
  void canvasChanged();
  void fileChanged();                                     //!< file name / legacy flag
  void selectionHint(const QStringList &ids);             //!< after an undo/redo: select these items

private:
  tpl::TemplateDocument d;
  QUndoStack stack;
  QString file;
  bool legacy = false;
  int epoch = 0;
};

#endif  // EDITORDOCUMENT_H
