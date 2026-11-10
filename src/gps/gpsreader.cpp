#include "gpsreader.h"
#include "gpsconfig.h"
#include "nmeaparser.h"
#include "locationsource.h"

#include <QSerialPort>

// Receivers send a position every second, in two or three sentences; one report per this long is enough for the boxes
static const int trackMinIntervalMs=800;

gpsReader::gpsReader(QObject *parent) : QObject(parent), port(nullptr), pending(false), sawNmea(false),
  trackPort(nullptr), trackingOn(false), trackPaused(false), trackSawNmea(false), trackStateNow(-1)
{
  timeout.setSingleShot(true);
  connect(&timeout,SIGNAL(timeout()),SLOT(slotTimeout()));
  trackWatchdog.setSingleShot(true);
  connect(&trackWatchdog,SIGNAL(timeout()),SLOT(slotTrackWatchdog()));
  trackRetry.setSingleShot(true);
  connect(&trackRetry,SIGNAL(timeout()),SLOT(slotTrackRetry()));
}

gpsReader::~gpsReader()
{
  delete port;
  delete trackPort;
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
  if(trackingOn && !trackPaused)
    {
      // the tracking holds the port: let go of it for this request, finish() takes it up again
      closeTracking();
      trackPaused=true;
    }
  if(loc!=nullptr)
    {
      usedPort=tr("the phone's location");
      timeout.start(timeoutMs);
      loc->requestOnce(timeoutMs);
      return true;
    }
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
  if(loc!=nullptr)
    {
      finish(false,0,0,tr("The phone has no position yet (is its location service on, and is the sky in view?)"));
      return;
    }
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
  if(trackPaused && !pending)   // a slot of fixReady may have stopped the tracking (trackPaused is false then) or asked again (pending)
    {
      trackPaused=false;
      if(trackingOn) openTracking();
    }
}

bool gpsReader::startTracking()
{
  if(!gpsEnabled || (gpsPort.isEmpty() && loc==nullptr)) return false;
  stopTracking();
  trackingOn=true;
  trackStateNow=-1;
  if(pending)
    {
      trackPaused=true;   // a one-shot request is running: finish() opens the port
      return true;
    }
  openTracking();
  return true;
}

void gpsReader::stopTracking()
{
  trackingOn=false;
  trackPaused=false;
  trackStateNow=-1;
  closeTracking();
}

void gpsReader::closeTracking()
{
  trackWatchdog.stop();
  trackRetry.stop();
  if(loc!=nullptr) loc->stop();
  if(trackPort!=nullptr)
    {
      trackPort->disconnect(this);
      trackPort->close();
      trackPort->deleteLater();
      trackPort=nullptr;
    }
  trackBuffer.clear();
}

void gpsReader::openTracking()
{
  closeTracking();
  trackSawNmea=false;
  lastReported.invalidate();   // the first position after (re)opening is reported at once
  if(loc!=nullptr)
    {
      setTrackState(TRACKWAITING,tr("Waiting for the phone's location"));
      trackWatchdog.start(trackSilenceMs);
      loc->start();
      return;
    }
  trackPort=new QSerialPort(gpsPort,this);
  trackPort->setBaudRate(gpsBaud);
  trackPort->setDataBits(QSerialPort::Data8);
  trackPort->setParity(QSerialPort::NoParity);
  trackPort->setStopBits(QSerialPort::OneStop);
  trackPort->setFlowControl(QSerialPort::NoFlowControl);
  if(!trackPort->open(QIODevice::ReadOnly))
    {
      const QString reason=tr("Cannot open %1: %2").arg(gpsPort,trackPort->errorString());
      closeTracking();
      setTrackState(TRACKFAILED,reason);
      trackRetry.start(trackRetryMs);
      return;
    }
  trackPort->clear(QSerialPort::Input);
  connect(trackPort,SIGNAL(readyRead()),SLOT(slotTrackReadyRead()));
  connect(trackPort,SIGNAL(errorOccurred(QSerialPort::SerialPortError)),SLOT(slotTrackError()));
  setTrackState(TRACKWAITING,tr("Waiting for the GPS receiver on %1").arg(gpsPort));
  trackWatchdog.start(trackSilenceMs);
}

void gpsReader::setTrackState(int state,const QString &reason)
{
  if(state==trackStateNow && reason==trackReasonNow) return;
  trackStateNow=state;
  trackReasonNow=reason;
  emit trackingState(state,reason);
}

void gpsReader::slotTrackReadyRead()
{
  if(!trackingOn || trackPort==nullptr) return;
  trackBuffer.append(trackPort->readAll());
  int end;
  while((end=trackBuffer.indexOf('\n'))>=0)
    {
      const QByteArray line=trackBuffer.left(end);
      trackBuffer.remove(0,end+1);
      if(line.trimmed().startsWith('$')) trackSawNmea=true;
      double lat,lon;
      if(!nmeaParseFix(line,lat,lon)) continue;
      trackWatchdog.start(trackSilenceMs);
      trackSawNmea=false;   // "no fix" below is told from "no data" by what arrives after this position
      setTrackState(TRACKFIXED,QString());
      if(lastReported.isValid() && lastReported.elapsed()<trackMinIntervalMs) continue;
      lastReported.start();
      emit positionUpdated(lat,lon);
      if(!trackingOn || trackPort==nullptr) return;   // a slot of positionUpdated() stopped the tracking
    }
  if(trackBuffer.size()>8192) trackBuffer.clear();   // no line ending at all: not NMEA at this baud rate
}

void gpsReader::slotTrackError()
{
  if(!trackingOn || trackPort==nullptr) return;
  if(trackPort->error()==QSerialPort::NoError) return;
  const QString reason=tr("GPS port %1: %2").arg(gpsPort,trackPort->errorString());
  closeTracking();
  setTrackState(TRACKFAILED,reason);
  trackRetry.start(trackRetryMs);
}

void gpsReader::slotTrackWatchdog()
{
  if(!trackingOn || (trackPort==nullptr && loc==nullptr)) return;
  if(loc!=nullptr)
    {
      setTrackState(TRACKWAITING,tr("The phone has no position now (is its location service on, and is the sky in view?)"));
      trackWatchdog.start(trackSilenceMs);
      return;
    }
  if(trackSawNmea) setTrackState(TRACKWAITING,tr("The GPS receiver on %1 has no position fix").arg(gpsPort));
  else setTrackState(TRACKWAITING,tr("No GPS data from %1 (check the port and the baud rate)").arg(gpsPort));
  trackSawNmea=false;   // from here on a silent receiver is told apart from one that talks without a fix
  trackWatchdog.start(trackSilenceMs);
}

void gpsReader::slotTrackRetry()
{
  if(!trackingOn || trackPaused) return;
  openTracking();
}

void gpsReader::setLocationSource(locationSource *source)
{
  if(loc!=nullptr) loc->disconnect(this);
  loc=source;
  if(loc==nullptr) return;
  connect(loc,&locationSource::position,this,&gpsReader::acceptTracked);
  connect(loc,&locationSource::onceReady,this,[this](double lat,double lon) {if(pending) finish(true,lat,lon,QString());});
  connect(loc,&locationSource::failed,this,&gpsReader::locationFailed);
}

void gpsReader::acceptTracked(double lat,double lon)
{
  if(!trackingOn || trackPaused) return;
  trackWatchdog.start(trackSilenceMs);
  setTrackState(TRACKFIXED,QString());
  if(lastReported.isValid() && lastReported.elapsed()<trackMinIntervalMs) return;
  lastReported.start();
  emit positionUpdated(lat,lon);
}

void gpsReader::locationFailed(const QString &reason)
{
  if(pending)
    {
      finish(false,0,0,reason);
      return;
    }
  if(!trackingOn || trackPaused) return;
  loc->stop();
  trackWatchdog.stop();
  setTrackState(TRACKFAILED,reason);
  trackRetry.start(trackRetryMs);   // tried again by itself, as a port that cannot be opened is
}
