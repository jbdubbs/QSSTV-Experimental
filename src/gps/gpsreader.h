#ifndef GPSREADER_H
#define GPSREADER_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QTimer>

class QSerialPort;

/*!
  One-shot position request to the GPS receiver on the serial port set in Configuration > GPS.
  The port is opened for the request and closed again before the result is reported, so it is
  not held while nothing needs a position. Everything runs on the GUI thread (readyRead and a
  timer), so a request never blocks.

  A position source other than the serial port (the operating system's location service, for a
  station without a GPS receiver) would be chosen in requestFix().
*/
class gpsReader : public QObject
{
  Q_OBJECT
public:
  explicit gpsReader(QObject *parent=nullptr);
  ~gpsReader();
  /*!
    Ask for a position with the saved settings; the answer arrives as fixReady(). Returns false, and
    nothing will be reported for this call, while an earlier request is still running.
  */
  bool requestFix(int timeoutMs=10000);
  //! The same with the settings of this call, so the configuration page can try values that are not saved yet
  bool requestFix(const QString &portName,int baud,int timeoutMs=10000);
  bool busy() const {return pending;}

signals:
  //! ok=false: reason says why (disabled, port cannot be opened, no fix in time)
  void fixReady(bool ok,double lat,double lon,const QString &reason);

private slots:
  void slotReadyRead();
  void slotTimeout();
  void slotSerialError();

private:
  void finish(bool ok,double lat,double lon,const QString &reason);

  QSerialPort *port;
  QTimer timeout;
  QByteArray buffer;
  QString usedPort;      //!< port name of the running request, for the messages
  bool pending;
  bool sawNmea;          //!< NMEA sentences arrive, but none holds a position yet
};

#endif // GPSREADER_H
