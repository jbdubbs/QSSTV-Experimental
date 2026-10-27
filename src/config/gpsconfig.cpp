#include "gpsconfig.h"
#include "ui_gpsconfig.h"
#include "appglobal.h"
#include "gpsreader.h"
#include "mainwindow.h"
#include "nmeaparser.h"
#include "placelookup.h"
#include "rigcontrol.h"
#include "serialports.h"

#include <QMessageBox>

bool    gpsEnabled;
QString gpsPort;
int     gpsBaud;
bool    gpsBeaconRefresh;
bool    gpsPlaceLookup;
int     gpsCoordFormat;

gpsConfig::gpsConfig(QWidget *parent) : baseConfig(parent), ui(new Ui::gpsConfig)
{
  ui->setupUi(this);
  connect(ui->rescanToolButton,SIGNAL(clicked()),SLOT(slotRescanPorts()));
  connect(ui->enableCheckBox,SIGNAL(clicked()),SLOT(slotCheckPort()));
  connect(ui->portComboBox,SIGNAL(activated(int)),SLOT(slotCheckPort()));   // a port chosen or typed after the box was ticked
  if(ui->portComboBox->lineEdit()!=nullptr) connect(ui->portComboBox->lineEdit(),SIGNAL(editingFinished()),SLOT(slotCheckPort()));
  connect(ui->testPushButton,SIGNAL(clicked()),SLOT(slotTest()));
  slotRescanPorts();
}

gpsConfig::~gpsConfig()
{
  delete ui;
}

void gpsConfig::readSettings()
{
  QSettings qSettings;
#ifdef Q_OS_WIN
  const QString defaultPort("COM1");
#else
  const QString defaultPort("/dev/ttyUSB0");
#endif
  qSettings.beginGroup("GPS");
  gpsEnabled=qSettings.value("enabled",false).toBool();
  gpsPort=qSettings.value("port",defaultPort).toString();
  gpsBaud=qSettings.value("baud",4800).toInt();
  gpsBeaconRefresh=qSettings.value("beaconRefresh",true).toBool();
  gpsPlaceLookup=qSettings.value("placeLookup",false).toBool();   // off: the position leaves the computer
  gpsCoordFormat=qSettings.value("coordinateFormat",COORDDECIMAL).toInt();
  if(gpsCoordFormat!=COORDDEGMIN) gpsCoordFormat=COORDDECIMAL;
  qSettings.endGroup();
  setParams();
}

void gpsConfig::writeSettings()
{
  QSettings qSettings;
  getParams();
  if(gpsEnabled)
    {
      // the CAT / PTT page has been written just before this one, so its ports are the new ones
      const QString conflict=portConflict(gpsPort);
      if(!conflict.isEmpty())
        {
          if(mainWindowPtr!=nullptr && mainWindowPtr->isQuitting())
            {
              // the settings are written for the last time: nobody can answer a message box now, and while one is open the program does not end
              addToLog(conflict+": the GPS has been disabled",LOGPARAM);
            }
          else
            {
              // the Configuration dialog has been closed by now: the box belongs to the main window, where it can be seen
              QWidget *owner=isVisible()? static_cast<QWidget *>(this):static_cast<QWidget *>(mainWindowPtr);
              QMessageBox::critical(owner,"Configuration error",conflict+"\nThe GPS has been disabled.");
            }
          gpsEnabled=false;
          setValue(false,ui->enableCheckBox);
        }
    }
  qSettings.beginGroup("GPS");
  qSettings.setValue("enabled",gpsEnabled);
  qSettings.setValue("port",gpsPort);
  qSettings.setValue("baud",gpsBaud);
  qSettings.setValue("beaconRefresh",gpsBeaconRefresh);
  qSettings.setValue("placeLookup",gpsPlaceLookup);
  qSettings.setValue("coordinateFormat",gpsCoordFormat);
  qSettings.endGroup();
}

void gpsConfig::getParams()
{
  const bool enabledCopy=gpsEnabled;
  const QString portCopy=gpsPort;
  const int baudCopy=gpsBaud;
  const bool refreshCopy=gpsBeaconRefresh;
  const bool placeCopy=gpsPlaceLookup;
  const int formatCopy=gpsCoordFormat;

  getValue(gpsEnabled,ui->enableCheckBox);
  gpsPort=ui->portComboBox->currentText().trimmed();
  getValue(gpsBaud,ui->baudComboBox);
  getValue(gpsBeaconRefresh,ui->beaconRefreshCheckBox);
  getValue(gpsPlaceLookup,ui->placeCheckBox);
  getIndex(gpsCoordFormat,ui->formatComboBox);

  changed=(enabledCopy!=gpsEnabled || portCopy!=gpsPort || baudCopy!=gpsBaud || refreshCopy!=gpsBeaconRefresh || placeCopy!=gpsPlaceLookup ||
           formatCopy!=gpsCoordFormat);
}

void gpsConfig::setParams()
{
  setValue(gpsEnabled,ui->enableCheckBox);
  ui->portComboBox->setCurrentText(gpsPort);
  setValue(gpsBaud,ui->baudComboBox);
  setValue(gpsBeaconRefresh,ui->beaconRefreshCheckBox);
  setValue(gpsPlaceLookup,ui->placeCheckBox);
  setIndex(gpsCoordFormat,ui->formatComboBox);
  ui->resultLabel->clear();
}

void gpsConfig::slotRescanPorts()
{
  fillPortComboBox(ui->portComboBox);
}

// The GPS must have a port of its own: hamlib keeps the CAT port and QSerialPort the PTT port open
QString gpsConfig::portConflict(const QString &port) const
{
  if(rigControllerPtr==nullptr || port.isEmpty()) return QString();
  const scatParams *cp=rigControllerPtr->params();
  if(cp->enableCAT && samePort(cp->serialPort,port))
    return QString("The GPS port (%1) must be different from the CAT serial port").arg(port);
  if(cp->enableSerialPTT && samePort(cp->pttSerialPort,port))
    return QString("The GPS port (%1) must be different from the PTT serial port").arg(port);
  return QString();
}

// the GPS cannot be enabled on a port that CAT or PTT uses, whether the box is ticked last or the port is chosen last
void gpsConfig::slotCheckPort()
{
  if(!ui->enableCheckBox->isChecked()) return;
  const QString conflict=portConflict(ui->portComboBox->currentText().trimmed());
  if(!conflict.isEmpty())
    {
      QMessageBox::critical(this,"Configuration error",conflict);
      ui->enableCheckBox->setChecked(false);
    }
}

// tries the port, the baud rate and the place lookup as they are on the page now, whether or not they have been saved
void gpsConfig::slotTest()
{
  const QString port=ui->portComboBox->currentText().trimmed();
  int baud;
  getValue(baud,ui->baudComboBox);
  const bool lookup=ui->placeCheckBox->isChecked();
  int format;
  getIndex(format,ui->formatComboBox);
  if(gpsReaderPtr==nullptr || port.isEmpty()) return;
  const QString conflict=portConflict(port);
  if(!conflict.isEmpty())
    {
      ui->resultLabel->setText(conflict);   // the port is not opened: it is busy, and opening it could key the PTT
      return;
    }
  ui->resultLabel->setText(tr("Reading %1 ...").arg(port));
  ui->testPushButton->setEnabled(false);
  const bool started=gpsReaderPtr->requestFix(port,baud);
  if(!started)
    {
      ui->resultLabel->setText(tr("The GPS is busy, try again in a moment"));
      ui->testPushButton->setEnabled(true);
      return;
    }
  connect(gpsReaderPtr,&gpsReader::fixReady,this,[this,lookup,format](bool ok,double lat,double lon,const QString &reason)
    {
      if(!ok)
        {
          ui->testPushButton->setEnabled(true);
          ui->resultLabel->setText(reason);
          return;
        }
      const QString position=tr("Position: %1, %2").arg(formatCoordinate(lat,true,format),formatCoordinate(lon,false,format));
      if(!lookup || placeLookupPtr==nullptr)
        {
          ui->testPushButton->setEnabled(true);
          ui->resultLabel->setText(position);
          return;
        }
      ui->resultLabel->setText(position+tr("\nLooking up the place ..."));
      if(!placeLookupPtr->requestPlace(lat,lon))
        {
          ui->testPushButton->setEnabled(true);
          ui->resultLabel->setText(position+tr("\nThe place lookup is busy, try again in a moment"));
          return;
        }
      connect(placeLookupPtr,&placeLookup::placeReady,this,[this,position](bool placeOk,const QString &place,const QString &why)
        {
          ui->testPushButton->setEnabled(true);
          ui->resultLabel->setText(position+"\n"+(placeOk?tr("Place: %1").arg(place):why));
        },Qt::SingleShotConnection);
    },Qt::SingleShotConnection);
}
