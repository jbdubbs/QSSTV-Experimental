#ifndef GPSREADER_H
#define GPSREADER_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QTimer>
#include <QElapsedTimer>

class QSerialPort;

/*!
  One-shot position request to the GPS receiver on the serial port set in Configuration > GPS.
  The port is opened for the request and closed again before the result is reported, so it is
  not held while nothing needs a position. Everything runs on the GUI thread (readyRead and a
  timer), so a request never blocks.

  Tracking (the GPS button on the Transmit tab): startTracking() opens the port and keeps it open,
  and every position the receiver sends is reported as positionUpdated(), at most about once a second,
  until stopTracking(). The state of the receiver (position arriving, none right now, port gone) is
  reported as trackingState(). A port that cannot be opened or goes away is opened again every few
  seconds by itself. A one-shot request while tracking pauses it and resumes it afterwards, so the
  Test button of Configuration > GPS can try another port.

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

  //! The state of the receiver while tracking
  enum trackState
  {
    TRACKFIXED,     //!< positions arrive
    TRACKWAITING,   //!< the port is open, but no position arrives (no data, or no fix yet, or the fix is lost)
    TRACKFAILED     //!< the port cannot be opened or is gone; it is tried again every few seconds
  };
  /*!
    Start tracking with the saved settings (restarts it when it is running already, so changed settings take
    effect). Returns false, and does nothing, when the GPS is not enabled or has no port. Everything else
    (port cannot be opened, ...) is reported as trackingState().
  */
  bool startTracking();
  void stopTracking();
  bool tracking() const {return trackingOn;}
  //! How long without a position is "no position now" (default 5 s) and how long a failed port waits before it is opened again (default 5 s); the tests shorten both
  void setTrackTimeouts(int silenceMs,int retryMs) {trackSilenceMs=silenceMs; trackRetryMs=retryMs;}

signals:
  //! ok=false: reason says why (disabled, port cannot be opened, no fix in time)
  void fixReady(bool ok,double lat,double lon,const QString &reason);
  //! While tracking: a position, at most about once a second
  void positionUpdated(double lat,double lon);
  //! While tracking: the state changed (state is a trackState); reason says why when it is not TRACKFIXED
  void trackingState(int state,const QString &reason);

private slots:
  void slotReadyRead();
  void slotTimeout();
  void slotSerialError();
  void slotTrackReadyRead();
  void slotTrackError();
  void slotTrackWatchdog();
  void slotTrackRetry();

private:
  void finish(bool ok,double lat,double lon,const QString &reason);
  void openTracking();
  void closeTracking();
  void setTrackState(int state,const QString &reason);

  QSerialPort *port;
  QTimer timeout;
  QByteArray buffer;
  QString usedPort;      //!< port name of the running request, for the messages
  bool pending;
  bool sawNmea;          //!< NMEA sentences arrive, but none holds a position yet

  QSerialPort *trackPort;       //!< the port of the tracking, open while it runs (not while a one-shot request has paused it)
  QTimer trackWatchdog;         //!< no position for this long: the state becomes TRACKWAITING
  QTimer trackRetry;            //!< a port that failed is opened again when this fires
  QByteArray trackBuffer;
  QElapsedTimer lastReported;   //!< when the last positionUpdated() went out
  bool trackingOn;              //!< startTracking() was called and stopTracking() not yet
  bool trackPaused;             //!< a one-shot request has the port; the tracking goes on in finish()
  bool trackSawNmea;            //!< sentences arrive, but none holds a position (since the last position or the last time the watchdog ran)
  int trackSilenceMs=5000;
  int trackRetryMs=5000;
  int trackStateNow;            //!< last reported state, -1 before the first one
  QString trackReasonNow;
};

#endif // GPSREADER_H
