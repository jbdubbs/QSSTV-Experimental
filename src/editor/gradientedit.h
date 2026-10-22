/***************************************************************************
 *   Editor for a template gradient: kind, angle and a bar with draggable   *
 *   colour stops.                                                          *
 *                                                                          *
 *   This program is free software; you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published by   *
 *   the Free Software Foundation; either version 3 of the License, or      *
 *   (at your option) any later version.                                    *
 ***************************************************************************/
#ifndef GRADIENTEDIT_H
#define GRADIENTEDIT_H

#include <QWidget>
#include "templatedoc.h"

class QComboBox;
class QSpinBox;
class QPushButton;
class GradientStopBar;

class GradientEdit : public QWidget
{
  Q_OBJECT
public:
  explicit GradientEdit(QWidget *parent = nullptr);
  /*! shows `g` (does not emit changed()); `base` is the item's fill colour, used for the stops of a new gradient */
  void setGradient(const tpl::Gradient &g, const QColor &base);
  tpl::Gradient gradient() const { return grad; }

signals:
  void changed(const tpl::Gradient &g);     //!< the user edited it

private slots:
  void kindChanged(int index);
  void angleChanged(int value);
  void barChanged();
  void stopColor();
  void removeStop();

private:
  void updateControls();
  tpl::Gradient grad;
  QColor baseColor;
  bool updating = false;
  QComboBox *kind;
  QSpinBox *angle;
  GradientStopBar *bar;
  QPushButton *colorButton, *removeButton;
};

#endif  // GRADIENTEDIT_H
