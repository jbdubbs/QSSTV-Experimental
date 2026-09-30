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

#include "imagesettings.h"
#include "ui_imagesettings.h"

#include <QFormLayout>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>

imageSettings::imageSettings(QCamera *camera, QWidget *parent) :
  QDialog(parent),
  ui(new Ui::imageSettingsUi),
  cameraPtr(camera)
{
  ui->setupUi(this);
  ui->buttonBox->button(QDialogButtonBox::Ok)->setDefault(false);
  ui->buttonBox->button(QDialogButtonBox::Cancel)->setDefault(false);
  loadCapabilities();
}

imageSettings::~imageSettings()
{
  delete ui;
}

void imageSettings::loadCapabilities()
{
  if(!cameraPtr) return;
  QCameraDevice cd=cameraPtr->cameraDevice();
  ui->cardLabel->setText(cd.description());
  ui->deviceLabel->setText(QString::fromUtf8(cd.id()));
  ui->driverLabel->setText(cd.isDefault() ? "Yes" : "No");
  QString posStr="Unspecified";
  if(cd.position()==QCameraDevice::FrontFace) posStr="Front";
  else if(cd.position()==QCameraDevice::BackFace) posStr="Back";
  ui->busLabel->setText(posStr);

  bool haveAny=false;
  QWidget *tab=new QWidget();
  QFormLayout *form=new QFormLayout(tab);

  if(cameraPtr->supportedFeatures().testFlag(QCamera::Feature::ExposureCompensation))
    {
      haveAny=true;
      QSlider *sl=new QSlider(Qt::Horizontal,tab);
      sl->setRange(-20,20); // -2.0 .. +2.0 EV in 0.1 steps -- QCamera has no queryable range
      sl->setValue(qRound(cameraPtr->exposureCompensation()*10));
      connect(sl,&QSlider::valueChanged,this,&imageSettings::slotExposureChanged);
      form->addRow("Exposure compensation:",sl);
    }

  if(cameraPtr->maximumZoomFactor()>cameraPtr->minimumZoomFactor())
    {
      haveAny=true;
      QSlider *sl=new QSlider(Qt::Horizontal,tab);
      sl->setRange(qRound(cameraPtr->minimumZoomFactor()*10),qRound(cameraPtr->maximumZoomFactor()*10));
      sl->setValue(qRound(cameraPtr->zoomFactor()*10));
      connect(sl,&QSlider::valueChanged,this,&imageSettings::slotZoomChanged);
      form->addRow("Zoom:",sl);
    }

  static const struct { QCamera::WhiteBalanceMode mode; const char *label; } wbModes[] =
  {
    {QCamera::WhiteBalanceAuto,"Auto"},
    {QCamera::WhiteBalanceSunlight,"Sunlight"},
    {QCamera::WhiteBalanceCloudy,"Cloudy"},
    {QCamera::WhiteBalanceShade,"Shade"},
    {QCamera::WhiteBalanceTungsten,"Tungsten"},
    {QCamera::WhiteBalanceFluorescent,"Fluorescent"},
    {QCamera::WhiteBalanceFlash,"Flash"},
    {QCamera::WhiteBalanceSunset,"Sunset"},
  };
  QComboBox *wbCombo=nullptr;
  for(const auto &w : wbModes)
    {
      if(!cameraPtr->isWhiteBalanceModeSupported(w.mode)) continue;
      if(!wbCombo) wbCombo=new QComboBox(tab);
      wbCombo->addItem(w.label,(int)w.mode);
    }
  if(wbCombo && wbCombo->count()>1) // "Auto" alone isn't a meaningful choice
    {
      haveAny=true;
      int cur=wbCombo->findData((int)cameraPtr->whiteBalanceMode());
      wbCombo->setCurrentIndex(cur>=0 ? cur : 0);
      connect(wbCombo,QOverload<int>::of(&QComboBox::currentIndexChanged),this,&imageSettings::slotWhiteBalanceChanged);
      form->addRow("White balance:",wbCombo);
    }
  else
    {
      delete wbCombo;
    }

  if(haveAny)
    {
      ui->tabWidget->addTab(tab,"Controls");
    }
  else
    {
      delete tab;
      ui->generalTab->layout()->addWidget(new QLabel("This camera does not report any adjustable properties.",ui->generalTab));
    }
}

void imageSettings::slotExposureChanged(int sliderValue)
{
  if(cameraPtr) cameraPtr->setExposureCompensation(sliderValue/10.0f);
}

void imageSettings::slotZoomChanged(int sliderValue)
{
  if(cameraPtr) cameraPtr->setZoomFactor(sliderValue/10.0f);
}

void imageSettings::slotWhiteBalanceChanged(int index)
{
  QComboBox *cb=qobject_cast<QComboBox*>(sender());
  if(cameraPtr && cb) cameraPtr->setWhiteBalanceMode((QCamera::WhiteBalanceMode)cb->itemData(index).toInt());
}
