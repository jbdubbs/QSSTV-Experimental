#include "kgstvcontroller.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTemporaryFile>
#include "txwidget.h"
#include "appglobal.h"
#include "drmparams.h"
#include "rxwidget.h"
#include "rxfunctions.h"
#include "ui_txwidget.h"
#include <QTimeZone>
#include <QTimer>
#include <QStandardItemModel>
#include "avifenc.h"
#include <QPainter>
#include "configparams.h"
#include "dispatcher.h"
#include "supportfunctions.h"
#include "gallerywidget.h"
#include "waterfalltext.h"

#include "ui_freqform.h"
#include "ui_sweepform.h"
#include "cameradialog.h"
#include "soundbase.h"
#include "waterfallform.h"
#include "bsrform.h"
#include "mainwindow.h"
#include "rigcontrol.h"
#include "imageviewer.h"
#include "templatedoc.h"
#include "testpatternselection.h"
#include "gpsreader.h"
#include "placelookup.h"
#include "nmeaparser.h"
#include <QMessageBox>
#include <QMenu>
#include <QToolButton>


txWidget::txWidget(QWidget *parent) :  QWidget(parent), ui(new Ui::txWidget)
{
  QString tmp;
  ui->setupUi(this);
  styleToolButtons(this);
  ui->previewWidget->setType(imageViewer::PREVIEW);
  txFunctionsPtr=new txFunctions(this);
  QWidget *kgstvTab=new QWidget(this);QVBoxLayout *kgstvLayout=new QVBoxLayout(kgstvTab);
  QLabel *notice=new QLabel(tr("Experimental KG-STV; original interoperability is unverified. Images use 320x240 pixels."),kgstvTab);notice->setWordWrap(true);kgstvLayout->addWidget(notice);
  QFormLayout *form=new QFormLayout;
  kgstvModulation=new QComboBox(kgstvTab);kgstvModulation->setObjectName("kgstvTxModulation");kgstvModulation->addItems({"MSK","4L-FSK"});form->addRow(tr("Modulation"),kgstvModulation);
  kgstvQuality=new QSpinBox(kgstvTab);kgstvQuality->setObjectName("kgstvQuality");kgstvQuality->setRange(0,15);kgstvQuality->setToolTip(tr("Higher levels reduce block size and image quality."));form->addRow(tr("Compression (0..15)"),kgstvQuality);
  kgstvFec=new QCheckBox(tr("Payload error correction"),kgstvTab);kgstvFec->setObjectName("kgstvFec");form->addRow(kgstvFec);kgstvLayout->addLayout(form);
  kgstvText=new QPlainTextEdit(kgstvTab);kgstvText->setObjectName("kgstvTextInput");kgstvText->setPlaceholderText(tr("Standalone text: 1..200 ASCII bytes"));kgstvText->setMaximumHeight(80);kgstvLayout->addWidget(kgstvText);
  kgstvSendText=new QPushButton(tr("Send Text"),kgstvTab);kgstvSendText->setObjectName("kgstvSendText");kgstvLayout->addWidget(kgstvSendText);
  kgstvRequest=new QPushButton(tr("Request missing blocks (BSR)"),kgstvTab);kgstvRequest->setObjectName("kgstvRequestBlocks");kgstvLayout->addWidget(kgstvRequest);
  kgstvRepair=new QPushButton(tr("Send requested blocks from cached image"),kgstvTab);kgstvRepair->setObjectName("kgstvSendRepairs");kgstvLayout->addWidget(kgstvRepair);
  QLabel *cacheNotice=new QLabel(tr("Repairs use the last completed KG-STV image, even if another image is now loaded. Replies are sent manually."),kgstvTab);cacheNotice->setWordWrap(true);kgstvLayout->addWidget(cacheNotice);
  ui->settingsTableWidget->addTab(kgstvTab,tr("KG-STV (experimental)"));
  if(kgstvControllerPtr) {
    auto sync=[this](){const auto options=kgstvControllerPtr->options();QSignalBlocker a(kgstvModulation),b(kgstvQuality),c(kgstvFec);kgstvModulation->setCurrentIndex(options.fourLevel?1:0);kgstvQuality->setValue(options.quality);kgstvFec->setChecked(options.fec);};sync();
    connect(kgstvControllerPtr,&Kgstv::Controller::optionsChanged,this,sync,Qt::QueuedConnection);
    auto update=[this,sync](){Kgstv::Options options;options.quality=kgstvQuality->value();options.fec=kgstvFec->isChecked();options.fourLevel=kgstvModulation->currentIndex()==1;QString error;
      if(!kgstvControllerPtr->configure(options,&error)){sync();QMessageBox::warning(this,tr("KG-STV"),error);}
    };
    connect(kgstvModulation,QOverload<int>::of(&QComboBox::activated),this,[update](int){update();});
    connect(kgstvQuality,QOverload<int>::of(&QSpinBox::valueChanged),this,[update](int){update();});
    connect(kgstvFec,&QCheckBox::toggled,this,[update](bool){update();});
  }
  connect(kgstvSendText,&QPushButton::clicked,this,[this](){kgstvAction(0);});
  connect(kgstvRequest,&QPushButton::clicked,this,[this](){kgstvAction(1);});
  connect(kgstvRepair,&QPushButton::clicked,this,[this](){kgstvAction(2);});

  imageViewerPtr=ui->imageFrame;

  imageViewerPtr->displayImage();
  rebuildModeComboBox();
  sizeChanged=true;
  ui->sstvResizeComboBox->addItem("Stretch");
  ui->sstvResizeComboBox->addItem("Crop");
  ui->sstvResizeComboBox->addItem("Fit");
  ui->sstvResizeComboBox->addItem("Free");
  ui->sstvGridComboBox->addItem("Single image");
  ui->sstvGridComboBox->addItem("Grid 1x2");
  ui->sstvGridComboBox->addItem("Grid 2x1");
  ui->sstvGridComboBox->addItem("Grid 2x2");
  // the DRM tab carries its own copy of the two combos; they are kept in step with the SSTV ones
  for(int j=0;j<4;j++) ui->drmResizeComboBox->addItem(ui->sstvResizeComboBox->itemText(j));
  for(int j=0;j<4;j++) ui->drmGridComboBox->addItem(ui->sstvGridComboBox->itemText(j));
  connect(ui->sstvGridComboBox,SIGNAL(activated(int)),SLOT(slotGridChanged(int)));
  connect(ui->drmGridComboBox,SIGNAL(activated(int)),SLOT(slotGridChanged(int)));
  connect(ui->drmResizeComboBox,SIGNAL(activated(int)),SLOT(slotResizeChanged(int)));
  connect(imageViewerPtr,&imageViewer::fitModeChanged,ui->drmResizeComboBox,&QComboBox::setCurrentIndex);

  connect(ui->sstvModeComboBox,SIGNAL(activated(int)),SLOT(slotModeChanged(int )));
  connect(ui->sstvResizeComboBox,SIGNAL(activated(int)),SLOT(slotResizeChanged(int)));
  connect(imageViewerPtr,&imageViewer::fitModeChanged,ui->sstvResizeComboBox,&QComboBox::setCurrentIndex);   // follows the selected grid cell

  connect(ui->drmTxBandwidthComboBox,SIGNAL(activated(int)),SLOT(slotGetTXParams()));
  connect(ui->drmTxInterleaveComboBox,SIGNAL(activated(int)),SLOT(slotGetTXParams()));
  connect(ui->drmTxProtectionComboBox,SIGNAL(activated(int)),SLOT(slotGetTXParams()));
  connect(ui->drmTxQAMComboBox,SIGNAL(activated(int)),SLOT(slotGetTXParams()));
  connect(ui->drmTxModeComboBox,SIGNAL(activated(int)),SLOT(slotGetTXParams()));
  connect(ui->drmTxReedSolomonComboBox,SIGNAL(activated(int)),SLOT(slotGetTXParams()));
  connect(ui->drmEncodingComboBox,SIGNAL(activated(int)),SLOT(slotEncodingChanged(int)));
  if(!avifWriteSupported())
    {
      // built without libavif: keep JPEG2000 selectable only
      qobject_cast<QStandardItemModel *>(ui->drmEncodingComboBox->model())->item(imageViewer::ENCAVIF)->setEnabled(false);
      ui->drmEncodingComboBox->setToolTip(ui->drmEncodingComboBox->toolTip()+"\nAVIF is unavailable: this build has no libavif.");
    }

  connect(ui->templatesComboBox,SIGNAL(currentIndexChanged(int)),SLOT(slotGetParams()));
  {
    // Edit... opens the selected template in the template editor; its menu starts a new one
    QToolButton *editTemplate=new QToolButton(this);
    editTemplate->setText(tr("Edit..."));
    editTemplate->setToolTip(tr("Edit the selected template; the arrow starts a new one"));
    editTemplate->setStatusTip(editTemplate->toolTip());
    QMenu *templateMenu=new QMenu(editTemplate);
    templateMenu->addAction(tr("New template..."),this,&txWidget::slotNewTemplate);
    editTemplate->setMenu(templateMenu);
    editTemplate->setPopupMode(QToolButton::MenuButtonPopup);
    connect(editTemplate,&QToolButton::clicked,this,&txWidget::slotEditTemplate);
    ui->gridLayout_3->addWidget(editTemplate,1,2);
  }
  connect(ui->templateCheckBox,SIGNAL(toggled(bool)),SLOT(slotGetParams()));
  connect(ui->refreshPushButton,SIGNAL(clicked()),SLOT(slotGetParams()));
  connect(ui->cwCheckBox,SIGNAL(toggled(bool)),SLOT(slotGetTXParams()));
  connect(ui->voxCheckBox,SIGNAL(toggled(bool)),SLOT(slotGetTXParams()));
  connect(ui->toCallLineEdit,SIGNAL(editingFinished ()),SLOT(slotGetParams()));
  connect(ui->operatorLineEdit,SIGNAL(editingFinished ()),SLOT(slotGetParams()));
  connect(ui->rsvLineEdit,SIGNAL(editingFinished ()),SLOT(slotGetParams()));
  connect(ui->latLineEdit,SIGNAL(editingFinished ()),SLOT(slotPositionEdited()));
  connect(ui->lonLineEdit,SIGNAL(editingFinished ()),SLOT(slotPositionEdited()));
  connect(ui->placeLineEdit,SIGNAL(editingFinished ()),SLOT(slotGetParams()));
  connect(ui->gpsToolButton,SIGNAL(clicked()),SLOT(slotGetGps()));
  connect(gpsReaderPtr,&gpsReader::fixReady,this,&txWidget::slotGpsFix);
  connect(placeLookupPtr,&placeLookup::placeReady,this,&txWidget::slotPlaceReady);
  connect(ui->xPlainTextEdit,SIGNAL(editingFinished()),SLOT(slotGetParams()));
  connect(ui->yPlainTextEdit,SIGNAL(editingFinished()),SLOT(slotGetParams()));
  connect(ui->zPlainTextEdit,SIGNAL(editingFinished()),SLOT(slotGetParams()));


  connect(ui->startToolButton, SIGNAL(clicked()), this, SLOT(slotStart()));
  connect(ui->stopToolButton, SIGNAL(clicked()), this, SLOT(slotStop()));
  connect(ui->drmProfileComboBox,SIGNAL(activated(int)),SLOT(slotProfileChanged(int )));

  connect(ui->hybridCheckBox,SIGNAL(toggled(bool)),SLOT(slotHybridToggled()));
  connect(ui->uploadToolButton, SIGNAL(clicked()), this, SLOT(slotUpload()));

  connect(ui->generateToneToolButton, SIGNAL(clicked()), this, SLOT(slotGenerateSignal()));
  connect(ui->sweepToneToolButton, SIGNAL(clicked()), this, SLOT(slotSweepSignal()));
  connect(ui->repeaterToneToolButton, SIGNAL(clicked()), this, SLOT(slotGenerateRepeaterTone()));
  connect(ui->openToolButton, SIGNAL(clicked()), this, SLOT(slotFileOpen()));
  connect(ui->editToolButton, SIGNAL(clicked()), this, SLOT(slotEdit()));
  connect(ui->snapshotToolButton, SIGNAL(clicked()), this, SLOT(slotSnapshot()));
  connect(&beaconTimer,SIGNAL(timeout()),SLOT(slotBeaconTimer()));
  connect(&beaconTick,SIGNAL(timeout()),SLOT(slotBeaconTick()));
  beaconTimer.setSingleShot(true);
  beaconTick.setInterval(1000);
  connect(&beaconCap,&beaconCapture::frameReady,this,&txWidget::slotBeaconFrame);
  connect(&beaconCap,&beaconCapture::failed,this,&txWidget::slotBeaconFailed);
  connect(ui->binaryToolButton,SIGNAL(clicked()),this,SLOT(slotBinary()));
  connect(ui->sizeSlider,SIGNAL(valueChanged(int)),SLOT(slotSize(int)));
  connect(ui->sizeSlider,SIGNAL(sliderReleased()),SLOT(slotSizeReleased()));
  sizeApplyTimer.setSingleShot(true);
  sizeApplyTimer.setInterval(100);
  connect(&sizeApplyTimer,SIGNAL(timeout()),SLOT(slotSizeApply()));
  connect(ui->settingsTableWidget,SIGNAL(currentChanged(int)),this, SLOT(slotTransmissionMode(int)));
  connect(imageViewerPtr,SIGNAL(imageChanged()),SLOT(slotImageChanged()));
  connect(imageViewerPtr,SIGNAL(compressionReady()),SLOT(slotCompressionReady()));
  connect(ui->templateCheckBox,SIGNAL(toggled(bool)),SLOT(slotImageChanged()));
  connect(&notifyTimer,SIGNAL(timeout()),SLOT(slotNotifyTimeout()));
  connect(&ff,SIGNAL(listingDone(bool)),SLOT(slotListingDone(bool)));
  connect(&repeaterTxDelayTimer,SIGNAL(timeout()),SLOT(slotRepaterDelay()));
  maxSize=0;
  notifyTimer.setSingleShot(true);
  notifyTimer.setInterval(NOTIFYCHECKINTERVAL);
  repeaterTxDelayTimer.setSingleShot(true);

#if defined(__APPLE__) && QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
  // workaround for performance issue on MacOS 12.x
  // idea courtesy
  // https://stackoverflow.com/questions/69890284/qslider-in-qt-misbehaves-in-new-macos-monterey-v12-0-1-any-workaround
  ui->sizeSlider->setStyleSheet("\
                      QSlider::groove:horizontal {\
                          height: 8px; /* the groove expands to the size of the slider by default. by giving it a height, it has a fixed size */ \
                          background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #B1B1B1, stop:1 #c4c4c4);\
                          margin: 2px 0;\
                      }\
                      \
                      QSlider::handle:horizontal {\
                          background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #b4b4b4, stop:1 #8f8f8f);\
                          border: 1px solid #5c5c5c;\
                          width: 18px;\
                          margin: -2px 0; /* handle is placed by default on the contents rect of the groove. Expand outside the groove */ \
                          border-radius: 3px;\
                      }\
                  ");
#endif  // __APPLE__ && Qt < 6.3
}

txWidget::~txWidget()
{
  writeSettings();
  txFunctionsPtr->terminate();
  delete txFunctionsPtr;

  delete ui;
}

void txWidget::init()
{
  splashMessage(QString( "Setting up TX" ));
  qApp->processEvents();

  readSettings();
  initView();
  setProfileNames();
  repeaterIndex=0;
  repeaterTimer=new QTimer(this);
  connect(repeaterTimer,SIGNAL(timeout()),SLOT(slotRepeaterTimer()));
  repeaterTimer->start(60000*repeaterImageInterval);
  addToLog("Reapater Timer Started",LOGTXMAIN);
  imageViewerPtr->setType(imageViewer::TXIMG);
  slotModeChanged(sstvModeIndexTx);
  changeTransmissionMode(transmissionModeIndex);
  //  setSettingsTab();
  slotProfileChanged(0);
  if(lowRes)
    {
      ui->txNotificationList->hide();
      ui->refreshPushButton->hide();
      ui->previewWidget->hide();
    }
}

void txWidget::readSettings()
{
  QSettings qSettings;
  qSettings.beginGroup("TX");
  sstvModeIndexTx=((esstvMode)qSettings.value("sstvModeIndexTx",0).toInt());
  if((sstvModeIndexTx<M1) || (sstvModeIndexTx>=NOTVALID))
    {
      sstvModeIndexTx=M1;
    }

  templateIndex=qSettings.value("templateIndex",0).toInt();
  useTemplate=qSettings.value("useTemplate",false).toBool();

  useCW=qSettings.value("useCW",false).toBool();
  useVOX=qSettings.value("useVOX",false).toBool();
  useHybrid=qSettings.value("useHybrid",false).toBool();
  compressedSize=qSettings.value("compressedSize",5000).toUInt();
  gridLayout=qSettings.value("gridLayout",0).toInt();
  drmParams.bandwith=qSettings.value("drmBandWith",0).toInt();
  drmParams.interleaver=qSettings.value("drmInterLeaver",0).toInt();
  drmParams.protection=qSettings.value("drmProtection",0).toInt();
  drmParams.qam=qSettings.value("drmQAM",0).toInt();
  drmParams.robMode=qSettings.value("drmRobMode",0).toInt();
  drmParams.reedSolomon=qSettings.value("drmReedSolomon",0).toInt();
  int enc=qSettings.value("drmImageEncoding",imageViewer::ENCJPEG2000).toInt();
  if(enc!=imageViewer::ENCAVIF || !avifWriteSupported()) enc=imageViewer::ENCJPEG2000;
  imageViewerPtr->setEncoding(enc);
  imageViewerPtr->latitude=qSettings.value("latitude",QString()).toString();
  imageViewerPtr->longitude=qSettings.value("longitude",QString()).toString();
  imageViewerPtr->place=qSettings.value("place",QString()).toString();
  positionFormat=gpsCoordFormat;   // Configuration > GPS has been read by now
  if(positionFormat==COORDDEGMIN)
    {
      double degrees;
      if(parseCoordinateText(imageViewerPtr->latitude,true,degrees)) imageViewerPtr->latitude=formatCoordinate(degrees,true,positionFormat);
      if(parseCoordinateText(imageViewerPtr->longitude,false,degrees)) imageViewerPtr->longitude=formatCoordinate(degrees,false,positionFormat);
    }
  placeKey=imageViewerPtr->latitude+"|"+imageViewerPtr->longitude;   // the saved place belongs to the saved position
  qSettings.endGroup();
  setParams();
}

void txWidget::writeSettings()
{
  QSettings qSettings;
  slotGetParams();
  qSettings.beginGroup("TX");
  qSettings.setValue( "sstvModeIndexTx", sstvModeIndexTx);
  qSettings.setValue( "templateIndex", templateIndex);
  qSettings.setValue( "useTemplate", useTemplate);
  qSettings.setValue( "useVOX", useVOX);
  qSettings.setValue( "useCW", useCW);
  qSettings.setValue( "useHybrid", useHybrid);
  qSettings.setValue("drmBandWith",drmParams.bandwith);
  qSettings.setValue("drmInterLeaver",drmParams.interleaver);
  qSettings.setValue("drmProtection",drmParams.protection);
  qSettings.setValue("drmQAM",drmParams.qam);
  qSettings.setValue("drmRobMode",drmParams.robMode);
  qSettings.setValue("drmReedSolomon",drmParams.reedSolomon);
  qSettings.setValue("compressedSize",compressedSize);
  qSettings.setValue("drmImageEncoding",imageViewerPtr->encoding());
  qSettings.setValue("gridLayout",gridLayout);
  qSettings.setValue("latitude",imageViewerPtr->latitude);     // the position belongs to the station, not to one QSO
  qSettings.setValue("longitude",imageViewerPtr->longitude);
  qSettings.setValue("place",imageViewerPtr->place);
  qSettings.endGroup();
}

void txWidget::slotGetTXParams()
{
  // get only the params that don't require re-applying the template
  // used by prepareTX() and slotGetParams()
  int temp=0;
  getIndex(temp,ui->sstvModeComboBox);
  sstvModeIndexTx=txModeList[temp];
  getValue(useVOX,ui->voxCheckBox);
  getValue(useCW,ui->cwCheckBox);
  getIndex(drmParams.bandwith,ui->drmTxBandwidthComboBox);
  getIndex(drmParams.interleaver,ui->drmTxInterleaveComboBox);
  getIndex(drmParams.protection,ui->drmTxProtectionComboBox);
  getIndex(drmParams.qam,ui->drmTxQAMComboBox);
  getIndex(drmParams.robMode,ui->drmTxModeComboBox);
  getIndex(drmParams.reedSolomon,ui->drmTxReedSolomonComboBox);
  drmParams.callsign=myCallsign;
  updateTxTime();
  txFunctionsPtr->forgetTxFileName();
}


void txWidget::slotGetParams()
{
  getIndex(templateIndex,ui->templatesComboBox);
  getValue(useTemplate,ui->templateCheckBox);
  getValue(useHybrid,ui->hybridCheckBox);
  getValue(imageViewerPtr->toCall,ui->toCallLineEdit);
  getValue(imageViewerPtr->toOperator,ui->operatorLineEdit);
  getValue(imageViewerPtr->rsv,ui->rsvLineEdit);
  getValue(imageViewerPtr->latitude,ui->latLineEdit);
  getValue(imageViewerPtr->longitude,ui->lonLineEdit);
  getValue(imageViewerPtr->place,ui->placeLineEdit);
  getValue(imageViewerPtr->comment1,ui->xPlainTextEdit);
  getValue(imageViewerPtr->comment2,ui->yPlainTextEdit);
  getValue(imageViewerPtr->comment3,ui->zPlainTextEdit);
  slotGetTXParams();
  getValue(compressedSize,ui->sizeSlider);
  ui->uploadToolButton->setEnabled(useHybrid && (transmissionModeIndex==TRXDRM));
  if(txFunctionsPtr->getTXState()==txFunctions::TXIDLE)
    {
      applyTemplate();
    }
}

void txWidget::setParams()
{
  rebuildModeComboBox();
  {
    int idx=txModeList.indexOf(sstvModeIndexTx);
    if(idx<0) idx=0;
    setIndex(idx,ui->sstvModeComboBox);
    sstvModeIndexTx=txModeList[idx];
  }
  ui->templateCheckBox->blockSignals(true);
  ui->templatesComboBox->blockSignals(true);
  setIndex(templateIndex,ui->templatesComboBox);
  setValue(useTemplate,ui->templateCheckBox);
  ui->templateCheckBox->blockSignals(false);
  ui->templatesComboBox->blockSignals(false);
  setValue(useVOX,ui->voxCheckBox);
  setValue(useCW,ui->cwCheckBox);
  setValue(useHybrid,ui->hybridCheckBox);
  setValue(imageViewerPtr->latitude,ui->latLineEdit);
  setValue(imageViewerPtr->longitude,ui->lonLineEdit);
  setValue(imageViewerPtr->place,ui->placeLineEdit);
  setIndex(drmParams.bandwith,ui->drmTxBandwidthComboBox);
  setIndex(drmParams.interleaver,ui->drmTxInterleaveComboBox);
  setIndex(drmParams.protection,ui->drmTxProtectionComboBox);
  setIndex(drmParams.qam,ui->drmTxQAMComboBox);
  setIndex(drmParams.robMode,ui->drmTxModeComboBox);
  setIndex(drmParams.reedSolomon,ui->drmTxReedSolomonComboBox);
  ui->drmEncodingComboBox->setCurrentIndex(imageViewerPtr->encoding());
  if(compressedSize<MINDRMSIZE) compressedSize=MINDRMSIZE;
  if(compressedSize>MAXDRMSIZE) compressedSize=MAXDRMSIZE;
  setValue(compressedSize,ui->sizeSlider);
  ui->uploadToolButton->setEnabled(useHybrid && (transmissionModeIndex==TRXDRM));
  ui->sstvGridComboBox->setCurrentIndex(qBound(0,gridLayout,3));
  ui->drmGridComboBox->setCurrentIndex(qBound(0,gridLayout,3));
  updateGridControl();
  updateTxTime();
}

void txWidget::copyProfile(drmTxParams d)
{
  drmParams=d;
  setIndex(drmParams.bandwith,ui->drmTxBandwidthComboBox);
  setIndex(drmParams.interleaver,ui->drmTxInterleaveComboBox);
  setIndex(drmParams.protection,ui->drmTxProtectionComboBox);
  setIndex(drmParams.qam,ui->drmTxQAMComboBox);
  setIndex(drmParams.robMode,ui->drmTxModeComboBox);
  setIndex(drmParams.reedSolomon,ui->drmTxReedSolomonComboBox);
}

void txWidget::initView()
{
  imageViewerPtr->createImage(QSize(320,256),imageBackGroundColor,imageStretch);
  imageViewerPtr->setType(imageViewer::TXIMG);
  setupTemplatesComboBox();
  ui->txProgressBar->setRange(0,100);
}


void txWidget::setupTemplatesComboBox()
{
  // keep the selected template when the list changes (a template was saved or added): by name, so a new file
  // sorting in front of it does not move the selection, and without a signal per inserted item
  const QString keepName=ui->templatesComboBox->currentText();
  const QStringList sl=galleryWidgetPtr->getFilenames();
  int idx=keepName.isEmpty() ? -1 : sl.indexOf(keepName);
  if(idx<0) idx=qBound(0,templateIndex,qMax(0,(int)sl.count()-1));
  {
    QSignalBlocker blocker(ui->templatesComboBox);
    ui->templatesComboBox->clear();
    ui->templatesComboBox->addItems(sl);
    ui->templatesComboBox->setCurrentIndex(sl.isEmpty() ? -1 : idx);
  }
  slotGetParams();   // re-applies the template: its file may have changed
}


void txWidget::slotEditTemplate()
{
  if(txFunctionsPtr->txBusy())
    {
      QMessageBox::warning(this,tr("Template editor"),tr("Transmission busy, the template editor is not available"));
      return;
    }
  dispatcherPtr->openTemplateEditor(galleryWidgetPtr->getTemplateFileName(ui->templatesComboBox->currentIndex()));
}


void txWidget::slotNewTemplate()
{
  if(txFunctionsPtr->txBusy())
    {
      QMessageBox::warning(this,tr("Template editor"),tr("Transmission busy, the template editor is not available"));
      return;
    }
  dispatcherPtr->openTemplateEditor(QString());
}


QString txWidget::getPreviewFilename()
{
  return ui->previewWidget->getFilename();
}

void  txWidget::setPreviewWidget(QString fn)
{
  addToLog(QString("previewfile: %1").arg(fn),LOGTXMAIN);
  ui->previewWidget->openImage(fn,false,false,false,true);
  refreshReplay();
}

/*!
  The picture in the preview is what the replay slot of a template shows. A new one does not change anything else on the TX
  tab, so the TX picture is rendered again here (when the selected template has such a slot). Nothing is re-rendered while the
  picture is being sent; the transmission itself always renders it again first (prepareTx), so what is sent is current.
*/
void txWidget::refreshReplay()
{
  if(repeaterEnabled || calTxActive || !useTemplate) return;
  const QString templateFile=galleryWidgetPtr->getTemplateFileName(ui->templatesComboBox->currentIndex());
  if(templateFile.isEmpty()) return;
  const tpl::LoadResult tl=tpl::loadFileCached(templateFile);
  if(!tl.ok || !tl.doc.hasReplay()) return;
  if(txFunctionsPtr->txBusy())
    {
      replayStale=true;
      return;
    }
  replayStale=false;
  applyTemplate();
}



void txWidget::slotStart()
{
  if(mainWindowPtr->busyMode()>=0) return;
  if(imageViewerPtr->hasValidImage())
    {
      doTx=TXNORMAL;
      prepareTx();
    }
  else
    {
      QMessageBox::warning(this,"TX Error","No image loaded");
    }
}

bool txWidget::sendCalibrationImage(esstvMode mode,const QImage &image)
{
  if(calTxActive || mainWindowPtr->busyMode()>=0 || transmissionModeIndex!=TRXSSTV) return false;
  if(imageViewerPtr->gridColumns()*imageViewerPtr->gridRowCount()>1 || image.isNull()) return false;
  calSavedValid=imageViewerPtr->hasValidImage();
  calSavedImage=*imageViewerPtr->getImagePtr();
  calSavedFile=imageViewerPtr->getFilename();
  calSavedMode=sstvModeIndexTx;
  calSavedUseTemplate=useTemplate;
  calTxActive=true;
  sstvModeIndexTx=mode;
  useTemplate=false;
  imageViewerPtr->openImage(image);
  doTx=TXNORMAL;
  prepareTx();
  return true;
}

bool txWidget::sendDrmTestImage(const drmTxParams &params,const QImage &image,uint jp2Bytes,int encoding)
{
  if(calTxActive || mainWindowPtr->busyMode()>=0 || transmissionModeIndex!=TRXDRM) return false;
  if(imageViewerPtr->gridColumns()*imageViewerPtr->gridRowCount()>1 || image.isNull()) return false;
  calSavedValid=imageViewerPtr->hasValidImage();
  calSavedImage=*imageViewerPtr->getImagePtr();
  calSavedFile=imageViewerPtr->getFilename();
  calSavedMode=sstvModeIndexTx;
  calSavedUseTemplate=useTemplate;
  calTxActive=true;
  useTemplate=false;
  copyProfile(params);
  if(encoding>=0) imageViewerPtr->setEncoding(encoding);
  if(jp2Bytes>0) compressedSize=qBound((uint)MINDRMSIZE,jp2Bytes,(uint)MAXDRMSIZE);
  // the viewer's compression target is only set by the size slider; without a window it is uninitialised and the
  // jp2 encode of the picture then divides by garbage
  imageViewerPtr->setSize(compressedSize,true);
  imageViewerPtr->openImage(image);
  doTx=TXNORMAL;
  prepareTx();
  return true;
}

void txWidget::abortCalibrationTx()
{
  if(!calTxActive) return;
  dispatcherPtr->idleAll();
  restoreAfterCalibrationTx();
}

void txWidget::restoreAfterCalibrationTx()
{
  calTxActive=false;
  sstvModeIndexTx=calSavedMode;
  useTemplate=calSavedUseTemplate;
  if(calSavedValid)
    {
      imageViewerPtr->openImage(calSavedImage);
      imageViewerPtr->setFilename(calSavedFile);
    }
  else imageViewerPtr->clear();
  calSavedImage=QImage();
  applyTemplate();
  ui->startToolButton->setEnabled(true);
  enableButtons(true);
  emit calibrationTxFinished();
}

void txWidget::slotUpload()
{
  if(imageViewerPtr->hasValidImage())
    {
      doTx=TXUPLOAD;
      prepareTx();
    }
}


void txWidget::prepareTx()
{
  addToLog(QString("doTx=%1").arg(doTx),LOGTXMAIN);
  enableButtons(false);
  applyTemplate();
  switch (transmissionModeIndex)
    {
    case TRXSSTV: txFunctionsPtr->prepareTX(txFunctions::TXPREPARESSTV);
      break;
    case TRXKGSTV:
      ++kgstvTicket;
      txFunctionsPtr->setKgstvImage(imageViewerPtr->getDisplayedImage()->copy(),kgstvTicket);
      txFunctionsPtr->prepareTX(txFunctions::TXPREPAREKGSTV);
      break;
    case TRXDRM:  txFunctionsPtr->prepareTX(txFunctions::TXPREPAREDRMPIC);
      break;
    case TRXNOMODE:
      break;
    }
}

void txWidget::prepareTxComplete(bool ok,quint64 ticket)
{
  if(transmissionModeIndex==TRXKGSTV && !ticket)return;
  if(ticket && (ticket!=kgstvTicket || transmissionModeIndex!=TRXKGSTV))return;
  if(transmissionModeIndex==TRXKGSTV && (!ok || !kgstvControllerPtr || !kgstvControllerPtr->prepared())){enableButtons(true);return;}
  if(transmissionModeIndex==TRXKGSTV)updateTxTime();
  addToLog(QString("ok=%1, doTx=%2").arg(ok).arg(doTx),LOGTXMAIN);
  if (!ok)
    {
      addToLog("Upload/prepare failed",LOGTXMAIN);
      if(beaconPending)
        {
          beaconPending=false;
          beaconSkipped("the picture could not be prepared for transmission");
        }
      if(calTxActive) restoreAfterCalibrationTx();
      enableButtons(true);   // prepareTx() locked the controls
      ui->uploadToolButton->setEnabled(useHybrid && (transmissionModeIndex==TRXDRM));
    }
  ui->startToolButton->setEnabled(true);
  if (ok && doTx!=TXUPLOAD)
    {
      if (doTx==TXBINARY)
        {
          if (useHybrid)
            {
              QMessageBox mbox(mainWindowPtr);
              QPushButton *sendButton;
              mbox.setWindowTitle("TX Binary File");
              mbox.setText("The file has been uploaded ready for transmission");

              sendButton = mbox.addButton(tr("Start Transmitting"), QMessageBox::AcceptRole);
              mbox.setStandardButtons(QMessageBox::Cancel);

              mbox.exec();
              if (mbox.clickedButton() == sendButton) {
                  dispatcherPtr->startTX(txFunctions::TXSENDDRMBINARY);
                }
            }
          else
            {
              dispatcherPtr->startTX(txFunctions::TXSENDDRMBINARY);
            }
        }
      else
        {
          startTxImage();
        }
    }
}

void txWidget::startTxImage()
{
  addToLog("startTxImage",LOGTXMAIN);
  QDateTime dt(QDateTime::currentDateTime().toUTC()); //this is compatible with QT 4.6
  dt.setTimeZone(QTimeZone::UTC);
  if(!imageViewerPtr->hasValidImage())
    {
      addToLog("invalidImage",LOGTXMAIN);
      if(beaconPending)
        {
          beaconPending=false;
          beaconSkipped("no valid picture to transmit");
        }
      return;
    }
  QString fn;
  fn=imageViewerPtr->getFilename();
  QFileInfo finf(fn);

  bool started=false;
  switch(transmissionModeIndex)
    {
    case TRXSSTV:
      if(saveTXimages && !calTxActive)
        {
          fn=QString("%1/%2_%3.%4").arg(txSSTVImagesPath).arg(finf.baseName()).arg(dt.toString("yyyyMMdd_HHmmss")).arg(defaultImageFormat);
          imageViewerPtr->save(fn,defaultImageFormat,true,false);
          galleryWidgetPtr->txImageChanged();
        }
      addToLog("dispatch startTx",LOGTXMAIN);
      started=dispatcherPtr->startTX(txFunctions::TXSSTVIMAGE);
      break;
    case TRXDRM:
      if(saveTXimages && !calTxActive)
        {
          fn=QString("%1/%2_%3.%4").arg(txDRMImagesPath).arg(finf.baseName()).arg(dt.toString("yyyyMMdd_HHmmss")).arg(defaultImageFormat);
          imageViewerPtr->save(fn,defaultImageFormat,true,false);
          galleryWidgetPtr->txImageChanged();
        }
      started=dispatcherPtr->startTX(txFunctions::TXSENDDRMPIC);
      break;
      //    case FAX:
      //    break;
    case TRXKGSTV:
      if(saveTXimages) {
        QTemporaryFile archived(QDir(txDRMImagesPath).filePath("KGSTV_"+dt.toString("yyyyMMdd_HHmmss_")+"XXXXXX.png"));
        if(archived.open() && txFunctionsPtr->kgstvPreparedImage().save(&archived,"PNG") && archived.flush()) {
          archived.setAutoRemove(false);archived.close();galleryWidgetPtr->txImageChanged();
        } else QMessageBox::warning(this,tr("KG-STV"),tr("Could not archive the prepared TX image."));
      }
      if(kgstvControllerPtr && kgstvControllerPtr->prepared())started=dispatcherPtr->startTX(txFunctions::TXSENDKGSTV);
      break;
    case TRXNOMODE:
      break;
    }
  if(beaconPending)
    {
      beaconPending=false;
      if(started) beaconError.clear();
      else beaconSkipped("the transmitter could not be started (check the sound output)");
    }
  if(started)
    {
      ui->startToolButton->setEnabled(false);
    }
  else if(calTxActive)
    {
      restoreAfterCalibrationTx();
    }
}

void txWidget::sendBSR()
{
  if(transmissionModeIndex==TRXKGSTV){kgstvAction(1);return;}
  if(mainWindowPtr->busyMode()>=0) return;
  QByteArray *p;
  bsrForm::eResult res;
  bsrForm bsrf;
  bsrf.init();
  if(!bsrf.hasBSR())
    {
      QMessageBox::information(this,"BSR","No incomplete DRM image has been received, nothing to request.");
      return;
    }
  res=(bsrForm::eResult)bsrf.exec();
  if(res==bsrForm::CANCEL) return;
  p=bsrf.getBA(res==bsrForm::COMPAT);
  if(p==NULL)
    {
      QMessageBox::warning(this,"BSR","The selected image has no missing segments to request.");
      return;
    }
  txFunctionsPtr->sendBSR(p,bsrf.getDRMParams());
}


void txWidget::sendWFID()
{

  waterfallPtr->setText(myCallsign);
  dispatcherPtr->startTX(txFunctions::TXSENDWFID);
  addToLog("sendWFID",LOGTXMAIN);
}

void txWidget::sendCWID()
{
  dispatcherPtr->startTX(txFunctions::TXSENDCWID);
  addToLog("sendWFID",LOGTXMAIN);
}

void txWidget::sendWfText()
{
  waterfallForm wf;
  if((wf.exec()==QDialog::Accepted)&&(!wf.text().isEmpty()))
    {
      waterfallPtr->setText(wf.text());
      dispatcherPtr->startTX(txFunctions::TXSENDWFID);
      addToLog("sendID",LOGTXMAIN);
    }
}

void txWidget::slotStop()
{
  ++kgstvTicket;
  if(kgstvControllerPtr)kgstvControllerPtr->cancelTransmit();
  if(calTxActive) restoreAfterCalibrationTx();
  if(replayStale) refreshReplay();
  ui->startToolButton->setEnabled(true);
  enableButtons(true);
  dispatcherPtr->startRX();
  emit txStopped();
}


void txWidget::enableButtons(bool enable)
{
  ui->uploadToolButton->setEnabled(enable && useHybrid && (transmissionModeIndex==TRXDRM));
  ui->hybridCheckBox->setEnabled(enable && transmissionModeIndex==TRXDRM);
  ui->startToolButton->setEnabled(enable);
  ui->sstvModeComboBox->setEnabled(enable);
  bool drmSingleImage=(transmissionModeIndex==TRXDRM && ui->sstvGridComboBox->currentIndex()==0);
  ui->sstvResizeComboBox->setEnabled(enable);
  ui->drmResizeComboBox->setEnabled(enable && !drmSingleImage);
  ui->drmGridComboBox->setEnabled(enable);
  ui->voxCheckBox->setEnabled(enable);
  ui->cwCheckBox->setEnabled(enable && transmissionModeIndex!=TRXKGSTV);
  ui->templateCheckBox->setEnabled(enable);
  ui->templatesComboBox->setEnabled(enable);
  ui->refreshPushButton->setEnabled(enable);
  // the size slider re-encodes the picture being transmitted: lock it during TX (DRM mode only)
  bool drmSize=enable && (transmissionModeIndex==TRXDRM);
  if(!drmSize) sizeApplyTimer.stop();
  ui->sizeSlider->setEnabled(drmSize);
  ui->sizeLabel->setEnabled(drmSize);
  ui->sizeKbLabel->setEnabled(drmSize);
  ui->sizeDurationLabel->setEnabled(drmSize);
  ui->settingsTableWidget->setEnabled(enable);
  kgstvModulation->setEnabled(enable);kgstvQuality->setEnabled(enable);kgstvFec->setEnabled(enable);
  kgstvText->setEnabled(enable);kgstvSendText->setEnabled(enable);kgstvRequest->setEnabled(enable);kgstvRepair->setEnabled(enable);
  if(rxWidgetPtr)rxWidgetPtr->setKgstvBusy(!enable);
}



void txWidget::slotFileOpen()
{
  QString fileName;
  imageViewerPtr->openImage(fileName,txStockImagesPath,true,true,true,false,true);
  applyTemplate();
}

void txWidget::slotGenerateSignal()
{
  QDialog qd;
  Ui::freqForm ff;
  ff.setupUi(&qd);

  QSettings qSettings;
  qSettings.beginGroup("TX");
  setValue(qSettings.value("toneFreq",ff.frequencySpinBox->value()).toInt(),ff.frequencySpinBox);
  setValue(qSettings.value("toneDuration",ff.durationSpinBox->value()).toInt(),ff.durationSpinBox);
  qSettings.endGroup();

  // keep Transmit/STOP the size of the original Transmit button (the red style would change its size hint)
  ff.txPushButton->setFixedSize(ff.txPushButton->sizeHint());
  const QFont txFont=ff.txPushButton->font(); // a stylesheet resets the font, so reapply it

  bool transmitting=false;
  QTimer toneTimer;
  toneTimer.setSingleShot(true);

  auto showIdle=[&]()
  {
    transmitting=false;
    toneTimer.stop();
    ff.txPushButton->setText("Transmit");
    ff.txPushButton->setStyleSheet("");
    ff.txPushButton->setFont(txFont);
    ff.frequencySpinBox->setEnabled(true);
    ff.durationSpinBox->setEnabled(true);
  };
  auto saveValues=[&]()
  {
    QSettings qs;
    qs.beginGroup("TX");
    qs.setValue("toneFreq",ff.frequencySpinBox->value());
    qs.setValue("toneDuration",ff.durationSpinBox->value());
    qs.endGroup();
  };
  auto stopTone=[&]()
  {
    if(transmitting)
      {
        showIdle();
        slotStop();
      }
  };

  // slotStop() runs on any TX end (natural end of tone or manual stop)
  QMetaObject::Connection endConn=connect(this,&txWidget::txStopped,&qd,showIdle);
  connect(&toneTimer,&QTimer::timeout,&qd,stopTone);
  connect(ff.txPushButton,&QPushButton::clicked,&qd,[&]()
  {
    if(transmitting)
      {
        stopTone();
        return;
      }
    int freq,duration;
    getValue(freq,ff.frequencySpinBox);
    getValue(duration,ff.durationSpinBox);
    saveValues();
    txFunctionsPtr->setToneParam((double)duration,(double)freq);
    if(dispatcherPtr->startTX(txFunctions::TXSENDTONE))
      {
        transmitting=true;
        ff.txPushButton->setText("STOP");
        ff.txPushButton->setStyleSheet("background-color: red; color: white");
        ff.txPushButton->setFont(txFont);
        ff.frequencySpinBox->setEnabled(false);
        ff.durationSpinBox->setEnabled(false);
        toneTimer.start((duration+3)*1000); // safety net if the end event never arrives
      }
  });

  qd.exec(); // Close button accepts, Esc rejects
  stopTone();
  saveValues();
  disconnect(endConn);
}

void txWidget::slotSweepSignal()
{
  QDialog qd;
  Ui::sweepForm ff;
  ff.setupUi(&qd);

  QSettings qSettings;
  qSettings.beginGroup("TX");
  setValue(qSettings.value("sweepLowerFreq",ff.lowerFrequencySpinBox->value()).toInt(),ff.lowerFrequencySpinBox);
  setValue(qSettings.value("sweepUpperFreq",ff.upperFrequencySpinBox->value()).toInt(),ff.upperFrequencySpinBox);
  setValue(qSettings.value("sweepDuration",ff.durationSpinBox->value()).toInt(),ff.durationSpinBox);
  qSettings.endGroup();

  int upperFreq;
  int lowerFreq;
  int  duration;
  if(qd.exec())
    {
      getValue(lowerFreq,ff.lowerFrequencySpinBox);
      getValue(upperFreq,ff.upperFrequencySpinBox);
      getValue(duration,ff.durationSpinBox);
      qSettings.beginGroup("TX");
      qSettings.setValue("sweepLowerFreq",lowerFreq);
      qSettings.setValue("sweepUpperFreq",upperFreq);
      qSettings.setValue("sweepDuration",duration);
      qSettings.endGroup();
      txFunctionsPtr->setToneParam((double)duration,(double)lowerFreq,(double)upperFreq);
      dispatcherPtr->startTX(txFunctions::TXSENDTONE);
    }
}



void txWidget::slotGenerateRepeaterTone()
{
  //  addToLog(QString("start of buffer %1").arg(soundIOPtr->txBuffer.count()),LOGTXMAIN);
  txFunctionsPtr->setToneParam(3.,1750);
  dispatcherPtr->startTX(txFunctions::TXSENDTONE);
  addToLog("sendTone",LOGTXMAIN);
}




void txWidget::slotEdit()
{
  if(txFunctionsPtr->txBusy())
    {
      QMessageBox::warning(this,"Editor","Transmission busy, editor not available");
      return;
    }
  imageViewerPtr->slotEdit();   // same path as the image's right-click Edit
}



void txWidget::applyTemplate()
{
  if(repeaterEnabled)
    {
      if(repeaterIdleImage)
        {
          txFunctionsPtr->applyTemplate(imageViewerPtr,repeaterIdleTemplate);
        }
      else
        {
          txFunctionsPtr->applyTemplate(imageViewerPtr,repeaterTemplate);
        }
    }
  else
    {
      txFunctionsPtr->applyTemplate(imageViewerPtr,galleryWidgetPtr->getTemplateFileName(ui->templatesComboBox->currentIndex()));
      ui->uploadToolButton->setEnabled(useHybrid && (transmissionModeIndex==TRXDRM));
    }

  updateTxTime();

}

void txWidget::updateTxTime()
{
  if(transmissionModeIndex==TRXKGSTV){
    ui->sizeKbLabel->clear();
    const int seconds=kgstvControllerPtr?int(std::ceil(kgstvControllerPtr->duration())):0;
    ui->sizeDurationLabel->setText(seconds>0?tr("%1 s").arg(seconds):QString("-"));
    ui->sizeDurationLabel->setToolTip(tr("Duration of the prepared KG-STV packets."));return;
  }
  int sizeKb,seconds,minutes;
  if(!imageViewerPtr->hasValidImage()) return;
  seconds=txFunctionsPtr->calcTxTime(false,0);
  minutes=seconds/60;
  seconds=seconds-minutes*60;
  if(minutes==0)
    {
      ui->sizeDurationLabel->setText(QString("%1 s").arg(seconds));
    }
  else
    {
      ui->sizeDurationLabel->setText(QString("%1 min %2 s").arg(minutes).arg(seconds));
    }
  sizeKb=round((double)imageViewerPtr->getFileSize()/1000.);
  ui->sizeKbLabel->setText( QString::number(sizeKb)+"KB");
}


void txWidget::setImages(const QStringList &files)
{
  addToLog(QString("setImages %1 files").arg(files.count()),LOGTXMAIN);
  imageViewerPtr->loadSegments(files);
}

void txWidget::setImageToSegment(QString fn,int seg)
{
  addToLog(QString("setImageToSegment %1 %2").arg(fn).arg(seg),LOGTXMAIN);
  if(seg<0) seg=imageViewerPtr->firstEmptySegment();
  imageViewerPtr->loadSegment(fn,qMax(0,seg));
}

int txWidget::txGridSegments() const
{
  return imageViewerPtr->gridColumns()*imageViewerPtr->gridRowCount();
}

void 	txWidget::setImage(QString fn)
{
  addToLog(QString("setImage %1").arg(fn),LOGTXMAIN);
  imageViewerPtr->openImage(fn,true,true,false,true);
  addToLog(QString("setImage %1 done").arg(fn),LOGTXMAIN);

}


void txWidget::setProgress(uint prg)
{
  ui->txProgressBar->setValue(prg);
  emit progressChanged((int)prg);
}

void txWidget::slotModeChanged(int m)
{

  addToLog("slotModeChange",LOGTXMAIN);
  if(transmissionModeIndex==TRXSSTV)
    {
      sstvModeIndexTx=txModeList[m];
      updateGridControl();
      applyTemplate();
    }
}

// SSTV: stitched grids only make sense for modes larger than 512x400 (the size of PD160); DRM has no mode size
// A single DRM image is always zoom/pan, so it has no Stretch/Crop/Fit choice
void txWidget::updateGridControl()
{
  bool allowed=transmissionModeIndex!=TRXKGSTV && ((transmissionModeIndex==TRXDRM) ||
      (SSTVTable[(int)sstvModeIndexTx].numberOfPixels>512 || SSTVTable[(int)sstvModeIndexTx].numberOfDisplayLines>400));
  ui->sstvGridComboBox->setEnabled(allowed);
  ui->drmGridComboBox->setEnabled(allowed);
  int idx=allowed ? ui->sstvGridComboBox->currentIndex() : 0;
  ui->drmGridComboBox->setCurrentIndex(idx);
  static const int cols[4]={1,1,2,2};
  static const int rows[4]={1,2,1,2};
  imageViewerPtr->setGrid(cols[idx],rows[idx]);
  ui->drmResizeComboBox->setEnabled(!(transmissionModeIndex==TRXDRM && idx==0));
}

void txWidget::slotGridChanged(int i)
{
  gridLayout=i;
  ui->sstvGridComboBox->setCurrentIndex(i);
  ui->drmGridComboBox->setCurrentIndex(i);
  updateGridControl();
  applyTemplate();
}

// Repopulates sstvModeComboBox from scratch with every mode and rebuilds
// txModeList (index -> esstvMode) in step; index<->mode conversions in
// this file go through txModeList instead of a direct cast.
void txWidget::rebuildModeComboBox()
{
  ui->sstvModeComboBox->blockSignals(true);
  ui->sstvModeComboBox->clear();
  txModeList.clear();
  for(int i=0;i<NUMSSTVMODES;i++)
    {
      esstvMode m=(esstvMode)i;
      ui->sstvModeComboBox->addItem(getSSTVModeNameLong(m));
      txModeList.append(m);
    }
  ui->sstvModeComboBox->blockSignals(false);
}

void txWidget::slotResizeChanged(int i)
{
  // Stretch / Crop / Fit / Free applies to the selected image or grid cell
  ui->sstvResizeComboBox->setCurrentIndex(i);
  ui->drmResizeComboBox->setCurrentIndex(i);
  imageViewerPtr->setFitMode(i);
  applyTemplate();
}





void txWidget::slotSnapshot()
{
  // Qt Multimedia camera capture (Phase 5 of the cross-platform plan) works on every OS,
  // so this is no longer __APPLE__-gated -- it also finally brings webcam capture to
  // macOS, which the old V4L2-only backend never supported.
  if(beaconRunning)
    {
      stopBeacon();   // the (green) beacon icon stops the beacon; the dialog is opened with the next click
      return;
    }
  QImage *im;
  cameraDialog camera;
  if(camera.exec()==QDialog::Accepted)
    {
      im=camera.getImage();
      if(!im)
        {
          QMessageBox::warning(this,"Snapshot","No usable frame was received from the camera.");
          return;
        }
      if(camera.beaconEnabled())
        {
          // the dialog's live frame is the first beacon picture: no need to reopen the camera yet
          QString bfn=QDir::tempPath()+"/qsstv_beacon.jpg";
          if(!im->save(bfn,"JPG",95))
            {
              QMessageBox::warning(this,"Beacon","Unable to save the camera image.");
              return;
            }
          setImage(bfn);
          startBeacon(camera.beaconMinutes());
          return;
        }
      QTemporaryFile itmp(txStockImagesPath+"/snapshotXXXXXX.jpg");
      itmp.setAutoRemove(false);
      if(!itmp.open() || !im->save(itmp.fileName()))
        {
          QMessageBox::warning(this,"Snapshot","Unable to save the camera image.");
          return;
        }
      setImage(itmp.fileName());
      galleryWidgetPtr->txStockImageChanged();
    }
}



void txWidget::setSettingsTab()
{
  {
    if(transmissionModeIndex==TRXDRM)
      {
        ui->hybridCheckBox->setEnabled(true);
        ui->uploadToolButton->setEnabled(useHybrid && (transmissionModeIndex==TRXDRM));
        ui->binaryToolButton->setEnabled(true);
        ui->sizeLabel->setEnabled(true);
        ui->sizeSlider->setEnabled(true);
        ui->sizeKbLabel->setEnabled(true);
        ui->sizeDurationLabel->setEnabled(true);
        mainWindowPtr->setSSTVDRMPushButton(true);
      }
    else
      {
        ui->hybridCheckBox->setEnabled(false);
        ui->uploadToolButton->setEnabled(false);
        ui->binaryToolButton->setEnabled(false);
        ui->sizeLabel->setEnabled(false);
        ui->sizeSlider->setEnabled(false);
        ui->sizeKbLabel->setEnabled(false);
        ui->sizeDurationLabel->setEnabled(false);
        ui->sizeDurationLabel->setText("-");
        if(mainWindowPtr)mainWindowPtr->setSSTVDRMPushButton(false);
      }
  }
  applyTemplate();
}


void txWidget::slotSize(int fsize)
{
  QString t;
  if((sizeChanged) || (compressedSize!=(uint)fsize))
    {
      sizeChanged=true;
      compressedSize=fsize;
      if(ui->sizeSlider->isSliderDown())
        {
          // live update while dragging; coalesce the expensive re-encode
          if(!sizeApplyTimer.isActive()) sizeApplyTimer.start();
        }
      else slotSizeApply();
    }
}

void txWidget::slotCompressionReady()
{
  imageViewerPtr->displayImage();
  updateTxTime();
}

void txWidget::slotSizeReleased()
{
  sizeApplyTimer.stop();
  if(sizeChanged) slotSizeApply();
}

void txWidget::slotSizeApply()
{
  if(txFunctionsPtr->txBusy()) return;   // never re-encode the picture while it is being transmitted

  bool dragging=ui->sizeSlider->isSliderDown();
  if(!dragging) QApplication::setOverrideCursor(Qt::WaitCursor);
  sizeChanged=false;
  imageViewerPtr->setAsyncEncode(true);   // AVIF searches run in the background; compressionReady() shows the result
  imageViewerPtr->setSize(compressedSize,transmissionModeIndex==TRXDRM);
  imageViewerPtr->setAsyncEncode(false);
  if(!imageViewerPtr->encodePending())
    {
      imageViewerPtr->displayImage();
      updateTxTime();
    }
  if(!dragging) QApplication::restoreOverrideCursor();
}

void txWidget::setModeLock(int busy)
{
  int i;
  for(i=0;i<TRXNOMODE;i++)
    {
      ui->settingsTableWidget->setTabEnabled(i,(busy<0)||(i==busy));
    }
}

void txWidget::slotTransmissionMode(int rxtxMode)
{
  emit modeSwitch(rxtxMode);
}

void txWidget::changeTransmissionMode(int rxtxMode)
{
  ++kgstvTicket;
  int i;
  transmissionModeIndex=(etransmissionMode)rxtxMode;
  enableButtons(true);
  if((transmissionModeIndex>=0)&&(transmissionModeIndex<TRXNOMODE))
    {
      for(i=0;i<TRXNOMODE;i++)
        {
          if(i!=transmissionModeIndex) ui->settingsTableWidget->widget(i)->setEnabled(false);
        }
      ui->settingsTableWidget->widget(transmissionModeIndex)->setEnabled(true);
      ui->settingsTableWidget->blockSignals(true);
      ui->settingsTableWidget->setCurrentIndex(transmissionModeIndex);
      ui->settingsTableWidget->blockSignals(false);

    }
  setSettingsTab();
  ui->txNotificationList->setVisible(transmissionModeIndex==TRXDRM && !lowRes);   // hybrid notifications are DRM-only
  updateGridControl();   // grid availability and canvas size depend on the mode
  static int lastMode=-1;
  bool modeChanged=(lastMode>=0 && lastMode!=transmissionModeIndex);
  lastMode=transmissionModeIndex;
  if(transmissionModeIndex==TRXDRM)
    {
      slotSizeApply();   // re-renders through setSize()
    }
  else if(modeChanged)
    {
      applyTemplate();   // re-render at the SSTV canvas size
    }
}

void txWidget::slotEncodingChanged(int enc)
{
  imageViewerPtr->setEncoding(enc);
  slotImageChanged();   // size range, re-encode and TX time follow the codec
}

void txWidget::slotProfileChanged(int i)
{
  drmProfileConfigPtr->getDRMParams(i,drmParams);
  setParams();
}

void txWidget::reloadProfiles()
{
  int index;
  index= ui->drmProfileComboBox->currentIndex();
  setProfileNames();
  slotProfileChanged(index);
}

void txWidget::setProfileNames()
{
  QString tmp;
  ui->drmProfileComboBox->clear();
  if(drmProfileConfigPtr->getName(0,tmp))
    {
      ui->drmProfileComboBox->addItem(tmp);
    }
  if(drmProfileConfigPtr->getName(1,tmp))
    {
      ui->drmProfileComboBox->addItem(tmp);
    }
  if(drmProfileConfigPtr->getName(2,tmp))
    {
      ui->drmProfileComboBox->addItem(tmp);
    }
}

void txWidget::slotImageChanged()
{
  uint temp;
  if(transmissionModeIndex==TRXDRM)
    {
      if(imageViewerPtr->encoding()==imageViewer::ENCAVIF)
        {
          // a near-lossless AVIF encode just to find the slider range would be slow; use the raw RGB size instead
          maxSize=imageViewerPtr->diplayedImageBytecount()*3/4;
        }
      else
        {
          maxSize=imageViewerPtr->setSize(imageViewerPtr->diplayedImageBytecount(),transmissionModeIndex==TRXDRM);
        }
      if(maxSize>MAXDRMSIZE) maxSize=MAXDRMSIZE;
      ui->sizeSlider->setMaximum(maxSize);
      ui->sizeSlider->setMinimum(MINDRMSIZE);
      if(compressedSize>MAXDRMSIZE) compressedSize=MAXDRMSIZE;
      if(compressedSize<MINDRMSIZE) compressedSize=MINDRMSIZE;
      temp=compressedSize;
      compressedSize=0;
      slotSize(temp);
    }
  else
    {
      slotSizeApply();
    }
  ui->uploadToolButton->setEnabled(useHybrid && (transmissionModeIndex==TRXDRM));
  updateTxTime();
}

void txWidget::slotBinary()
{
  if(mainWindowPtr->busyMode()>=0) return;
  slotGetParams();
  doTx=TXBINARY;
  dispatcherPtr->startDRMTxBinary();
}

void txWidget::slotHybridToggled()
{
  getValue(useHybrid,ui->hybridCheckBox);
  ui->uploadToolButton->setEnabled(useHybrid && (transmissionModeIndex==TRXDRM));
  updateTxTime();
}


void txWidget::txTestPattern(etpSelect sel)
{
  // the pattern is a single full-size image: drop any stitched grid first
  if(gridLayout!=0 || ui->sstvGridComboBox->currentIndex()!=0)
    {
      gridLayout=0;
      ui->sstvGridComboBox->setCurrentIndex(0);
      ui->drmGridComboBox->setCurrentIndex(0);
      updateGridControl();
    }
  txFunctionsPtr->txTestPattern(imageViewerPtr,sel);
  if(transmissionModeIndex==TRXSSTV) applyTemplate();   // scale to the mode like any other image
}

void txWidget::startNotifyCheck(QString fn)
{
  mask="Dummy"+fn+"+++*";
  numberOfNotifyChecks=NUMBEROFNOTIFYCHECKS;
  notifyTimer.start();
  notifyBusy=false;
}

void txWidget::slotNotifyTimeout()
{
  if(notifyBusy)
    {
      numberOfNotifyChecks--;
      if(numberOfNotifyChecks>=0)
        {
          notifyTimer.start();
        }
      return;
    }
  notifyBusy=true;
  ff.setupFtp("Notify check", hybridFtpRemoteHost,hybridFtpPort,hybridFtpLogin,hybridFtpPassword,hybridFtpRemoteDirectory+"/"+hybridNotifyDir);
  ff.listFiles(mask,true);
}

void txWidget::slotListingDone(bool err)
{
  int i;
  QString info;
  QString tmp;
  QList <QUrlInfo> notifications;
  if(err) return;
  notifications=ff.getListing();

  for(i=0;i<notifications.count();i++)
    {
      tmp=notifications.at(i).name();
      tmp.replace("Dummyde_","");
      tmp.replace("+++."," ");
      info += tmp;
    }
  ui->txNotificationList->setPlainText(info);
  numberOfNotifyChecks--;
  if(numberOfNotifyChecks>=0)
    {
      notifyTimer.start();
    }
  notifyBusy=false;
}

void txWidget::sendRepeaterImage(esstvMode rxMode)
{
  if(transmissionModeIndex==TRXKGSTV)return;
  useHybrid=0;
  setValue(useHybrid,ui->hybridCheckBox);
  rxWidgetPtr->getImageViewerPtr()->slotToTX();
  repeaterTxDelayTimer.start(repeaterTxDelay*1000);

  if(transmissionModeIndex==TRXSSTV)
    {
      if(repeaterTxMode==0) txMode=rxMode; // 0 index = Same as RX
      else txMode=(esstvMode)(repeaterTxMode-1);
      slotModeChanged(txMode);
    }
}


void txWidget::slotRepaterDelay()
{
  if(transmissionModeIndex==TRXKGSTV)return;
  addToLog("repeater send image",LOGTXMAIN);
//  qDebug() << "starting repeater tx";
  repeaterIdleImage=false;
  repeaterTimer->start(60000*repeaterImageInterval);  //retrigger repeater timer for idle images
  //
  // imPtr points to image to send
  doTx=TXNORMAL;
  slotSize(repeaterImageSize*1000);
  prepareTx();
}





void txWidget::slotRepeaterTimer()
{
  if(transmissionModeIndex==TRXKGSTV)return;
  QString fn;
  QFile fi;
  repeaterTimer->start(60000*repeaterImageInterval); //retrigger repeater timer
  if( txFunctionsPtr->txBusy()
      || rxWidgetPtr->rxBusy()
      || !repeaterEnabled )
    {
      return;
    }

  switch(repeaterIndex)
    {
    case 0:
      fn=repeaterImage1;
      break;
    case 1:
      fn=repeaterImage2;
      break;
    case 2:
      fn=repeaterImage3;
      break;
    case 3:
      fn=repeaterImage4;
      break;
    default:
      fn=repeaterImage1;
      break;
    }
  fi.setFileName(fn);
  if (fi.exists())
    {
      slotModeChanged(repeaterIdleTxMode);
      setImage(fn);
      repeaterIdleImage=true;
      doTx=TXNORMAL;
      addToLog("repeater send idle image",LOGTXMAIN);
      slotSize(repeaterImageSize*1000);
      prepareTx();
    }
  repeaterIndex++;
  if(repeaterIndex>3) repeaterIndex=0;
}














void txWidget::beaconSkipped(const QString &reason)
{
  QString msg=QString("WA8LMF Beacon Mode not sent %1: %2").arg(QTime::currentTime().toString("HH:mm")).arg(reason);
  addToLog(msg,LOGTXMAIN);
  beaconError=msg;
  if(beaconRunning)
    {
      beaconTimer.start(30000);
      slotBeaconTick();
    }
}

void txWidget::beaconRestartInterval()
{
  beaconTimer.start(60000*beaconMinutes);
}

void txWidget::updateBeaconUi()
{
  QIcon icon;
  if(beaconRunning)
    {
      // the beacon icon with a green tint overlay
      QPixmap pm(":/icons/tb_beacon.png");
      QPainter painter(&pm);
      painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
      painter.fillRect(pm.rect(),QColor(0,200,0,130));
      painter.end();
      icon=QIcon(pm);
      ui->snapshotToolButton->setStyleSheet("QToolButton { background-color: rgba(0,200,0,70); border: 1px solid rgb(0,140,0); border-radius: 4px; }");
      ui->snapshotToolButton->setToolTip(QString("WA8LMF Beacon Mode every %1 min - click to stop").arg(beaconMinutes));
    }
  else
    {
      icon=QIcon(":/icons/tb_camera.png");
      ui->snapshotToolButton->setStyleSheet(QString());
      ui->snapshotToolButton->setToolTip("Camera picture or WA8LMF Beacon Mode");
    }
  ui->snapshotToolButton->setIcon(icon);
}

void txWidget::startBeacon(int minutes)
{
  if(transmissionModeIndex==TRXKGSTV)return;
  beaconMinutes=minutes;
  beaconRunning=true;
  beaconPending=false;
  beaconWaitPosition=false;
  beaconError.clear();
  updateBeaconUi();
  if(beaconUsesGps()) requestGps(false);   // the first picture is sent after 10 s, time enough for the position
  // the first picture is already loaded (slotSnapshot); it is sent after 10 s, so the transmit settings can still be adjusted
  beaconHold=true;
  beaconTimer.start(10000);
  beaconTick.start();
  slotBeaconTick();
}

void txWidget::stopBeacon()
{
  beaconRunning=false;
  beaconPending=false;
  beaconHold=false;
  beaconWaitPosition=false;
  beaconTimer.stop();
  beaconTick.stop();
  beaconCap.abort();
  beaconError.clear();
  updateBeaconUi();
  emit beaconCountdownChanged(QString(),false);
}

void txWidget::slotBeaconTick()
{
  if(!beaconRunning) return;
  if(!beaconError.isEmpty())
    {
      emit beaconCountdownChanged(beaconError,true);
      return;
    }
  QString txt;
  if(beaconTimer.isActive())
    {
      int sec=(beaconTimer.remainingTime()+999)/1000;
      txt=QString("WA8LMF Beacon Mode: next in %1:%2").arg(sec/60).arg(sec%60,2,10,QChar('0'));
    }
  else if(beaconCap.isBusy()) txt="WA8LMF Beacon Mode: grabbing camera image";
  else if(beaconWaitPosition) txt=placeAwaiting?"WA8LMF Beacon Mode: looking up the place":"WA8LMF Beacon Mode: reading the GPS";
  else txt="WA8LMF Beacon Mode: preparing transmission";
  emit beaconCountdownChanged(txt,false);
}

void txWidget::slotBeaconTimer()
{
  if(!beaconRunning) return;
  if(beaconCap.isBusy() || beaconPending) return;
  if(beaconHold)
    {
      beaconHold=false;
      beaconTransmitLoaded();
      return;
    }
  if(rxWidgetPtr->rxBusy())
    {
      beaconSkipped("a picture is being received");
      return;
    }
  if(txFunctionsPtr->txBusy() || mainWindowPtr->busyMode()>=0)
    {
      beaconSkipped("the transmitter is busy");
      return;
    }
  if(calTxActive)
    {
      beaconSkipped("a calibration transmission is running");
      return;
    }
  if(transmissionModeIndex==TRXDRM && transmissionModeIndex!=TRXDRM)
    {
      beaconSkipped("no SSTV or DRM transmit mode is selected");
      return;
    }
  if(beaconUsesGps()) requestGps(false);   // reads the position while the camera warms up
  beaconCap.start();
}

void txWidget::slotBeaconFailed(const QString &reason)
{
  beaconSkipped(reason);
}

void txWidget::slotBeaconFrame(const QImage &image)
{
  if(!beaconRunning) return;
  QString fn=QDir::tempPath()+"/qsstv_beacon.jpg";
  if(!image.save(fn,"JPG",95))
    {
      beaconSkipped("the camera picture could not be saved");
      return;
    }
  setImage(fn);
  if(!imageViewerPtr->hasValidImage())
    {
      beaconSkipped("the camera picture could not be loaded");
      return;
    }
  beaconTransmitLoaded();
}

// sends the picture in the TX image viewer with the current Transmit settings
void txWidget::beaconTransmitLoaded()
{
  if(!beaconRunning) return;
  if(gpsAwaiting || placeAwaiting)
    {
      beaconWaitPosition=true;   // the position is still coming: slotGpsFix() or slotPlaceReady() comes back here, the timeouts of the GPS and the lookup bound the wait
      return;
    }
  // the capture takes a few seconds: check that nothing started meanwhile
  if(rxWidgetPtr->rxBusy())
    {
      beaconSkipped("a picture is being received");
      return;
    }
  if(txFunctionsPtr->txBusy() || mainWindowPtr->busyMode()>=0)
    {
      beaconSkipped("the transmitter is busy");
      return;
    }
  if(!imageViewerPtr->hasValidImage())
    {
      beaconSkipped("no valid picture to transmit");
      return;
    }
  beaconRestartInterval();
  beaconPending=true;
  doTx=TXNORMAL;
  prepareTx();
}

// a GPS position is read for every beacon transmission when the GPS is set up and the option is on (Configuration > GPS)
bool txWidget::beaconUsesGps() const
{
  return gpsEnabled && gpsBeaconRefresh;
}

bool txWidget::requestGps(bool manual)
{
  if(gpsAwaiting)
    {
      if(manual) gpsManual=true;   // a beacon read is already under way: the button joins it
      return true;
    }
  if(!gpsReaderPtr->requestFix()) return false;
  gpsAwaiting=true;
  gpsManual=manual;
  ui->gpsToolButton->setEnabled(false);
  return true;
}

void txWidget::slotGetGps()
{
  if(!gpsEnabled)
    {
      QMessageBox::information(this,"GPS","The GPS is not enabled.\nSet it up in Options > Configuration > GPS.");
      return;
    }
  if(!requestGps(true)) QMessageBox::information(this,"GPS","The GPS is busy, try again in a moment.");
}

void txWidget::slotGpsFix(bool ok,double lat,double lon,const QString &reason)
{
  if(!gpsAwaiting) return;   // somebody else's result, such as the Test button of Configuration > GPS
  gpsAwaiting=false;
  const bool manual=gpsManual;
  gpsManual=false;
  ui->gpsToolButton->setEnabled(true);
  if(ok)
    {
      setValue(formatCoordinate(lat,true,gpsCoordFormat),ui->latLineEdit);
      setValue(formatCoordinate(lon,false,gpsCoordFormat),ui->lonLineEdit);
      slotGetParams();   // takes the values over and renders the template again
      requestPlace(manual);
    }
  else
    {
      addToLog(QString("GPS: %1, using the last position").arg(reason),LOGTXMAIN);
    }
  if(beaconWaitPosition && !placeAwaiting)
    {
      beaconWaitPosition=false;
      beaconTransmitLoaded();   // a missing position never stops the beacon
    }
  if(!ok && manual) QMessageBox::warning(this,"GPS",reason);
}

bool txWidget::requestPlace(bool manual)
{
  if(!gpsPlaceLookup || placeAwaiting) return false;
  double lat,lon;   // whatever the notation, as long as it is one (range included)
  if(!parseCoordinateText(imageViewerPtr->latitude,true,lat) || !parseCoordinateText(imageViewerPtr->longitude,false,lon)) return false;
  if(!placeLookupPtr->requestPlace(lat,lon)) return false;
  placeKey=imageViewerPtr->latitude+"|"+imageViewerPtr->longitude;
  placeAwaiting=true;
  placeManual=manual;
  return true;
}

void txWidget::slotPlaceReady(bool ok,const QString &place,const QString &reason)
{
  if(!placeAwaiting) return;   // somebody else's result, such as the Test button of Configuration > GPS
  placeAwaiting=false;
  const bool manual=placeManual;
  placeManual=false;
  if(ok)
    {
      setValue(place,ui->placeLineEdit);
      slotGetParams();   // takes the place over and renders the template again
    }
  else
    {
      placeKey.clear();   // the next time the position is left or read, the lookup is tried again
      addToLog(QString("Place lookup: %1, keeping the last place").arg(reason),LOGTXMAIN);
    }
  if(beaconWaitPosition && !gpsAwaiting)
    {
      beaconWaitPosition=false;
      beaconTransmitLoaded();   // a missing place never stops the beacon
    }
  if(!ok && manual) QMessageBox::warning(this,"Place",reason);
}

// Writes both boxes in the given format. Text that is no coordinate (free text such as 47°N or a name) stays as it is.
void txWidget::convertPosition(int format)
{
  double degrees;
  if(parseCoordinateText(ui->latLineEdit->text(),true,degrees)) setValue(formatCoordinate(degrees,true,format),ui->latLineEdit);
  if(parseCoordinateText(ui->lonLineEdit->text(),false,degrees)) setValue(formatCoordinate(degrees,false,format),ui->lonLineEdit);
}

// Another position format in Configuration > GPS: the boxes follow. The place stays, it belongs to the same position.
void txWidget::positionFormatChanged()
{
  if(gpsCoordFormat==positionFormat) return;
  positionFormat=gpsCoordFormat;
  const bool sameKey=(ui->latLineEdit->text()+"|"+ui->lonLineEdit->text()==placeKey);
  convertPosition(positionFormat);
  slotGetParams();   // takes the values over and renders the template again
  if(sameKey) placeKey=imageViewerPtr->latitude+"|"+imageViewerPtr->longitude;
}

// typed-in coordinates: the place follows them, but only when they changed (a box that is just left must not replace a place typed in)
void txWidget::slotPositionEdited()
{
  // in decimal degrees the text is printed as typed; in degrees and minutes whatever was typed or pasted is converted
  if(gpsCoordFormat==COORDDEGMIN) convertPosition(COORDDEGMIN);
  slotGetParams();
  if(imageViewerPtr->latitude+"|"+imageViewerPtr->longitude==placeKey) return;
  requestPlace(false);
}

void txWidget::kgstvAction(int action)
{
  if(transmissionModeIndex!=TRXKGSTV || !kgstvControllerPtr)return;
  if(txFunctionsPtr->txBusy() || (mainWindowPtr && mainWindowPtr->busyMode()>=0)){QMessageBox::warning(this,tr("KG-STV"),tr("Transmission is busy."));return;}
  QString error;bool ok=false;
  if(action==0)ok=kgstvControllerPtr->prepareTextJob(kgstvText->toPlainText(),myCallsign,&error);
  else if(action==1)ok=kgstvControllerPtr->prepareRequestJob(myCallsign,&error);
  else ok=kgstvControllerPtr->prepareRepairJob(myCallsign,&error);
  if(!ok){QMessageBox::warning(this,tr("KG-STV"),error);return;}
  updateTxTime();enableButtons(false);if(!dispatcherPtr->startTX(txFunctions::TXSENDKGSTV))slotStop();
}
