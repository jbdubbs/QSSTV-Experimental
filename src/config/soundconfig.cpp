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
#ifdef Q_OS_ANDROID
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include "androidaudio.h"
#endif

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
int micPreset;
double rxGainDb;



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
  connect(ui->duplicateChannelCheckBox,&QCheckBox::toggled,this,&soundConfig::updateChannelEnables);
#ifdef Q_OS_ANDROID
  buildPhoneAudioBox();
#endif
}

#ifdef Q_OS_ANDROID
void soundConfig::buildPhoneAudioBox()
{
  QGroupBox *box=new QGroupBox(tr("Phone audio"),this);
  QFormLayout *form=new QFormLayout(box);
  micPresetCombo=new QComboBox(box);
  micPresetCombo->addItem(tr("Automatic: raw if the phone has it"));   // order = micPresetAuto, Unprocessed, VoiceRecognition, Standard
  micPresetCombo->addItem(tr("Unprocessed"));
  micPresetCombo->addItem(tr("Voice recognition"));
  micPresetCombo->addItem(tr("Standard (the phone may apply gain control and noise suppression)"));
  micPresetCombo->setToolTip(tr("What the system may do to the microphone signal"));
  form->addRow(tr("Microphone"),micPresetCombo);
  QWidget *gainRow=new QWidget(box);
  QHBoxLayout *gainLayout=new QHBoxLayout(gainRow);
  gainLayout->setContentsMargins(0,0,0,0);
  rxGainSlider=new QSlider(Qt::Horizontal,gainRow);
  rxGainSlider->setRange(0,30);
  rxGainLabel=new QLabel(gainRow);
  rxGainLabel->setMinimumWidth(rxGainLabel->fontMetrics().horizontalAdvance("+30 dB"));
  gainLayout->addWidget(rxGainSlider,1);
  gainLayout->addWidget(rxGainLabel);
  connect(rxGainSlider,&QSlider::valueChanged,this,[this](int v){rxGainLabel->setText(QString("+%1 dB").arg(v));});
  form->addRow(tr("Receive gain"),gainRow);
  QWidget *volumeRow=new QWidget(box);
  QHBoxLayout *volumeLayout=new QHBoxLayout(volumeRow);
  volumeLayout->setContentsMargins(0,0,0,0);
  volumeSlider=new QSlider(Qt::Horizontal,volumeRow);
  volumeSlider->setRange(0,100);
  QLabel *volumeLabel=new QLabel(volumeRow);
  volumeLabel->setMinimumWidth(volumeLabel->fontMetrics().horizontalAdvance("100 %"));
  volumeLayout->addWidget(volumeSlider,1);
  volumeLayout->addWidget(volumeLabel);
  // the phone's media volume is a system setting: it changes as soon as the slider moves
  connect(volumeSlider,&QSlider::valueChanged,this,[volumeLabel](int v){volumeLabel->setText(QString("%1 %").arg(v));});
  connect(volumeSlider,&QSlider::sliderReleased,this,[this]{androidSetMediaVolumePercent(volumeSlider->value());});
  volumeSlider->setToolTip(tr("The media volume of the phone, which sets how loud a transmission is"));
  form->addRow(tr("Media volume"),volumeRow);
  routeLabel=new QLabel(box);
  routeLabel->setWordWrap(true);
  form->addRow(tr("In use"),routeLabel);
  ui->verticalLayout_4->insertWidget(1,box);
}

void soundConfig::showEvent(QShowEvent *e)
{
  baseConfig::showEvent(e);
  // the page is built before the sound device is opened, and the volume keys change the volume at any time
  if(soundIOPtr) routeLabel->setText(soundIOPtr->routeSummary());
  volumeSlider->setValue(androidMediaVolumePercent());
}
#endif

// Swap and PTT-tone-on-other-channel have no meaning when both channels carry the same audio (issue #88)
void soundConfig::updateChannelEnables()
{
  bool dup=ui->duplicateChannelCheckBox->isChecked();
  ui->swapChannelCheckBox->setEnabled(!dup);
  ui->pttToneCheckBox->setEnabled(!dup);
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
  pttToneOtherChannel=qSettings.value("pttToneOtherChannel",false).toBool();
  // Default on, except for existing configs that rely on the PTT tone channel (duplicate overrides it)
  duplicateChannel=qSettings.value("duplicateChannel",!pttToneOtherChannel).toBool();
  soundRoutingInput=  (soundBase::edataSrc)qSettings.value("soundRoutingInput",  0 ).toInt();
  soundRoutingOutput= (soundBase::edataDst)qSettings.value("soundRoutingOutput", 0 ).toInt();
  recordingSize= qSettings.value("recordingSize", 100 ).toInt();
  micPreset=qSettings.value("micPreset",micPresetAuto).toInt();
  if(micPreset<micPresetAuto || micPreset>micPresetStandard) micPreset=micPresetAuto;
  rxGainDb=qSettings.value("rxGainDb",0.0).toDouble();
  if(!(rxGainDb>=0.0 && rxGainDb<=30.0)) rxGainDb=0.0;
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
  qSettings.setValue("micPreset",micPreset);
  qSettings.setValue("rxGainDb",rxGainDb);
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
  updateChannelEnables();
  if(soundRoutingInput==soundBase::SNDINCARD) ui->inFromCard->setChecked(true);
  else if (soundRoutingInput==soundBase::SNDINFROMFILE) ui->inFromFile->setChecked(true);
  else ui->inRecordFromCard->setChecked(true);

  if(soundRoutingOutput==soundBase::SNDOUTCARD) ui->outToCard->setChecked(true);
  else ui->outRecord->setChecked(true);
  setValue(recordingSize,ui->mbSpinBox);
#ifdef Q_OS_ANDROID
  micPresetCombo->setCurrentIndex(micPreset);
  rxGainSlider->setValue(int(rxGainDb+0.5));
  rxGainLabel->setText(QString("+%1 dB").arg(rxGainSlider->value()));
  volumeSlider->setValue(androidMediaVolumePercent());
  if(soundIOPtr) routeLabel->setText(soundIOPtr->routeSummary());
#endif
}

void soundConfig::getParams()
{
  QString inputAudioDeviceCopy=inputAudioDevice;
  QString  outputAudioDeviceCopy=outputAudioDevice;
  bool alsaSelectedCopy=alsaSelected;


  soundBase::edataSrc soundRoutingInputCopy=soundRoutingInput;
  soundBase::edataDst soundRoutingOutputCopy=soundRoutingOutput;
  const int micPresetCopy=micPreset;

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
#ifdef Q_OS_ANDROID
  micPreset=micPresetCombo->currentIndex();
  rxGainDb=rxGainSlider->value();
  if(soundIOPtr) soundIOPtr->setRxGainDb(rxGainDb);   // takes effect without reopening the stream
#endif
  changed=false;
  if(inputAudioDeviceCopy!=inputAudioDevice
     || outputAudioDeviceCopy!=outputAudioDevice
     || soundRoutingInputCopy!=soundRoutingInput
     || soundRoutingOutputCopy!=soundRoutingOutput
     || alsaSelectedCopy!=alsaSelected
     || micPresetCopy!=micPreset)   // the input preset is chosen when the stream is opened
  {
    changed=true;
  }
}
