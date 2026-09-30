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

#include "cameradialog.h"
#include "ui_cameradialog.h"

#include "appglobal.h"
#include "imagesettings.h"

#include <QMediaDevices>
#include <QVideoFrameFormat>
#include <QMessageBox>

cameraDialog::cameraDialog(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::cameraDialog),
  cameraPtr(nullptr)
{
  ui->setupUi(this);
  captureSession.setVideoSink(&videoSink);
  // Populate everything and pick the initial device/format/size *before* wiring up the
  // combo box signals below, same as the pre-Qt-Multimedia code did -- so populating the
  // combo boxes here can't trigger a premature restartCapturing() before exec() ever
  // deliberately starts capturing.
  listCameraDevices();
  if(cameraList.count()==0) return;
  connect(ui->settingsButton,SIGNAL(clicked()),SLOT(slotSettings()));
  connect(ui->devicesComboBox,SIGNAL(currentIndexChanged(int)),SLOT(slotDeviceChanged(int)));
  connect(ui->formatsComboBox,SIGNAL(currentIndexChanged(int)),SLOT(slotFormatChanged(int)));
  connect(ui->sizeComboBox,SIGNAL(currentIndexChanged(int)),SLOT(slotSizeChanged(int)));
  connect(&videoSink,&QVideoSink::videoFrameChanged,this,&cameraDialog::slotVideoFrameChanged);
}

cameraDialog::~cameraDialog()
{
  delete cameraPtr;
  delete ui;
}

int cameraDialog::exec()
{
  if(!restartCapturing())
    {
      QMessageBox::warning(this,"Capturing","Unable to start capturing");
      return QDialog::Rejected;
    }
  addToLog("cameracontrol exec",LOGCAM);
  int result=QDialog::exec();
  if(cameraPtr) cameraPtr->stop();
  if(result==QDialog::Accepted) return true;
  return false;
}

QImage *cameraDialog::getImage()
{
  return lastImage.isNull() ? nullptr : &lastImage;
}

void cameraDialog::slotVideoFrameChanged(const QVideoFrame &frame)
{
  if(!frame.isValid()) return;
  lastImage=frame.toImage();
  ui->viewFinder->openImage(lastImage);
}

void cameraDialog::slotSettings()
{
  if(!cameraPtr) return;
  imageSettings settingsDialog(cameraPtr,this);
  settingsDialog.exec();
}

void cameraDialog::listCameraDevices()
{
  cameraList=QMediaDevices::videoInputs();
  for(const QCameraDevice &d : cameraList) ui->devicesComboBox->addItem(d.description());
  if(cameraList.count()>0) setupFormatComboBox(cameraList.at(0));
}

void cameraDialog::setupFormatComboBox(const QCameraDevice &cd)
{
  ui->formatsComboBox->blockSignals(true);
  ui->formatsComboBox->clear();
  QList<int> seen;
  for(const QCameraFormat &f : cd.videoFormats())
    {
      int pf=(int)f.pixelFormat();
      if(seen.contains(pf)) continue;
      seen.append(pf);
      ui->formatsComboBox->addItem(QVideoFrameFormat::pixelFormatToString(f.pixelFormat()),pf);
    }
  if(ui->formatsComboBox->count()>0) ui->formatsComboBox->setCurrentIndex(0);
  ui->formatsComboBox->blockSignals(false);
  if(ui->formatsComboBox->count()>0) setupSizeComboBox(cd,ui->formatsComboBox->itemData(0).toInt());
}

void cameraDialog::setupSizeComboBox(const QCameraDevice &cd,int pixelFormat)
{
  ui->sizeComboBox->blockSignals(true);
  ui->sizeComboBox->clear();
  QList<QSize> seen;
  for(const QCameraFormat &f : cd.videoFormats())
    {
      if((int)f.pixelFormat()!=pixelFormat) continue;
      if(seen.contains(f.resolution())) continue;
      seen.append(f.resolution());
      ui->sizeComboBox->addItem(QString("%1x%2").arg(f.resolution().width()).arg(f.resolution().height()),f.resolution());
    }
  if(ui->sizeComboBox->count()>0) ui->sizeComboBox->setCurrentIndex(0);
  ui->sizeComboBox->blockSignals(false);
}

QCameraFormat cameraDialog::selectedFormat() const
{
  int devIdx=ui->devicesComboBox->currentIndex();
  if(devIdx<0 || devIdx>=cameraList.count()) return QCameraFormat();
  int pixelFormat=ui->formatsComboBox->currentData().toInt();
  QSize size=ui->sizeComboBox->currentData().toSize();
  for(const QCameraFormat &f : cameraList.at(devIdx).videoFormats())
    if((int)f.pixelFormat()==pixelFormat && f.resolution()==size) return f;
  return QCameraFormat();
}

void cameraDialog::slotDeviceChanged(int idx)
{
  if(idx<0 || idx>=cameraList.count()) return;
  setupFormatComboBox(cameraList.at(idx));
  restartCapturing();
}

void cameraDialog::slotFormatChanged(int idx)
{
  int devIdx=ui->devicesComboBox->currentIndex();
  if(idx<0 || devIdx<0 || devIdx>=cameraList.count()) return;
  setupSizeComboBox(cameraList.at(devIdx),ui->formatsComboBox->itemData(idx).toInt());
  restartCapturing();
}

void cameraDialog::slotSizeChanged(int idx)
{
  Q_UNUSED(idx);
  restartCapturing();
}

bool cameraDialog::restartCapturing()
{
  int devIdx=ui->devicesComboBox->currentIndex();
  if(devIdx<0 || devIdx>=cameraList.count()) return false;

  if(cameraPtr)
    {
      cameraPtr->stop();
      delete cameraPtr;
      cameraPtr=nullptr;
    }
  cameraPtr=new QCamera(cameraList.at(devIdx),this);
  captureSession.setCamera(cameraPtr);
  QCameraFormat fmt=selectedFormat();
  if(!fmt.isNull()) cameraPtr->setCameraFormat(fmt);
  cameraPtr->start();
  if(cameraPtr->error()!=QCamera::NoError)
    {
      addToLog(QString("camera error: %1").arg(cameraPtr->errorString()),LOGCAM);
      return false;
    }
  return true;
}
