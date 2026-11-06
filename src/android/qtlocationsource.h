#ifndef QTLOCATIONSOURCE_H
#define QTLOCATIONSOURCE_H

#include "locationsource.h"
#include <functional>

class QGeoPositionInfoSource;

/*!
  The phone's own position (GPS, network) through Qt Positioning, for gpsReader::setLocationSource(): the position of the station
  for the Transmit tab and the templates, without a GPS receiver on a cable. The system asks the person for the permission
  (precise location, while the program is in use) the first time; a refusal is reported as failed(), with where to change it.
*/
class qtLocationSource : public locationSource
{
  Q_OBJECT
public:
  explicit qtLocationSource(QObject *parent=nullptr);
  void start() override;
  void stop() override;
  void requestOnce(int timeoutMs) override;

private:
  void withPermission(std::function<void()> go);
  bool ensureSource();
  QGeoPositionInfoSource *source=nullptr;
  bool onceWanted=false;
};

#endif // QTLOCATIONSOURCE_H
