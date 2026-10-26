#include "gpsconfig.h"
#include "ui_gpsconfig.h"
#include "appglobal.h"
#include "gpsreader.h"
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

gpsConfig::gpsConfig(QWidget *parent) : baseConfig(parent), ui(new Ui::gpsConfig)
{
  ui->setupUi(this);
  connect(ui->rescanToolButton,SIGNAL(clicked()),SLOT(slotRescanPorts()));
  connect(ui->enableCheckBox,SIGNAL(clicked()),SLOT(slotEnableChanged()));
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
          QMessageBox::critical(this,"Configuration error",conflict+"\nThe GPS has been disabled.");
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
  qSettings.endGroup();
}

void gpsConfig::getParams()
{
  const bool enabledCopy=gpsEnabled;
  const QString portCopy=gpsPort;
  const int baudCopy=gpsBaud;
  const bool refreshCopy=gpsBeaconRefresh;
  const bool placeCopy=gpsPlaceLookup;

  getValue(gpsEnabled,ui->enableCheckBox);
  gpsPort=ui->portComboBox->currentText().trimmed();
  getValue(gpsBaud,ui->baudComboBox);
  getValue(gpsBeaconRefresh,ui->beaconRefreshCheckBox);
  getValue(gpsPlaceLookup,ui->placeCheckBox);

  changed=(enabledCopy!=gpsEnabled || portCopy!=gpsPort || baudCopy!=gpsBaud || refreshCopy!=gpsBeaconRefresh || placeCopy!=gpsPlaceLookup);
}

void gpsConfig::setParams()
{
  setValue(gpsEnabled,ui->enableCheckBox);
  ui->portComboBox->setCurrentText(gpsPort);
  setValue(gpsBaud,ui->baudComboBox);
  setValue(gpsBeaconRefresh,ui->beaconRefreshCheckBox);
  setValue(gpsPlaceLookup,ui->placeCheckBox);
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

void gpsConfig::slotEnableChanged()
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
  if(gpsReaderPtr==nullptr || port.isEmpty()) return;
  ui->resultLabel->setText(tr("Reading %1 ...").arg(port));
  ui->testPushButton->setEnabled(false);
  const bool started=gpsReaderPtr->requestFix(port,baud);
  if(!started)
    {
      ui->resultLabel->setText(tr("The GPS is busy, try again in a moment"));
      ui->testPushButton->setEnabled(true);
      return;
    }
  connect(gpsReaderPtr,&gpsReader::fixReady,this,[this,lookup](bool ok,double lat,double lon,const QString &reason)
    {
      if(!ok)
        {
          ui->testPushButton->setEnabled(true);
          ui->resultLabel->setText(reason);
          return;
        }
      const QString position=tr("Position: %1, %2").arg(formatCoordinate(lat),formatCoordinate(lon));
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
