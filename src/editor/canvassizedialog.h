/***************************************************************************
 *   Dialog for the size of a template's canvas: a mode's picture size or a  *
 *   custom size, optionally scaling the existing items along.              *
 *                                                                          *
 *   This program is free software; you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published by   *
 *   the Free Software Foundation; either version 3 of the License, or      *
 *   (at your option) any later version.                                    *
 ***************************************************************************/
#ifndef CANVASSIZEDIALOG_H
#define CANVASSIZEDIALOG_H

#include <QDialog>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

struct SizePreset
{
  QString label;     //!< "PD120, PD180 (640 × 496)"
  QSize size;
};

class QCheckBox;
class QComboBox;
class QRadioButton;
class QSpinBox;

class CanvasSizeDialog : public QDialog
{
  Q_OBJECT
public:
  /*! `current` is preselected; `scaleOption` shows the "scale the items" check box (not for a new template) */
  CanvasSizeDialog(const QVector<SizePreset> &presets, const QSize &current, bool scaleOption, const QString &title, QWidget *parent = nullptr);
  QSize canvasSize() const;
  bool scaleItems() const;

  /*! shows the layout choice: keep the proportions of the template on every mode (items stick to the edges they are anchored to)
      or stretch the whole template over the picture, as old templates do */
  void setLayoutOption(bool keepProportions);
  bool keepProportions() const;
  /*! shows a "Start from" list (a new template); entry 0 should be the blank one */
  void setStarters(const QStringList &names);
  int starter() const;

  static const int minSide = 16;
  static const int maxSide = 4096;

private slots:
  void presetChosen(int index);
  void sizeEdited();

private:
  QVector<SizePreset> presets;
  QComboBox *combo;
  QSpinBox *widthSpin, *heightSpin;
  QCheckBox *scaleCheck;
  QRadioButton *keepRadio = nullptr, *stretchRadio = nullptr;
  QWidget *layoutBox = nullptr;
  QComboBox *starterCombo = nullptr;
  QWidget *starterRow = nullptr;
  bool updating = false;
};

#endif  // CANVASSIZEDIALOG_H
