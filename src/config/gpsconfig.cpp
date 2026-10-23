#include "gpsconfig.h"
#include "ui_gpsconfig.h"
#include "appglobal.h"
#include "gpsreader.h"
#include "nmeaparser.h"
#include "rigcontrol.h"

#include <QMessageBox>
#include <QSerialPortInfo>

bool    gpsEnabled;
QString gpsPort;
int     gpsBaud;
bool    gpsBeaconRefresh;

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
  qSettings.endGroup();
}

void gpsConfig::getParams()
{
  const bool enabledCopy=gpsEnabled;
  const QString portCopy=gpsPort;
  const int baudCopy=gpsBaud;
  const bool refreshCopy=gpsBeaconRefresh;

  getValue(gpsEnabled,ui->enableCheckBox);
  gpsPort=ui->portComboBox->currentText().trimmed();
  getValue(gpsBaud,ui->baudComboBox);
  getValue(gpsBeaconRefresh,ui->beaconRefreshCheckBox);

  changed=(enabledCopy!=gpsEnabled || portCopy!=gpsPort || baudCopy!=gpsBaud || refreshCopy!=gpsBeaconRefresh);
}

void gpsConfig::setParams()
{
  setValue(gpsEnabled,ui->enableCheckBox);
  ui->portComboBox->setCurrentText(gpsPort);
  setValue(gpsBaud,ui->baudComboBox);
  setValue(gpsBeaconRefresh,ui->beaconRefreshCheckBox);
  ui->resultLabel->clear();
}

// the serial ports of the system, as Configuration > CAT would name them
void gpsConfig::slotRescanPorts()
{
  const QString current=ui->portComboBox->currentText();
  ui->portComboBox->clear();
  const QList<QSerialPortInfo> ports=QSerialPortInfo::availablePorts();
  for(const QSerialPortInfo &info:ports)
    {
#ifdef Q_OS_WIN
      const QString name=info.portName();
#else
      const QString name=info.systemLocation();
#endif
      ui->portComboBox->addItem(name);
      ui->portComboBox->setItemData(ui->portComboBox->count()-1,info.description(),Qt::ToolTipRole);
    }
  ui->portComboBox->setCurrentText(current.isEmpty()?gpsPort:current);
}

// The GPS must have a port of its own: hamlib keeps the CAT port and QSerialPort the PTT port open
QString gpsConfig::portConflict(const QString &port) const
{
  if(rigControllerPtr==nullptr || port.isEmpty()) return QString();
  const scatParams *cp=rigControllerPtr->params();
  if(cp->enableCAT && cp->serialPort.compare(port,Qt::CaseInsensitive)==0)
    return QString("The GPS port (%1) must be different from the CAT serial port").arg(port);
  if(cp->enableSerialPTT && cp->pttSerialPort.compare(port,Qt::CaseInsensitive)==0)
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

// tries the port and baud rate as they are on the page now, whether or not they have been saved
void gpsConfig::slotTest()
{
  const QString port=ui->portComboBox->currentText().trimmed();
  int baud;
  getValue(baud,ui->baudComboBox);
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
  connect(gpsReaderPtr,&gpsReader::fixReady,this,[this](bool ok,double lat,double lon,const QString &reason)
    {
      ui->testPushButton->setEnabled(true);
      if(ok) ui->resultLabel->setText(tr("Position: %1, %2").arg(formatCoordinate(lat),formatCoordinate(lon)));
      else ui->resultLabel->setText(reason);
    },Qt::SingleShotConnection);
}
