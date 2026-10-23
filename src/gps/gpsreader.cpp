#include "gpsreader.h"
#include "gpsconfig.h"
#include "nmeaparser.h"

#include <QSerialPort>

gpsReader::gpsReader(QObject *parent) : QObject(parent), port(nullptr), pending(false), sawNmea(false)
{
  timeout.setSingleShot(true);
  connect(&timeout,SIGNAL(timeout()),SLOT(slotTimeout()));
}

gpsReader::~gpsReader()
{
  delete port;
}

bool gpsReader::requestFix(int timeoutMs)
{
  if(pending) return false;
  if(!gpsEnabled)
    {
      pending=true;
      QTimer::singleShot(0,this,[this] {finish(false,0,0,tr("GPS is not enabled (Configuration > GPS)"));});
      return true;
    }
  return requestFix(gpsPort,gpsBaud,timeoutMs);
}

bool gpsReader::requestFix(const QString &portName,int baud,int timeoutMs)
{
  if(pending) return false;
  pending=true;
  sawNmea=false;
  buffer.clear();
  usedPort=portName;

  port=new QSerialPort(portName,this);
  port->setBaudRate(baud);
  port->setDataBits(QSerialPort::Data8);
  port->setParity(QSerialPort::NoParity);
  port->setStopBits(QSerialPort::OneStop);
  port->setFlowControl(QSerialPort::NoFlowControl);
  if(!port->open(QIODevice::ReadOnly))
    {
      const QString reason=tr("Cannot open %1: %2").arg(portName,port->errorString());
      QTimer::singleShot(0,this,[this,reason] {finish(false,0,0,reason);});
      return true;
    }
  port->clear(QSerialPort::Input);   // only sentences that arrive from now on count as the current position
  connect(port,SIGNAL(readyRead()),SLOT(slotReadyRead()));
  connect(port,SIGNAL(errorOccurred(QSerialPort::SerialPortError)),SLOT(slotSerialError()));
  timeout.start(timeoutMs);
  return true;
}

void gpsReader::slotReadyRead()
{
  if(!pending || port==nullptr) return;
  buffer.append(port->readAll());
  int end;
  while((end=buffer.indexOf('\n'))>=0)
    {
      const QByteArray line=buffer.left(end);
      buffer.remove(0,end+1);
      if(line.trimmed().startsWith('$')) sawNmea=true;
      double lat,lon;
      if(nmeaParseFix(line,lat,lon))
        {
          finish(true,lat,lon,QString());
          return;
        }
    }
  if(buffer.size()>8192) buffer.clear();   // no line ending at all: not NMEA at this baud rate
}

void gpsReader::slotTimeout()
{
  if(!pending) return;
  if(sawNmea) finish(false,0,0,tr("The GPS receiver on %1 has no position fix yet").arg(usedPort));
  else finish(false,0,0,tr("No GPS data from %1 (check the port and the baud rate)").arg(usedPort));
}

void gpsReader::slotSerialError()
{
  if(!pending || port==nullptr) return;
  if(port->error()==QSerialPort::NoError) return;
  finish(false,0,0,tr("GPS port %1: %2").arg(usedPort,port->errorString()));
}

void gpsReader::finish(bool ok,double lat,double lon,const QString &reason)
{
  timeout.stop();
  if(port!=nullptr)
    {
      port->disconnect(this);
      port->close();
      port->deleteLater();
      port=nullptr;
    }
  pending=false;
  emit fixReady(ok,lat,lon,reason);
}
