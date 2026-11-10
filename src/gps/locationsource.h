#ifndef LOCATIONSOURCE_H
#define LOCATIONSOURCE_H

#include <QObject>
#include <QString>

/*!
  A position that is not read from a serial port: the operating system's own location service (Android: src/android/
  qtlocationsource.*, Qt Positioning, with the permission the system asks for). gpsReader uses one when it has been given one
  (setLocationSource()), and does the same with it as with a receiver on a port: a position on request, or tracking with the
  state of the source reported. The tests of src/gps use a source of their own.
*/
class locationSource : public QObject
{
  Q_OBJECT
public:
  explicit locationSource(QObject *parent=nullptr) : QObject(parent) {}
  //! continuous: position() every time there is one (about once a second), failed() when there is none to be had
  virtual void start() = 0;
  virtual void stop() = 0;
  //! one position, within \a timeoutMs: onceReady(), or failed()
  virtual void requestOnce(int timeoutMs) = 0;

signals:
  void position(double lat,double lon);
  void onceReady(double lat,double lon);
  //! no position can be had: the service is off, the program may not use it, there is none in time
  void failed(const QString &reason);
};

#endif // LOCATIONSOURCE_H
