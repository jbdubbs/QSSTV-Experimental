#include "placelookup.h"
#include "nmeaparser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSslSocket>
#include <QTimer>
#include <QUrlQuery>
#include <cmath>
#ifdef Q_OS_ANDROID
#include "androidhttp.h"
#endif

// Nominatim wants a User-Agent that names the application, and no more than one request a second
static const char *defaultService="https://nominatim.openstreetmap.org/reverse";
static const char *userAgent="QSSTV-Experimental (https://github.com/jbdubbs/QSSTV-Experimental)";

// A template has room for about this many characters of "City, State"; a longer place is shortened (the lookup only, text typed in is left alone)
static const int maxPlaceLength=25;
static const int minCityLength=10;   // a long state or country name must not squeeze the city out
// Generic words in front of or behind the name of a municipality ("Meridian Charter Township", "City of Westminster"), tried in this order.
// Not " City", " Town" or " Village" behind a name: Salt Lake City, Cape Town and Greenwich Village are the names.
static const char *const designatorPrefixes[]={"Charter Township of ","Township of ","Regional Municipality of ","Rural Municipality of ",
                                               "Municipality of ","City of ","Town of ","Village of ","Borough of ","County of ","Shire of ","District of "};
static const char *const designatorSuffixes[]={" Charter Township"," Township"," Municipality"," Borough"};

// the name without its designator, or the name itself when nothing else would be left
static QString withoutDesignator(const QString &name)
{
  for(const char *p:designatorPrefixes)
    {
      const QString prefix=QLatin1String(p);
      if(name.size()>prefix.size() && name.startsWith(prefix,Qt::CaseInsensitive)) return name.mid(prefix.size()).trimmed();
    }
  for(const char *s:designatorSuffixes)
    {
      const QString suffix=QLatin1String(s);
      if(name.size()>suffix.size() && name.endsWith(suffix,Qt::CaseInsensitive)) return name.left(name.size()-suffix.size()).trimmed();
    }
  return name;
}

// the name cut to at most room characters: whole words (split at spaces and hyphens) when at least half of room remains, otherwise in the middle of a word
static QString cutName(const QString &name,int room)
{
  if(name.size()<=room) return name;
  auto separator=[](QChar c) {return c==QLatin1Char(' ') || c==QLatin1Char('-');};
  int end=room;
  if(name.at(end-1).isHighSurrogate()) end--;   // not between the halves of a character
  if(!separator(name.at(end)))
    {
      int boundary=end;
      while(boundary>0 && !separator(name.at(boundary-1))) boundary--;   // the start of the word that is cut
      if(boundary-1>=room/2) end=boundary-1;
    }
  QString cut=name.left(end);
  while(!cut.isEmpty() && (separator(cut.back()) || cut.endsWith(QLatin1Char(',')))) cut.chop(1);
  return cut;
}

// "City, State" from its two parts, which may be empty
static QString joinPlace(const QString &locality,const QString &region)
{
  if(region.isEmpty() || region==locality) return locality;
  return locality.isEmpty()?region:locality+", "+region;
}

placeLookup::placeLookup(QObject *parent) : QObject(parent), reply(nullptr), serviceUrl(defaultService), pending(false),
  requestLat(0), requestLon(0), haveAnswer(false), cacheLat(0), cacheLon(0)
{
  nam=new QNetworkAccessManager(this);
}

bool placeLookup::requestPlace(double lat,double lon,int timeoutMs)
{
  if(pending) return false;
  pending=true;
  if(haveAnswer && std::fabs(lat-cacheLat)<sameSpotDegrees && std::fabs(lon-cacheLon)<sameSpotDegrees)
    {
      const QString place=cachePlace;
      QTimer::singleShot(0,this,[this,place] {finish(true,place,QString());});
      return true;
    }
#ifndef Q_OS_ANDROID   // (Android: the system's own HTTPS does the request, below)
  if(serviceUrl.scheme()==QLatin1String("https") && !QSslSocket::supportsSsl())
    {
      // a Linux build takes OpenSSL from the system, where it may be missing (Windows brings its own TLS plugin)
      QTimer::singleShot(0,this,[this] {finish(false,QString(),tr("This computer cannot make secure (https) connections (the OpenSSL libraries are probably missing)"));});
      return true;
    }
#endif
  requestLat=lat;
  requestLon=lon;
  QUrl url=serviceUrl;
  QUrlQuery query(url);
  query.addQueryItem("format","jsonv2");
  query.addQueryItem("lat",formatCoordinate(lat));
  query.addQueryItem("lon",formatCoordinate(lon));
  query.addQueryItem("zoom","10");                // city level
  query.addQueryItem("addressdetails","1");
  query.addQueryItem("accept-language","en");     // names the template font can draw
  url.setQuery(query);
#ifdef Q_OS_ANDROID
  if(url.scheme()==QLatin1String("https"))
    {
      // Qt's Android build has no TLS library to load (OpenSSL is not in the package): the system's own HTTPS, with the system's certificates
      QPointer<placeLookup> self(this);
      const qint64 token=androidHttpGet(url,userAgent,timeoutMs,[self](int status,const QByteArray &body,const QString &error)
        {
          if(self) self->slotHttpDone(status,body,error);
        });
      httpToken=token;
      // the helper's own timeouts come first; this one is the backstop, and what is answered later is dropped
      QTimer::singleShot(timeoutMs+1000,this,[this,token]
        {
          if(httpToken!=token) return;
          androidHttpCancel(token);
          httpToken=0;
          finish(false,QString(),tr("The place service did not answer in time"));
        });
      return true;
    }
#endif
  QNetworkRequest req(url);
  req.setRawHeader("User-Agent",userAgent);
  req.setRawHeader("Accept","application/json");
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setTransferTimeout(timeoutMs);
  reply=nam->get(req);
  connect(reply,&QNetworkReply::finished,this,&placeLookup::slotFinished);
  return true;
}

void placeLookup::slotFinished()
{
  QNetworkReply *r=reply;
  if(r==nullptr) return;
  reply=nullptr;
  r->deleteLater();
  if(r->error()!=QNetworkReply::NoError)
    {
      const int status=r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      QString why;
      if(status>0) why=tr("The place service answered with an error (HTTP %1)").arg(status);
      else if(r->error()==QNetworkReply::OperationCanceledError || r->error()==QNetworkReply::TimeoutError) why=tr("The place service did not answer in time");
      else why=tr("Cannot reach the place service: %1").arg(r->errorString());
      finish(false,QString(),why);
      return;
    }
  handleReply(r->readAll());
}

// the body of an answer of the service: the place in it is kept for the next position within a kilometre
void placeLookup::handleReply(const QByteArray &json)
{
  QString error;
  const QString place=placeFromReply(json,error);
  if(!place.isEmpty())
    {
      haveAnswer=true;
      cacheLat=requestLat;
      cacheLon=requestLon;
      cachePlace=place;
    }
  finish(!place.isEmpty(),place,error);
}

// Android: the answer of the system's HTTPS (status 0 and a reason when there was none)
void placeLookup::slotHttpDone(int status,const QByteArray &body,const QString &error)
{
  httpToken=0;
  if(!pending) return;
  if(!error.isEmpty())
    {
      finish(false,QString(),error==QLatin1String("timeout") ? tr("The place service did not answer in time") : tr("Cannot reach the place service: %1").arg(error));
      return;
    }
  if(status>=400)
    {
      finish(false,QString(),tr("The place service answered with an error (HTTP %1)").arg(status));
      return;
    }
  handleReply(body);
}

void placeLookup::finish(bool ok,const QString &place,const QString &reason)
{
  pending=false;
  emit placeReady(ok,place,reason);
}

QString placeLookup::placeFromReply(const QByteArray &json,QString &error)
{
  error.clear();
  QJsonParseError pe;
  const QJsonDocument doc=QJsonDocument::fromJson(json,&pe);
  if(!doc.isObject())
    {
      error=tr("Unexpected reply from the place service");
      return QString();
    }
  const QJsonObject obj=doc.object();
  const QJsonObject addr=obj.value("address").toObject();
  if(obj.contains("error") || addr.isEmpty())
    {
      error=tr("No place is known for this position");   // open sea, for one
      return QString();
    }
  auto first=[&addr](std::initializer_list<const char *> keys)
  {
    for(const char *k:keys)
      {
        const QString v=addr.value(QLatin1String(k)).toString().trimmed();
        if(!v.isEmpty()) return v;
      }
    return QString();
  };
  QString locality=first({"city","town","village","hamlet","municipality","suburb","county"});

  // "WA" rather than "Washington" where the code is what people write: the ISO 3166-2 code is "US-WA"
  QString region;
  const QString country=addr.value("country_code").toString().toUpper();
  if(country=="US" || country=="CA" || country=="AU")
    {
      const QString iso=addr.value("ISO3166-2-lvl4").toString();
      if(iso.startsWith(country+"-")) region=iso.mid(country.size()+1);
    }
  if(region.isEmpty()) region=first({"state","province","region"});
  if(region.isEmpty()) region=first({"country"});

  QString place=joinPlace(locality,region);
  if(place.size()>maxPlaceLength && !locality.isEmpty())
    {
      // too long for a template: drop "Charter Township" and the like, then cut the city, the state stays whole
      locality=withoutDesignator(locality);
      place=joinPlace(locality,region);
      if(place.size()>maxPlaceLength)
        {
          const bool both=!region.isEmpty() && region!=locality;
          const int room=both?qMax(maxPlaceLength-2-int(region.size()),minCityLength):maxPlaceLength;
          place=joinPlace(cutName(locality,room),region);
        }
    }
  if(place.isEmpty()) error=tr("No place is known for this position");
  return place;
}
