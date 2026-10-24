#ifndef PLACELOOKUP_H
#define PLACELOOKUP_H

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

/*!
  City and state of a position (the %g template macro), from the reverse geocoding service of
  OpenStreetMap (Nominatim). The position leaves the computer, so the callers ask only when the user
  switched the lookup on in Configuration > GPS.

  One request at a time, asynchronous on the GUI thread. The usage rules of the service are kept: an
  identifying User-Agent, no parallel requests, and a position that is within about a kilometre of the
  last answered one gets that answer again without a request (a station that stays put asks once).
*/
class placeLookup : public QObject
{
  Q_OBJECT
public:
  explicit placeLookup(QObject *parent=nullptr);
  /*!
    Ask for the place of a position; the answer arrives as placeReady(), always after this call
    has returned. Returns false, and nothing will be reported for this call, while an earlier
    request is still running.
  */
  bool requestPlace(double lat,double lon,int timeoutMs=8000);
  bool busy() const {return pending;}
  //! The service to ask (the default is OpenStreetMap); the tests point it at a server of their own
  void setServiceUrl(const QUrl &url) {serviceUrl=url;}
  QUrl service() const {return serviceUrl;}
  /*!
    "Seattle, WA" from the JSON reply of the service. The state is the two or three letter code in the
    United States, Canada and Australia and the name of the region elsewhere; without a state the
    country stands in. Returns an empty string, and says why in error, when the reply names no place.
  */
  static QString placeFromReply(const QByteArray &json,QString &error);

signals:
  //! ok=false: reason says why (no connection, no TLS, the service refused, no place known there)
  void placeReady(bool ok,const QString &place,const QString &reason);

private slots:
  void slotFinished();

private:
  void finish(bool ok,const QString &place,const QString &reason);

  QNetworkAccessManager *nam;
  QNetworkReply *reply;
  QUrl serviceUrl;
  bool pending;
  double requestLat,requestLon;      //!< position of the running request
  bool haveAnswer;                   //!< cacheLat / cacheLon / cachePlace hold the last good answer
  double cacheLat,cacheLon;
  QString cachePlace;
};

#endif // PLACELOOKUP_H
