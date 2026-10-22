/***************************************************************************
 *   The properties dock of the template editor: position, size, rotation,  *
 *   fill, gradient, outline, text and font of the selected item(s).        *
 *   Editing a field changes every selected item (where it applies) in one  *
 *   undo step; showing a selection never writes anything back.             *
 *                                                                          *
 *   This program is free software; you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published by   *
 *   the Free Software Foundation; either version 3 of the License, or      *
 *   (at your option) any later version.                                    *
 ***************************************************************************/
#ifndef PROPERTIESPANEL_H
#define PROPERTIESPANEL_H

#include <QScrollArea>
#include <QTimer>
#include <functional>
#include "editorcanvas.h"
#include "editordocument.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFontComboBox;
class QGroupBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class GradientEdit;

class PropertiesPanel : public QScrollArea
{
  Q_OBJECT
public:
  PropertiesPanel(EditorDocument *doc, EditorCanvas *canvas, QWidget *parent = nullptr);
  /*! puts `macro` (e.g. "%c") into the text of the selected text item, at the text cursor */
  bool insertMacro(const QString &macro);

private slots:
  void refresh();
  void replaceImage();

private:
  using Edit = std::function<void(tpl::Element &)>;
  void apply(const Edit &fn, const QString &text, const QString &mergeKey);
  QDoubleSpinBox *spin(double min, double max, double step, int decimals, const QString &suffix = QString());
  QPushButton *swatch(const QString &tip);
  void setSwatch(QPushButton *b, const QColor &c);
  QColor pickColor(const QColor &initial, const QString &title);
  void build();

  EditorDocument *doc;
  EditorCanvas *canvas;
  bool updating = false;
  QTimer refreshTimer;

  QLabel *emptyLabel;
  QGroupBox *geometryBox, *fillBox, *outlineBox, *textBox, *pictureBox;
  QDoubleSpinBox *xSpin, *ySpin, *wSpin, *hSpin, *rotSpin, *shearHSpin, *shearVSpin, *opacitySpin;
  QCheckBox *lockedCheck;
  QPushButton *fillButton;
  GradientEdit *gradientEdit;
  QPushButton *outlineColorButton;
  QDoubleSpinBox *outlineWidthSpin;
  QComboBox *outlineStyleCombo, *outlineJoinCombo;
  QLabel *outlineJoinLabel;
  QPlainTextEdit *textEdit;
  QLabel *macroWarning, *fontWarning;
  QFontComboBox *fontCombo;
  QDoubleSpinBox *sizeSpin, *spacingSpin;
  QComboBox *weightCombo;
  QCheckBox *italicCheck, *underlineCheck, *strikeCheck;
  QLabel *pictureInfo;
  QPushButton *replaceImageButton;
};

#endif  // PROPERTIESPANEL_H
