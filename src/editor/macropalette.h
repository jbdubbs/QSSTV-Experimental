/***************************************************************************
 *   The macro dock of the template editor: lists the %x macros a text can  *
 *   contain, what each one is replaced with, and inserts them.             *
 *                                                                          *
 *   This program is free software; you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published by   *
 *   the Free Software Foundation; either version 3 of the License, or      *
 *   (at your option) any later version.                                    *
 ***************************************************************************/
#ifndef MACROPALETTE_H
#define MACROPALETTE_H

#include <QHash>
#include <QWidget>

class QAction;
class QCheckBox;
class QListWidget;

class MacroPalette : public QWidget
{
  Q_OBJECT
public:
  explicit MacroPalette(QWidget *parent = nullptr);
  /*! a check box mirroring the "preview macros" action */
  void setPreviewAction(QAction *a);
  /*! the sample values shown next to each macro */
  void setValues(const QHash<QChar, QString> &values);

signals:
  void macroActivated(const QString &macro);   //!< e.g. "%c"

private:
  QListWidget *list;
  QCheckBox *previewCheck;
};

#endif  // MACROPALETTE_H
