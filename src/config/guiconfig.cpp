/**************************************************************************
*   Copyright (C) 2000-2019 by Johan Maes                                 *
*   on4qz@telenet.be                                                      *
*   https://www.qsl.net/o/on4qz                                           *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/

#include "guiconfig.h"
#include "ui_guiconfig.h"
#include <QImageWriter>
#include <QColorDialog>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include "externaleditor.h"


int galleryRows;
int galleryColumns;
bool imageStretch;
bool smoothDisplayScaling;
QColor backGroundColor;
QColor imageBackGroundColor;
bool slowCPU;
bool lowRes;
bool confirmDeletion;
bool confirmClose;
QString externalEditorCommand;

guiConfig::guiConfig(QWidget *parent) : baseConfig(parent),  ui(new Ui::guiConfig)
{
  ui->setupUi(this);
  backGroundColorChanged=false;
  connect(ui->backgroundColorPushButton,SIGNAL(clicked()),SLOT(slotBGColorSelect()));
  connect(ui->imageBackgroundColorPushButton,SIGNAL(clicked()),SLOT(slotIBGColorSelect()));
  connect(ui->externalEditorBrowseButton,SIGNAL(clicked()),SLOT(slotExternalEditorBrowse()));
  connect(ui->externalEditorDetectButton,SIGNAL(clicked()),SLOT(slotExternalEditorDetect()));
}

guiConfig::~guiConfig()
{
  delete ui;
}

void guiConfig::readSettings()
{
  QSettings qSettings;
  // the option used to live on the Receive tab (RX/smoothDisplayScaling)
  bool legacySmooth=qSettings.value("RX/smoothDisplayScaling",true).toBool();
  qSettings.beginGroup("GUI");
  galleryRows=qSettings.value("galleryRows",4).toInt();
  galleryColumns=qSettings.value("galleryColumns",4).toInt();
  imageStretch=qSettings.value("imageStretch",true).toBool();
  smoothDisplayScaling=qSettings.value("smoothDisplayScaling",legacySmooth).toBool();
  backGroundColor=qSettings.value("backGroundColor",QColor(0,0,0)).value<QColor>();
  imageBackGroundColor=qSettings.value("imageBackGroundColor",QColor(128,128,128)).value<QColor>();
  slowCPU=qSettings.value("slowCPU",false).toBool();
  lowRes=qSettings.value("lowRes",false).toBool();
  confirmDeletion=qSettings.value("confirmDeletion",true).toBool();
  confirmClose=qSettings.value("confirmClose",true).toBool();
  externalEditorCommand=qSettings.value("externalEditorCommand","").toString();
  qSettings.endGroup();
  setParams();
}

void guiConfig::writeSettings()
{
  QSettings qSettings;
  getParams();
  qSettings.beginGroup("GUI");
  qSettings.setValue("galleryRows",galleryRows);
  qSettings.setValue("galleryColumns",galleryColumns);
  qSettings.setValue("imageStretch",imageStretch);
  qSettings.setValue("smoothDisplayScaling",smoothDisplayScaling);
  qSettings.setValue("backGroundColor",backGroundColor);
  qSettings.setValue("imageBackGroundColor",imageBackGroundColor);
  qSettings.setValue("slowCPU",slowCPU);
  qSettings.setValue("lowRes",lowRes);
  qSettings.setValue("confirmDeletion",confirmDeletion);
  qSettings.setValue("confirmClose",confirmClose);
  qSettings.setValue("externalEditorCommand",externalEditorCommand);


  qSettings.endGroup();
}

void guiConfig::getParams()
{
  int galleryRowsCopy=galleryRows;
  int galleryColumnsCopy=galleryColumns;
  getValue(galleryRows,ui->rowsSpinBox);
  getValue(galleryColumns, ui->columnsSpinBox);
  changed=false;
  if( galleryRowsCopy!=galleryRows || galleryColumnsCopy!=galleryColumns)
    changed=true;
  getValue(imageStretch,ui->stretchCheckBox);
  bool smoothCopy=smoothDisplayScaling;
  getValue(smoothDisplayScaling,ui->smoothScalingCheckBox);
  smoothChanged=(smoothCopy!=smoothDisplayScaling);
  getValue(slowCPU,ui->slowCPUCheckBox);
  getValue(lowRes,ui->lowResCheckBox);

  getValue(confirmDeletion,ui->confirmDeletionCheckBox);
  getValue(confirmClose,ui->confirmCloseCheckBox);
  externalEditorCommand=ui->externalEditorLineEdit->text().trimmed();
}

void guiConfig::setParams()
{
  setValue(galleryRows,ui->rowsSpinBox);
  setValue(galleryColumns, ui->columnsSpinBox);
  setValue(imageStretch,ui->stretchCheckBox);
  setValue(smoothDisplayScaling,ui->smoothScalingCheckBox);
  setColorLabel(backGroundColor,false);
  setColorLabel(imageBackGroundColor,true);
  setValue(slowCPU,ui->slowCPUCheckBox);
  setValue(lowRes,ui->lowResCheckBox);
  setValue(confirmDeletion,ui->confirmDeletionCheckBox);
  setValue(confirmClose,ui->confirmCloseCheckBox);
  ui->externalEditorLineEdit->setText(externalEditorCommand);
}


void guiConfig::slotBGColorSelect()
{
  QColor c;
  c=QColorDialog::getColor(backGroundColor,this,"",QColorDialog::ShowAlphaChannel);
  setColorLabel(c,false);
}

void guiConfig::slotIBGColorSelect()
{
  QColor c;
  c=QColorDialog::getColor(imageBackGroundColor,this,"",QColorDialog::ShowAlphaChannel);
  setColorLabel(c,true);
}


void guiConfig::slotExternalEditorBrowse()
{
  const QString fn=QFileDialog::getOpenFileName(this,tr("Picture editor program"));
  if(fn.isEmpty()) return;
  ui->externalEditorLineEdit->setText((fn.contains(' ') ? "\""+fn+"\"" : fn)+" %f");
}

void guiConfig::slotExternalEditorDetect()
{
  const QVector<externalEditor::Candidate> found=externalEditor::detect();
  if(found.isEmpty())
    {
      QMessageBox::information(this,tr("Picture editor"),tr("No picture editor (GIMP, Krita, KolourPaint, ...) was found. Browse for the program, or leave the field empty to use the system's default program for pictures."));
      return;
    }
  QMenu menu(this);
  for(const externalEditor::Candidate &c:found)
    {
      QAction *a=menu.addAction(c.label);
      a->setData(c.command);
      a->setToolTip(c.command);
    }
  QAction *chosen=menu.exec(ui->externalEditorDetectButton->mapToGlobal(QPoint(0,ui->externalEditorDetectButton->height())));
  if(chosen) ui->externalEditorLineEdit->setText(chosen->data().toString());
}

void guiConfig::setColorLabel(QColor c,bool image)
{
if (c.isValid())
  {
    if(!image)
      {
        backGroundColorChanged=true;
        QPalette palette = ui->backGroundColorLabel->palette();
        palette.setColor(ui->backGroundColorLabel->backgroundRole(), c);
        ui->backGroundColorLabel->setPalette(palette);
        backGroundColor=c;
      }
    else
      {
        QPalette palette = ui->imageBackGroundColorLabel->palette();
        palette.setColor(ui->imageBackGroundColorLabel->backgroundRole(), c);
        ui->imageBackGroundColorLabel->setPalette(palette);
        imageBackGroundColor=c;
      }
   }
}
