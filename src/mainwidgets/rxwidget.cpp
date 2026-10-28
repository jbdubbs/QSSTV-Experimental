#include "filedecoder.h"
#include "kgstvcontroller.h"
#include "gallerywidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QTemporaryFile>
#include <QCheckBox>
#include "rxwidget.h"
#include "appglobal.h"
#include "dispatcher.h"
#include "dirdialog.h"
#include "rxfunctions.h"
#include "imageviewer.h"
#include "sstvparam.h"
#include "filterparam.h"
#include "mainwindow.h"
#include "soundbase.h"
#include "vumeter.h"
#include "sstvrx.h"
#include "guiconfig.h"
#include "configparams.h"
#include "ftpthread.h"
#include "dispatch/filedecoder.h"


rxWidget::rxWidget(QWidget *parent):QWidget(parent),ui(new Ui::rxWidget)
{
  ui->setupUi(this);
  styleToolButtons(this);
  rxFunctionsPtr=new rxFunctions();
  QWidget *kgstvTab=new QWidget(this);QVBoxLayout *kgstvLayout=new QVBoxLayout(kgstvTab);
  QLabel *notice=new QLabel(tr("Experimental KG-STV. Original interoperability is unverified."),kgstvTab);notice->setWordWrap(true);kgstvLayout->addWidget(notice);
  kgstvModulation=new QComboBox(kgstvTab);kgstvModulation->setObjectName("kgstvRxModulation");kgstvModulation->addItems({"MSK","4L-FSK"});kgstvLayout->addWidget(kgstvModulation);
  kgstvAutoSave=new QCheckBox(tr("Save complete KG-STV images automatically"),kgstvTab);kgstvAutoSave->setObjectName("kgstvAutoSave");kgstvAutoSave->setChecked(autoSave);kgstvLayout->addWidget(kgstvAutoSave);
  connect(kgstvAutoSave,&QCheckBox::toggled,this,[this](bool checked){autoSave=checked;ui->autoSaveCheckBox->setChecked(checked);});
  connect(ui->autoSaveCheckBox,&QCheckBox::toggled,kgstvAutoSave,&QCheckBox::setChecked);
  kgstvStatus=new QLabel(tr("KG-STV ready"),kgstvTab);kgstvStatus->setWordWrap(true);kgstvStatus->setTextFormat(Qt::PlainText);kgstvStatus->setMaximumHeight(80);kgstvStatus->setObjectName("kgstvRxStatus");kgstvLayout->addWidget(kgstvStatus);
  kgstvHistory=new QPlainTextEdit(kgstvTab);kgstvHistory->setReadOnly(true);kgstvHistory->setMaximumBlockCount(1000);kgstvHistory->setObjectName("kgstvMessageHistory");kgstvLayout->addWidget(kgstvHistory);
  ui->settingsTableWidget->addTab(kgstvTab,tr("KG-STV (experimental)"));
  if(kgstvControllerPtr) {
    auto sync=[this](){QSignalBlocker block(kgstvModulation);kgstvModulation->setCurrentIndex(kgstvControllerPtr->options().fourLevel?1:0);};sync();
    connect(kgstvControllerPtr,&Kgstv::Controller::optionsChanged,this,sync,Qt::QueuedConnection);
    connect(kgstvModulation,QOverload<int>::of(&QComboBox::activated),this,[this,sync](int index){
      auto options=kgstvControllerPtr->options();options.fourLevel=index==1;QString error;
      if(!kgstvControllerPtr->configure(options,&error)){sync();QMessageBox::warning(this,tr("KG-STV"),error);}
    });
    connect(kgstvControllerPtr,&Kgstv::Controller::imageChanged,this,&rxWidget::slotKgstvImage,Qt::QueuedConnection);
    connect(kgstvControllerPtr,&Kgstv::Controller::messageReceived,this,&rxWidget::slotKgstvText,Qt::QueuedConnection);
    connect(kgstvControllerPtr,&Kgstv::Controller::statusChanged,this,&rxWidget::slotKgstvStatus,Qt::QueuedConnection);
    connect(kgstvControllerPtr,&Kgstv::Controller::localTextResult,this,&rxWidget::slotKgstvLocalText,Qt::QueuedConnection);
  }

  ui->syncWidget->setHorizontal(false);
  ui->syncWidget->setLabelText("S");

  ui->vuWidget->setHorizontal(true);
  ui->vuWidget->setLabelText("V");
  imageViewerPtr=ui->imageFrame;


  rebuildModeComboBox();
  QStringList writable;
  for (const QByteArray &format : QImageWriter::supportedImageFormats()) writable << QString::fromLatin1(format).toLower();
  foreach(QString f,commonImageFormats())
    {
      if(writable.contains(f)) ui->defaultImageFormatComboBox->addItem(f);
    }
  QStringList strLst;
  strLst << "Low" << "Normal"  << "High" << "DX";
  ui->sensitivityComboBox->insertItems(0,strLst);
  connect(&rxFunctionsPtr->sstvRxPtr->syncWideProc,SIGNAL(callReceived(QString)),SLOT(slotNewCall(QString)));
  connect(rxFunctionsPtr->sstvRxPtr,SIGNAL(resetCall()),SLOT(slotResetCall()));
  connect(ui->logPushButton,SIGNAL(clicked()),SLOT(slotLogCall()));
  connect(ui->whoPushButton,SIGNAL(clicked()),SLOT(slotWho()));
  connect(ui->fixFilePushButton,SIGNAL(clicked()),SIGNAL(fixFileRequested()));
  connect(&ff,SIGNAL(listingDone(bool)),SLOT(slotWhoResult(bool)));
  doRemove=false;
}

rxWidget::~rxWidget()
{
  writeSettings();
  rxFunctionsPtr->terminate();
  delete rxFunctionsPtr;
}

void rxWidget::init()
{
  splashMessage(QString( "Setting up RX" ));

  qApp->processEvents();
  readSettings();
  //  imageViewerPtr->createImage(QSize(320,256),QColor(0,0,128),imageStretch);
  imageViewerPtr->createImage(QSize(320,256),imageBackGroundColor,imageStretch);
  imageViewerPtr->setType(imageViewer::RXIMG);
  setSettingsTab();

  rxFunctionsPtr->init();


  // make connections after initialization
  connect(ui->startToolButton, SIGNAL(clicked()),SLOT(slotStart()));
  connect(ui->stopToolButton, SIGNAL(clicked()),SLOT(slotStop()));
  connect(ui->resyncToolButton,SIGNAL(clicked()),SLOT(slotResync()));
  connect(ui->decodeFileToolButton,SIGNAL(clicked()),SLOT(slotDecodeFile()));
  connect(ui->autoSaveCheckBox,SIGNAL(clicked()),SLOT(slotGetParams()));
  connect(ui->autoSlantAdjustCheckBox,SIGNAL(clicked()),SLOT(slotGetParams()));
  connect(ui->completeSpinBox,SIGNAL(valueChanged(int)),SLOT(slotGetParams()));
  connect(ui->defaultImageFormatComboBox,SIGNAL(currentIndexChanged(int)),SLOT(slotGetParams()));




  connect(ui->sstvModeComboBox,SIGNAL(currentIndexChanged(int)),SLOT(slotSyncSettingChanged()));
  connect(ui->sensitivityComboBox,SIGNAL(currentIndexChanged(int)),SLOT(slotSyncSettingChanged()));
  connect(ui->settingsTableWidget,SIGNAL(currentChanged(int)),this, SLOT(slotTransmissionMode(int)));
  connect(ui->eraseToolButton, SIGNAL(clicked()),SLOT(slotErase()));
  connect(ui->saveToolButton, SIGNAL(clicked()),SLOT(slotSave()));

  if(slowCPU)
    {
      ui->drmFACLabel->hide();
      ui->drmMSCLabel->hide();
      ui->drmMSCWidget->hide();
      ui->drmFACWidget->hide();
    }
  if(slowCPU || lowRes)
    {
      ui->rxNotificationList->hide();
      ui->whoPushButton->hide();
      //      ui->whoSpacer->hide();
    }
  setOnlineStatus(true, onlineStatusText);
}

void rxWidget::readSettings()
{
  QSettings qSettings;
  qSettings.beginGroup("RX");
  autoSlantAdjust=qSettings.value("autoSlantAdjust",false).toBool();
  autoSave=qSettings.value("autoSave",true).toBool();
  sensitivity=qSettings.value("sensitivity",1).toInt();
  sstvModeIndexRx=(esstvMode)qSettings.value("sstvModeIndexRx",0).toInt();
  defaultImageFormat=qSettings.value("defaultImageFormat","png").toString();
  if(ui->defaultImageFormatComboBox->findText(defaultImageFormat)<0) defaultImageFormat="png"; // saved format no longer offered
  minCompletion=qSettings.value("minCompletion",25).toInt();
  setParams();
  qSettings.endGroup();

}

void rxWidget::writeSettings()
{
  QSettings qSettings;
  qSettings.beginGroup("RX");
  getParams();
  qSettings.setValue("autoSlantAdjust",autoSlantAdjust);
  qSettings.setValue("autoSave",autoSave);
  qSettings.setValue("sensitivity",sensitivity);
  qSettings.setValue("sstvModeIndexRx",sstvModeIndexRx);
  qSettings.setValue("defaultImageFormat",defaultImageFormat);
  qSettings.setValue("minCompletion",minCompletion);
  qSettings.endGroup();
}

void rxWidget::getParams()
{
  int idx;
  getValue(autoSlantAdjust,ui->autoSlantAdjustCheckBox);
  getValue(autoSave,ui->autoSaveCheckBox);
  getIndex(sensitivity,ui->sensitivityComboBox);
  getIndex(idx,ui->sstvModeComboBox);
  esstvMode selected=(idx>=0 && idx<rxModeList.size()) ? rxModeList[idx] : NOTVALID;
  sstvModeIndexRx=(selected==NOTVALID) ? (esstvMode)0 : (esstvMode)((int)selected+1);
  getValue(defaultImageFormat,ui->defaultImageFormatComboBox);
  getValue(minCompletion,ui->completeSpinBox);
}

void rxWidget::setParams()
{
  setValue(autoSlantAdjust,ui->autoSlantAdjustCheckBox);
  setValue(autoSave,ui->autoSaveCheckBox);
  kgstvAutoSave->setChecked(autoSave);
  setIndex(sensitivity,ui->sensitivityComboBox);
  rebuildModeComboBox();
  setValue(defaultImageFormat,ui->defaultImageFormatComboBox);
  setValue(minCompletion,ui->completeSpinBox);
}

// Populates sstvModeComboBox ("Auto" always first) with every SSTV mode,
// rebuilds rxModeList (index -> esstvMode, index 0 == the "Auto" sentinel
// NOTVALID) in step, and re-selects whichever mode sstvModeIndexRx
// previously pointed to if it's still present (else falls back to
// "Auto") -- this guards against a stale index from an older settings
// file with a different NUMSSTVMODES. sstvModeIndexRx itself always
// stores the real esstvMode plus one (0 meaning "Auto"), never a raw
// combo-box row (see syncProcessor::init()'s own -1 conversion, which
// this preserves unchanged).
void rxWidget::rebuildModeComboBox()
{
  esstvMode currentReal=(sstvModeIndexRx==(esstvMode)0) ? NOTVALID : (esstvMode)((int)sstvModeIndexRx-1);
  ui->sstvModeComboBox->blockSignals(true);
  ui->sstvModeComboBox->clear();
  rxModeList.clear();
  ui->sstvModeComboBox->addItem("Auto");
  rxModeList.append(NOTVALID);
  for(int i=0;i<NUMSSTVMODES;i++)
    {
      esstvMode m=(esstvMode)i;
      ui->sstvModeComboBox->addItem(getSSTVModeNameLong(m));
      rxModeList.append(m);
    }
  int newIndex=(currentReal==NOTVALID) ? 0 : rxModeList.indexOf(currentReal);
  if(newIndex<0) newIndex=0; // stale index (e.g. old settings file) -> fall back to Auto
  ui->sstvModeComboBox->setCurrentIndex(newIndex);
  esstvMode nowSelected=rxModeList[newIndex];
  sstvModeIndexRx=(nowSelected==NOTVALID) ? (esstvMode)0 : (esstvMode)((int)nowSelected+1);
  ui->sstvModeComboBox->blockSignals(false);
}

bool rxWidget::setRxModeByName(const QString &name)
{
  esstvMode mode=NOTVALID;   // NOTVALID = Auto
  if(name.compare("auto",Qt::CaseInsensitive)!=0)
    {
      for(int i=0;i<NUMSSTVMODES;i++)
        {
          if(getSSTVModeNameShort((esstvMode)i).compare(name,Qt::CaseInsensitive)==0)
            {
              mode=(esstvMode)i;
              break;
            }
        }
      if(mode==NOTVALID) return false;
    }
  int idx=rxModeList.indexOf(mode);
  if(idx<0) return false;
  ui->sstvModeComboBox->blockSignals(true);
  ui->sstvModeComboBox->setCurrentIndex(idx);
  ui->sstvModeComboBox->blockSignals(false);
  sstvModeIndexRx=(mode==NOTVALID) ? (esstvMode)0 : (esstvMode)((int)mode+1);
  return true;
}

void rxWidget::setMinCompletion(int percent)
{
  // through the box, so a later getParams() (any change of the other RX settings) keeps the value; the box's
  // valueChanged() copies it into minCompletion
  ui->completeSpinBox->setMaximum(100);
  ui->completeSpinBox->setValue(percent);
}

void rxWidget::slotGetParams()
{
  getParams();
}


void rxWidget::slotStart()
{
  getParams();
  dispatcherPtr->startRX();
}

void rxWidget::slotStop()
{
  getParams();
  dispatcherPtr->idleAll();
}

void rxWidget::slotDecodeFile()
{
  fileDecoderPtr->chooseAndDecode(this);
}

void rxWidget::slotSyncSettingChanged()
{
  getParams();
  // the sync processors cache the mode filter and sensitivity (minMatchedLines) at init, so re-init a running receiver
  if(!rxFunctionsPtr->isIdle()) rxFunctionsPtr->restartRX();
}

void rxWidget::slotResync()
{
  rxFunctionsPtr->restartRX();
}

void rxWidget::setModeLock(int busy)
{
  int i;
  for(i=0;i<TRXNOMODE;i++)
    {
      ui->settingsTableWidget->setTabEnabled(i,(busy<0)||(i==busy));
    }
}

void rxWidget::setFixFileEnabled(bool enable)
{
  ui->fixFilePushButton->setEnabled(enable);
}

void rxWidget::syncModeTab(int m)
{
  ui->settingsTableWidget->blockSignals(true);
  ui->settingsTableWidget->setCurrentIndex(m);
  ui->settingsTableWidget->blockSignals(false);
}

void rxWidget::slotTransmissionMode(int rxtxMode)
{
  emit modeSwitch(rxtxMode);
}


void rxWidget::changeTransmissionMode(int rxtxMode)
{
  transmissionModeIndex=(etransmissionMode)rxtxMode;
  dispatcherPtr->idleAll();
  setSettingsTab();
  switch(transmissionModeIndex)
    {
    case TRXSSTV:
      mainWindowPtr->setSSTVDRMPushButton(false);
      ui->resyncToolButton->setEnabled(true);
      break;
    case TRXKGSTV:
      mainWindowPtr->setSSTVDRMPushButton(false);
      ui->resyncToolButton->setEnabled(false);
      break;
    case TRXDRM:
      mainWindowPtr->setSSTVDRMPushButton(true);
      ui->resyncToolButton->setEnabled(false);
      break;
    default:
      break;
    }
  dispatcherPtr->startRX();
}

void rxWidget::slotLogCall()
{
  QString call;
  call=ui->callLineEdit->text().toUpper();
  dispatcherPtr->logSSTV(call,false);
}

void rxWidget::slotNewCall(QString call)
{
  ui->callLineEdit->setText(call);
  dispatcherPtr->logSSTV(call,true);
}

void rxWidget::slotResetCall()
{
  ui->callLineEdit->clear();
}

void rxWidget::slotErase()
{
  rxFunctionsPtr->eraseImage();
  imageViewerPtr->createImage(transmissionModeIndex==TRXKGSTV?QSize(320,240):QSize(320,256),imageBackGroundColor,imageStretch);
}


void rxWidget::slotSave()
{
  QDateTime dt(QDateTime::currentDateTime().toUTC()); //this is compatible with QT 4.6
  QString path;
  QString info;
  dirDialog d(this);
  if(transmissionModeIndex==TRXSSTV || transmissionModeIndex==TRXKGSTV)
    {
      path=transmissionModeIndex==TRXKGSTV?rxDRMImagesPath:rxSSTVImagesPath;

    }
  else
    {
      QMessageBox::information(this,"Saving image","Not available in DRM mode");
      return;
    }

  info="";
  QString fileName=d.saveFileName(path,"*","png");
  if (fileName.isNull()) return ;
  getImageViewerPtr()->save(fileName,transmissionModeIndex==TRXKGSTV?QString("png"):defaultImageFormat,true,false);
  if(QFileInfo::exists(fileName)) getImageViewerPtr()->setFilename(fileName);
  if(transmissionModeIndex==TRXKGSTV)galleryWidgetPtr->putRxImage(fileName);
  else dispatcherPtr->saveImage(fileName,info);
}

void rxWidget::setOnlineStatus(bool online, QString info)
{
  QString fn;

  if(!online) dispatcherPtr->showOffLine();

  if(ff.isBusy())
    {
      return;
    }

  fn=myCallsign+"_"+info;
  //  ftpFunctions ff;
  // we can use onlineStatusInt directly because this function is only used from the main thread

  if(hybridFtpRemoteHost.isEmpty()) return;

  if(online && onlineStatusEnabled && transmissionModeIndex==TRXDRM)
    {
      ff.setupFtp("OnlineStatus",hybridFtpRemoteHost,hybridFtpPort,hybridFtpLogin, hybridFtpPassword,hybridFtpRemoteDirectory+"/"+onlineStatusDir);
      ff.uploadData(QByteArray("Dummy\r\n"),fn,false,true);
    }
  else if(!online && transmissionModeIndex==TRXDRM)
    {
      displayMBoxEvent *stmb;
      stmb= new displayMBoxEvent("Hybrid Server","Cleaning up files on server");
      QApplication::postEvent( dispatcherPtr, stmb );  // Qt will delete it when done
      ff.setupFtp("OnlineStatus",hybridFtpRemoteHost,hybridFtpPort,hybridFtpLogin, hybridFtpPassword,hybridFtpRemoteDirectory);


      if(!onlineStatusDir.isEmpty()&& onlineStatusEnabled)
        {
          ff.changePath(onlineStatusDir,true);
          ff.mremove("*"+myCallsign+"_*", true,false);
          ff.changePath("..",true);
        }
      if(!hybridFtpHybridFilesDirectory.isEmpty())
        {
          ff.changePath(hybridFtpHybridFilesDirectory,true);
          ff.mremove("*"+myCallsign+"-*", true,false);
          ff.changePath("..",true);
         }
       if(!hybridNotifyDir.isEmpty())
         {
          ff.changePath(hybridNotifyDir,true);
          ff.mremove("*"+myCallsign+"-*", true,true);
         }
    }
}



void rxWidget::slotWho()
{
  // get a list of online callsigns
  if(!ff.isBusy() && !onlineStatusDir.isEmpty())
    {
      ff.setupFtp("WhoResult",hybridFtpRemoteHost,hybridFtpPort,hybridFtpLogin, hybridFtpPassword,hybridFtpRemoteDirectory+"/"+onlineStatusDir);
      ff.listFiles("*", true);
    }
  //slotWhoResult is called when we have the info
}

void rxWidget::slotWhoResult(bool err )
{
  int i;
  QString info;
  QDateTime lastModif;
  QDateTime now = QDateTime::currentDateTime();
  QList <QUrlInfo> users;

  if(err) return;

  users=ff.getListing();

  for(i=0;i<users.count();i++)
    {
      lastModif=users.at(i).lastModified();
      if(lastModif.secsTo(now)<=3600)
        {
          info += users.at(i).name()+"\n";
        }
    }
  ui->rxNotificationList->setPlainText(info);
}

void rxWidget::setSettingsTab()
{

  int i;
  if((transmissionModeIndex>=0)&&(transmissionModeIndex<TRXNOMODE))
    {
      for(i=0;i<TRXNOMODE;i++)
        {
          if(i!=transmissionModeIndex) ui->settingsTableWidget->widget(i)->setEnabled(false);
        }
      ui->settingsTableWidget->widget(transmissionModeIndex)->setEnabled(true);
      QSignalBlocker blocker(ui->settingsTableWidget);
      ui->settingsTableWidget->setCurrentIndex(transmissionModeIndex);
    }
  ui->vuWidget->setMaximum(100);
  ui->vuWidget->setMinimum(0);
  ui->vuWidget->setValue(-15);
  ui->syncWidget->setLabelText(transmissionModeIndex==TRXKGSTV?"B":"S");
  ui->syncWidget->setToolTip(transmissionModeIndex==TRXKGSTV?tr("Received KG-STV image blocks"):QString());
  if(transmissionModeIndex==TRXKGSTV) {
    ui->syncWidget->setColors(Qt::red,QColor(255,165,0),Qt::green);
    ui->syncWidget->setMaximum(300);ui->syncWidget->setMinimum(0);
    ui->syncWidget->setValue(kgstvControllerPtr?kgstvControllerPtr->receivedCount():0);
    if(mainWindowPtr && mainWindowPtr->spectrumFramePtr)mainWindowPtr->spectrumFramePtr->displaySettings(false);
  }
  else if(transmissionModeIndex==TRXDRM)
    {
      ui->syncWidget->setColors(QColor(0,90,0),QColor(0,190,0),Qt::green);
      ui->syncWidget->setMaximum(25.);
      ui->syncWidget->setMinimum(5.);
      ui->syncWidget->setValue(0.);
      ui->vuWidget->setColors(QColor(255,50,0),Qt::green,Qt::red);
      if(mainWindowPtr && mainWindowPtr->spectrumFramePtr)mainWindowPtr->spectrumFramePtr->displaySettings(true);
    }
  else
    {
      ui->syncWidget->setColors(Qt::red,QColor(255,165,0),Qt::green);
      ui->syncWidget->setMaximum(10.);
      ui->syncWidget->setMinimum(0.);
      ui->syncWidget->setValue(0.);
      ui->vuWidget->setColors(QColor(255,50,0),Qt::green,Qt::red);
      if(mainWindowPtr && mainWindowPtr->spectrumFramePtr)mainWindowPtr->spectrumFramePtr->displaySettings(false);
    }
}

void rxWidget::startRX(bool st)
{
  if(st)
    {
      getParams();
      dispatcherPtr->startRX();
      addToLog("starting rxfunction run",LOGRXMAIN);
    }
  else
    {
      dispatcherPtr->idleAll();
    }
}

bool rxWidget::rxBusy()
{
  return rxFunctionsPtr->rxBusy();
}

void rxWidget::setSSTVStatusText(QString txt)
{
  ui->sstvStatusLineEdit->setText(txt);
}


vuMeter *rxWidget::vMeterPtr()
{
  return ui->vuWidget;
}

vuMeter *rxWidget::sMeterPtr()
{
  return ui->syncWidget;
}


void rxWidget::setKgstvBusy(bool busy){kgstvModulation->setEnabled(!busy);}
void rxWidget::slotKgstvImage(QImage image,int received,bool complete,quint64 epoch)
{
  if(transmissionModeIndex!=TRXKGSTV || !kgstvControllerPtr || epoch!=kgstvControllerPtr->receiveEpoch())return;
  imageViewerPtr->openImage(image);
  kgstvStatus->setText(tr("KG-STV: %1/300 blocks").arg(received));
  ui->syncWidget->setValue(received);
  if(!complete)kgstvSaved=false;
  if(complete && !kgstvSaved && fileDecoderPtr && fileDecoderPtr->isActive()) {
    fileDecoderPtr->kgstvImageDecoded(image);kgstvSaved=fileDecoderPtr->isBatch();
  }
  if(complete && !kgstvSaved && autoSave) {
    QTemporaryFile file(QDir(rxDRMImagesPath).filePath("KGSTV_"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmss_")+"XXXXXX.png"));
    if(file.open() && image.save(&file,"PNG") && file.flush()) {
      file.setAutoRemove(false);file.close();kgstvSaved=true;galleryWidgetPtr->putRxImage(file.fileName());
    } else kgstvStatus->setText(tr("KG-STV image received; automatic save failed."));
  }
}
void rxWidget::slotKgstvText(QString sender,QString text)
{
  if(fileDecoderPtr && fileDecoderPtr->isActive())fileDecoderPtr->kgstvTextDecoded(sender,text);
  kgstvHistory->appendPlainText(QString("[%1 UTC] %2: %3").arg(QDateTime::currentDateTimeUtc().toString("HH:mm:ss"),sender.isEmpty()?tr("Unknown sender"):sender,text));
}
void rxWidget::slotKgstvLocalText(QString text,bool sent)
{
  kgstvHistory->appendPlainText(QString("[%1 UTC] %2: %3").arg(QDateTime::currentDateTimeUtc().toString("HH:mm:ss"),sent?tr("TX sent locally"):tr("TX canceled/failed"),text));
}
void rxWidget::slotKgstvStatus(QString status)
{
  if(transmissionModeIndex==TRXKGSTV)kgstvStatus->setText(status);
}
