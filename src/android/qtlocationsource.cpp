#include "qtlocationsource.h"

#include <QCoreApplication>
#include <QGeoPositionInfo>
#include <QGeoPositionInfoSource>
#include <QLocationPermission>

qtLocationSource::qtLocationSource(QObject *parent) : locationSource(parent)
{
}

// the system's question (a box over the program) is asked by the first of these calls, the later ones go straight on
void qtLocationSource::withPermission(std::function<void()> go)
{
  QLocationPermission permission;
  permission.setAccuracy(QLocationPermission::Precise);
  permission.setAvailability(QLocationPermission::WhenInUse);
  auto refused=[this]
  {
    emit failed(tr("The program may not use the phone's location. Allow it in the phone's Settings (Apps, QSSTV-Experimental, "
                   "Permissions, Location, precise), or use a GPS receiver."));
  };
  switch(qApp->checkPermission(permission))
    {
    case Qt::PermissionStatus::Granted:
      go();
      break;
    case Qt::PermissionStatus::Undetermined:
      qApp->requestPermission(permission,this,[go,refused](const QPermission &answer)
        {
          if(answer.status()==Qt::PermissionStatus::Granted) go();
          else refused();
        });
      break;
    default:
      refused();
      break;
    }
}

bool qtLocationSource::ensureSource()
{
  if(source!=nullptr) return true;
  source=QGeoPositionInfoSource::createDefaultSource(this);
  if(source==nullptr)
    {
      emit failed(tr("The phone has no location service the program can use."));
      return false;
    }
  source->setPreferredPositioningMethods(QGeoPositionInfoSource::AllPositioningMethods);
  connect(source,&QGeoPositionInfoSource::positionUpdated,this,[this](const QGeoPositionInfo &info)
    {
      if(!info.isValid()) return;
      const double lat=info.coordinate().latitude(),lon=info.coordinate().longitude();
      if(onceWanted)
        {
          onceWanted=false;
          emit onceReady(lat,lon);
        }
      else emit position(lat,lon);
    });
  connect(source,&QGeoPositionInfoSource::errorOccurred,this,[this](QGeoPositionInfoSource::Error error)
    {
      switch(error)
        {
        case QGeoPositionInfoSource::AccessError:
          emit failed(tr("The phone's location service is off, or the program may not use it."));
          break;
        case QGeoPositionInfoSource::ClosedError:
          emit failed(tr("The phone's location service was closed."));
          break;
        case QGeoPositionInfoSource::UpdateTimeoutError:
          onceWanted=false;
          emit failed(tr("The phone has no position yet (is its location service on, and is the sky in view?)"));
          break;
        case QGeoPositionInfoSource::NoError:
          break;
        default:
          emit failed(tr("The phone's location service gave an error."));
          break;
        }
    });
  return true;
}

void qtLocationSource::start()
{
  withPermission([this]
    {
      if(!ensureSource()) return;
      onceWanted=false;
      source->setUpdateInterval(1000);
      source->startUpdates();
    });
}

void qtLocationSource::stop()
{
  if(source!=nullptr) source->stopUpdates();
}

void qtLocationSource::requestOnce(int timeoutMs)
{
  withPermission([this,timeoutMs]
    {
      if(!ensureSource()) return;
      onceWanted=true;
      source->requestUpdate(timeoutMs);
    });
}
