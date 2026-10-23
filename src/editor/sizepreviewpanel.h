/***************************************************************************
 *   The size preview dock of the template editor: the template laid over   *
 *   a picture of any SSTV mode's size, drawn by the code that puts it on   *
 *   the transmitted picture. Shows at a glance what a stretched layout     *
 *   does to text on a mode of another shape, and how the anchors of a      *
 *   template that keeps its proportions place the items.                   *
 *                                                                          *
 *   This program is free software; you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published by   *
 *   the Free Software Foundation; either version 3 of the License, or      *
 *   (at your option) any later version.                                    *
 ***************************************************************************/
#ifndef SIZEPREVIEWPANEL_H
#define SIZEPREVIEWPANEL_H

#include <QHash>
#include <QImage>
#include <QTimer>
#include <QWidget>
#include "canvassizedialog.h"
#include "editordocument.h"

class QComboBox;
class QLabel;
class QPushButton;
class PreviewImage;

class SizePreviewPanel : public QWidget
{
  Q_OBJECT
public:
  explicit SizePreviewPanel(EditorDocument *doc, QWidget *parent = nullptr);

  /*! the sizes offered (the SSTV mode sizes); `preferred` is chosen when the user has not chosen one before */
  void setSizes(const QVector<SizePreset> &presets, const QSize &preferred);
  /*! the picture under the template (the Transmit tab's picture); null = a plain background */
  void setPicture(const QImage &img);
  /*! macro values shown in the text */
  void setSampleValues(const QHash<QChar, QString> &vars);

  QSize currentSize() const;
  /*! the preview as it is drawn now (rendered at once, not waiting for the timer) */
  QImage renderNow();

signals:
  void keepProportionsRequested();

protected:
  void showEvent(QShowEvent *e) override;

private slots:
  void schedule();
  void rebuild();
  void sizeChosen();

private:
  QImage background(const QSize &size) const;
  EditorDocument *doc;
  QComboBox *sizeCombo;
  PreviewImage *view;
  QLabel *hint;
  QPushButton *keepButton;
  QTimer timer;
  QImage picture;
  QHash<QChar, QString> vars;
  QVector<SizePreset> presets;
  QSize preferred;
};

#endif  // SIZEPREVIEWPANEL_H
