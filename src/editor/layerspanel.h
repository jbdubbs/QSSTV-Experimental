/***************************************************************************
 *   The layers dock of the template editor: every item, topmost first.     *
 *   Click to select (also locked items), drag to change the stacking.      *
 *                                                                          *
 *   This program is free software; you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published by   *
 *   the Free Software Foundation; either version 3 of the License, or      *
 *   (at your option) any later version.                                    *
 ***************************************************************************/
#ifndef LAYERSPANEL_H
#define LAYERSPANEL_H

#include <QTimer>
#include <QWidget>
#include "editorcanvas.h"
#include "editordocument.h"

class QListWidget;

class LayersPanel : public QWidget
{
  Q_OBJECT
public:
  LayersPanel(EditorDocument *doc, EditorCanvas *canvas, QWidget *parent = nullptr);

protected:
  void changeEvent(QEvent *e) override;

private slots:
  void rebuild();
  void canvasSelectionChanged();
  void listSelectionChanged();
  void rowsMoved();

private:
  EditorDocument *doc;
  EditorCanvas *canvas;
  QListWidget *list;
  QTimer rebuildTimer;
  QStringList rowSignature;   // what the rows were built from
  bool syncing = false;
};

#endif  // LAYERSPANEL_H
