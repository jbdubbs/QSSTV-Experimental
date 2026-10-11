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

#include "soundconfig.h"
#include "ui_soundconfig.h"
#include "configparams.h"
#include "supportfunctions.h"
#include "soundqtmultimedia.h"

#include <QSettings>
#include <QMediaDevices>

#include <math.h>

int samplingrate;
double rxClock;
double txClock;
bool pulseSelected;
bool alsaSelected;
bool swapChannel;
bool duplicateChannel;
bool pttToneOtherChannel;
QString inputAudioDevice;
QString outputAudioDevice;
soundBase::edataSrc soundRoutingInput;
soundBase::edataDst soundRoutingOutput;

quint32 recordingSize;



soundConfig::soundConfig(QWidget *parent) :  baseConfig(parent), ui(new Ui::soundConfig)
{
  QStringList inputPCMList, outputPCMList;
  ui->setupUi(this);
  getCardList(inputPCMList, outputPCMList);
  ui->inputPCMNameComboBox->addItems(inputPCMList);
  ui->outputPCMNameComboBox->addItems(outputPCMList);
  // ALSA vs PulseAudio was a Linux-only choice from before the Qt Multimedia backend
  // unified all three OSes onto one audio path (Phase 4 of the cross-platform plan); the
  // radio buttons no longer select anything, so hide them rather than leave them looking
  // functional. pulseSelected/alsaSelected stay readable/writable in QSettings so an
  // existing config file round-trips harmlessly.
  ui->alsaRadioButton->hide();
  ui->pulseRadioButton->hide();
  // Devices come and go (USB, Bluetooth, PipeWire restarts); keep the lists current so a
  // newly available device can be picked without restarting the app (issue #73).
  static QMediaDevices mediaDevices;
  connect(&mediaDevices,&QMediaDevices::audioInputsChanged,this,&soundConfig::refreshDeviceLists);
  connect(&mediaDevices,&QMediaDevices::audioOutputsChanged,this,&soundConfig::refreshDeviceLists);
}

void soundConfig::refreshDeviceLists()
{
  QStringList inputPCMList, outputPCMList;
  getCardList(inputPCMList, outputPCMList);
  const QString curIn=ui->inputPCMNameComboBox->currentText();
  const QString curOut=ui->outputPCMNameComboBox->currentText();
  ui->inputPCMNameComboBox->clear();
  ui->outputPCMNameComboBox->clear();
  ui->inputPCMNameComboBox->addItems(inputPCMList);
  ui->outputPCMNameComboBox->addItems(outputPCMList);
  int i=ui->inputPCMNameComboBox->findText(curIn);
  if(i>=0) ui->inputPCMNameComboBox->setCurrentIndex(i);
  i=ui->outputPCMNameComboBox->findText(curOut);
  if(i>=0) ui->outputPCMNameComboBox->setCurrentIndex(i);
}


soundConfig::~soundConfig()
{
  delete ui;
}

void soundConfig::readSettings()
{
  QSettings qSettings;
  qSettings.beginGroup("SOUND");
  rxClock=qSettings.value("rxclock",BASESAMPLERATE).toDouble();
  txClock=qSettings.value("txclock",BASESAMPLERATE).toDouble();
  if(fabs(1-rxClock/BASESAMPLERATE)>0.002) rxClock=BASESAMPLERATE;
  if(fabs(1-txClock/BASESAMPLERATE)>0.002) txClock=BASESAMPLERATE;
  samplingrate=BASESAMPLERATE;
  inputAudioDevice=qSettings.value("inputAudioDevice","default").toString();
  outputAudioDevice=qSettings.value("outputAudioDevice","default").toString();
  alsaSelected=qSettings.value("alsaSelected",false).toBool();
  pulseSelected=qSettings.value("pulseSelected",false).toBool();
  swapChannel=qSettings.value("swapChannel",false).toBool();
  duplicateChannel=qSettings.value("duplicateChannel",false).toBool();
  pttToneOtherChannel=qSettings.value("pttToneOtherChannel",false).toBool();
  soundRoutingInput=  (soundBase::edataSrc)qSettings.value("soundRoutingInput",  0 ).toInt();
  soundRoutingOutput= (soundBase::edataDst)qSettings.value("soundRoutingOutput", 0 ).toInt();
  recordingSize= qSettings.value("recordingSize", 100 ).toInt();
  qSettings.endGroup();
  setParams();
}

void soundConfig::writeSettings()
{
  QSettings qSettings;
  getParams();
  qSettings.beginGroup("SOUND");
  qSettings.setValue("rxclock",rxClock);
  qSettings.setValue("txclock",txClock);
  qSettings.setValue("inputAudioDevice",inputAudioDevice);
  qSettings.setValue("outputAudioDevice",outputAudioDevice);
  qSettings.setValue("alsaSelected",alsaSelected);
  qSettings.setValue("pulseSelected",pulseSelected);
  qSettings.setValue("swapChannel",swapChannel);
  qSettings.setValue("duplicateChannel",duplicateChannel);
  qSettings.setValue("pttToneOtherChannel",pttToneOtherChannel);
  qSettings.setValue ("soundRoutingInput", soundRoutingInput );
  qSettings.setValue ("soundRoutingOutput",soundRoutingOutput );
  qSettings.setValue ("recordingSize",recordingSize );
  qSettings.endGroup();
}


void soundConfig::setParams()
{
  setValue(rxClock,ui->inputClockLineEdit,9);
  setValue(txClock,ui->outputClockLineEdit,9);
  setValue(inputAudioDevice,ui->inputPCMNameComboBox);
  setValue(outputAudioDevice,ui->outputPCMNameComboBox);
  setValue(alsaSelected,ui->alsaRadioButton);
  setValue(pulseSelected,ui->pulseRadioButton);
  setValue(swapChannel,ui->swapChannelCheckBox);
  setValue(duplicateChannel,ui->duplicateChannelCheckBox);
  setValue(pttToneOtherChannel,ui->pttToneCheckBox);
  if(soundRoutingInput==soundBase::SNDINCARD) ui->inFromCard->setChecked(true);
  else if (soundRoutingInput==soundBase::SNDINFROMFILE) ui->inFromFile->setChecked(true);
  else ui->inRecordFromCard->setChecked(true);

  if(soundRoutingOutput==soundBase::SNDOUTCARD) ui->outToCard->setChecked(true);
  else ui->outRecord->setChecked(true);
  setValue(recordingSize,ui->mbSpinBox);
}

void soundConfig::getParams()
{
  QString inputAudioDeviceCopy=inputAudioDevice;
  QString  outputAudioDeviceCopy=outputAudioDevice;
  bool alsaSelectedCopy=alsaSelected;


  soundBase::edataSrc soundRoutingInputCopy=soundRoutingInput;
  soundBase::edataDst soundRoutingOutputCopy=soundRoutingOutput;

  getValue(rxClock,ui->inputClockLineEdit);
  getValue(txClock,ui->outputClockLineEdit);
  getValue(inputAudioDevice,ui->inputPCMNameComboBox);
  getValue(outputAudioDevice,ui->outputPCMNameComboBox);
  getValue(alsaSelected,ui->alsaRadioButton);
  getValue(pulseSelected,ui->pulseRadioButton);
  getValue(swapChannel,ui->swapChannelCheckBox);
  getValue(duplicateChannel,ui->duplicateChannelCheckBox);
  getValue(pttToneOtherChannel,ui->pttToneCheckBox);

  if (ui->inFromCard->isChecked()) soundRoutingInput=soundBase::SNDINCARD;
  else if(ui->inFromFile->isChecked()) soundRoutingInput=soundBase::SNDINFROMFILE;
  else soundRoutingInput=soundBase::SNDINCARDTOFILE;

  if (ui->outToCard->isChecked()) soundRoutingOutput=soundBase::SNDOUTCARD;
  else soundRoutingOutput=soundBase::SNDOUTTOFILE;
  getValue(recordingSize,ui->mbSpinBox);
  changed=false;
  if(inputAudioDeviceCopy!=inputAudioDevice
     || outputAudioDeviceCopy!=outputAudioDevice
     || soundRoutingInputCopy!=soundRoutingInput
     || soundRoutingOutputCopy!=soundRoutingOutput
     || alsaSelectedCopy!=alsaSelected)
  {
    changed=true;
  }
}
