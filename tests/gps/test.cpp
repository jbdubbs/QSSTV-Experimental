// Tests of src/gps: the NMEA parser, the one-shot GPS reader and the place lookup.
// The reader runs against a pseudo terminal that this test feeds, so no GPS receiver is needed; the place lookup asks a
// little HTTP server of this test instead of OpenStreetMap, so no internet is needed either.
#include "nmeaparser.h"
#include "gpsreader.h"
#include "placelookup.h"
#include "fakeservice.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

// the settings Configuration > GPS would hold (defined in config/gpsconfig.cpp in the application)
bool    gpsEnabled=true;
QString gpsPort;
int     gpsBaud=4800;
bool    gpsBeaconRefresh=true;

static int failures=0;
#define CHECK(cond) do { if(!(cond)) { std::printf("FAIL %s:%d  %s\n",__FILE__,__LINE__,#cond); failures++; } } while(0)

static bool near(double a,double b) {return std::fabs(a-b)<1e-4;}

// "$" + body + "*hh" + CRLF with the right checksum
static QByteArray nmea(const QByteArray &body)
{
  int sum=0;
  for(char c:body) sum^=static_cast<unsigned char>(c);
  return "$"+body+QByteArray("*")+QByteArray::number(sum,16).rightJustified(2,'0').toUpper()+"\r\n";
}

static void testParser()
{
  double lat=0,lon=0;

  // the classic examples
  CHECK(nmeaParseFix("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47",lat,lon));
  CHECK(near(lat,48.1173) && near(lon,11.516667));
  CHECK(nmeaParseFix("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A",lat,lon));
  CHECK(near(lat,48.1173) && near(lon,11.516667));
  CHECK(nmeaParseFix("$GPGLL,4916.45,N,12311.12,W,225444,A,*1D",lat,lon));
  CHECK(near(lat,49.274167) && near(lon,-123.185333));

  // hemispheres
  CHECK(nmeaParseFix(nmea("GPGGA,000000,3351.000,S,15112.000,E,1,05,1.0,10.0,M,0.0,M,,"),lat,lon));
  CHECK(near(lat,-33.85) && near(lon,151.2));
  CHECK(nmeaParseFix(nmea("GPGGA,000000,4736.372,N,12219.926,W,2,05,1.0,10.0,M,0.0,M,,"),lat,lon));
  CHECK(near(lat,47.606200) && near(lon,-122.332100));

  // other talkers, and the line ending or a missing '$' / checksum
  CHECK(nmeaParseFix(nmea("GNRMC,000000,A,4736.372,N,12219.926,W,0,0,010125,,"),lat,lon));
  CHECK(nmeaParseFix(nmea("GLGGA,000000,4736.372,N,12219.926,W,1,05,1.0,10.0,M,0.0,M,,"),lat,lon));
  CHECK(nmeaParseFix(nmea("BDGLL,4736.372,N,12219.926,W,000000,A,A"),lat,lon));
  CHECK(nmeaParseFix("GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,",lat,lon));
  CHECK(nmeaParseFix("  $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n",lat,lon));

  // no fix
  CHECK(!nmeaParseFix(nmea("GPGGA,123519,,,,,0,00,99.9,,,,,,"),lat,lon));
  CHECK(!nmeaParseFix(nmea("GPGGA,123519,4807.038,N,01131.000,E,0,00,99.9,,,,,,"),lat,lon));
  CHECK(!nmeaParseFix(nmea("GPRMC,123519,V,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W"),lat,lon));
  CHECK(!nmeaParseFix(nmea("GPGLL,4916.45,N,12311.12,W,225444,V,N"),lat,lon));

  // bad checksum, truncated, garbage, other sentences
  CHECK(!nmeaParseFix("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*48",lat,lon));
  CHECK(!nmeaParseFix("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*4",lat,lon));
  CHECK(!nmeaParseFix("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*ZZ",lat,lon));
  CHECK(!nmeaParseFix(nmea("GPGGA,123519,4807.038,N"),lat,lon));
  CHECK(!nmeaParseFix(nmea("GPGGA"),lat,lon));
  CHECK(!nmeaParseFix("",lat,lon));
  CHECK(!nmeaParseFix("$",lat,lon));
  CHECK(!nmeaParseFix("garbage that is not nmea",lat,lon));
  CHECK(!nmeaParseFix(nmea("GPGSV,3,1,11,03,03,111,00,04,15,270,00,06,01,010,00,13,06,292,00"),lat,lon));
  CHECK(!nmeaParseFix(nmea("PUBX,00,081350.00,4717.113210,N,00833.915187,E,546.589,G3,2.1,2.0,0.007,77.52,0.007,,0.92,1.19,0.77,9,0,0"),lat,lon));

  // out of range, bad letters, bad minutes
  CHECK(!nmeaParseFix(nmea("GPGGA,000000,9100.000,N,01131.000,E,1,05,1.0,10.0,M,0.0,M,,"),lat,lon));
  CHECK(!nmeaParseFix(nmea("GPGGA,000000,4807.038,N,18100.000,E,1,05,1.0,10.0,M,0.0,M,,"),lat,lon));
  CHECK(!nmeaParseFix(nmea("GPGGA,000000,4807.038,X,01131.000,E,1,05,1.0,10.0,M,0.0,M,,"),lat,lon));
  CHECK(!nmeaParseFix(nmea("GPGGA,000000,4875.000,N,01131.000,E,1,05,1.0,10.0,M,0.0,M,,"),lat,lon));
  CHECK(!nmeaParseFix(nmea("GPGGA,000000,48a7.038,N,01131.000,E,1,05,1.0,10.0,M,0.0,M,,"),lat,lon));
  CHECK(nmeaParseFix(nmea("GPGGA,000000,9000.000,N,18000.000,E,1,05,1.0,10.0,M,0.0,M,,"),lat,lon));   // the poles / date line are valid
  CHECK(near(lat,90.0) && near(lon,180.0));

  // a failed parse leaves the outputs alone
  lat=1.5;
  lon=2.5;
  CHECK(!nmeaParseFix("garbage",lat,lon));
  CHECK(lat==1.5 && lon==2.5);

  CHECK(formatCoordinate(47.6062)=="47.60620");
  CHECK(formatCoordinate(-122.3321)=="-122.33210");
  CHECK(formatCoordinate(0)=="0.00000");
}

// ---- the coordinate formats of Configuration > GPS ----

static bool parses(const char *text,bool latitude,double expected)
{
  double d=12345;
  return parseCoordinateText(QString::fromUtf8(text),latitude,d) && near(d,expected);
}

static bool rejects(const char *text,bool latitude)
{
  double d=12345;
  return !parseCoordinateText(QString::fromUtf8(text),latitude,d) && d==12345;   // and the output is left alone
}

static void testCoordinates()
{
  // the examples of issue #133: "42 44.54N  084 23.82W", the longitude padded to three digits
  CHECK(formatCoordinate(42.0+44.54/60.0,true,COORDDEGMIN)=="42 44.54N");
  CHECK(formatCoordinate(-(84.0+23.82/60.0),false,COORDDEGMIN)=="084 23.82W");
  CHECK(formatCoordinate(47.6062,true,COORDDEGMIN)=="47 36.37N");
  CHECK(formatCoordinate(-122.3321,false,COORDDEGMIN)=="122 19.93W");
  CHECK(formatCoordinate(-33.85,true,COORDDEGMIN)=="33 51.00S");
  CHECK(formatCoordinate(151.2,false,COORDDEGMIN)=="151 12.00E");
  CHECK(formatCoordinate(7.5,true,COORDDEGMIN)=="07 30.00N");           // the latitude is padded to two digits
  CHECK(formatCoordinate(-7.5,false,COORDDEGMIN)=="007 30.00W");        // and the longitude to three
  CHECK(formatCoordinate(0.0,true,COORDDEGMIN)=="00 00.00N");
  CHECK(formatCoordinate(0.0,false,COORDDEGMIN)=="000 00.00E");
  CHECK(formatCoordinate(-0.000001,true,COORDDEGMIN)=="00 00.00N");     // rounds to zero: no hemisphere flip
  CHECK(formatCoordinate(-0.01,true,COORDDEGMIN)=="00 00.60S");
  CHECK(formatCoordinate(90.0,true,COORDDEGMIN)=="90 00.00N");
  CHECK(formatCoordinate(-90.0,true,COORDDEGMIN)=="90 00.00S");
  CHECK(formatCoordinate(180.0,false,COORDDEGMIN)=="180 00.00E");
  CHECK(formatCoordinate(-180.0,false,COORDDEGMIN)=="180 00.00W");
  // minutes that round up to 60 carry into the degrees
  CHECK(formatCoordinate(47.99999,true,COORDDEGMIN)=="48 00.00N");
  CHECK(formatCoordinate(-122.99999,false,COORDDEGMIN)=="123 00.00W");
  CHECK(formatCoordinate(47.0+59.996/60.0,true,COORDDEGMIN)=="48 00.00N");
  CHECK(formatCoordinate(47.0+59.994/60.0,true,COORDDEGMIN)=="47 59.99N");
  // the decimal format is the old one, whatever the axis
  CHECK(formatCoordinate(47.6062,true,COORDDECIMAL)=="47.60620");
  CHECK(formatCoordinate(-122.3321,false,COORDDECIMAL)=="-122.33210");
  CHECK(formatCoordinate(-122.3321,false,COORDDECIMAL)==formatCoordinate(-122.3321));

  // typed in, in any of the three notations
  CHECK(parses("47.6062",true,47.6062) && parses("-122.3321",false,-122.3321) && parses("+47.6062",true,47.6062));
  CHECK(parses("47.6062N",true,47.6062) && parses("N47.6062",true,47.6062) && parses("47.6062 S",true,-47.6062));
  CHECK(parses("122.3321W",false,-122.3321) && parses("w 122.3321",false,-122.3321) && parses("122.3321 E",false,122.3321));
  CHECK(parses("47 36.37N",true,47.6062) && parses("47 36.37",true,47.6062) && parses("47 36.37 n",true,47.6062));
  CHECK(parses("122 19.93W",false,-122.3321) && parses("-122 19.93",false,-122.3321) && parses("084 23.82W",false,-(84.0+23.82/60.0)));
  CHECK(parses("47\xC2\xB0" "36.37'N",true,47.6062));                              // 47°36.37'N
  CHECK(parses("47\xC2\xB0" "36'22\"N",true,47.0+36.0/60.0+22.0/3600.0));          // 47°36'22"N
  CHECK(parses("47 36 22.5N",true,47.0+36.0/60.0+22.5/3600.0) && parses("122 19 55.6W",false,-(122.0+19.0/60.0+55.6/3600.0)));
  CHECK(parses("47 36\xE2\x80\xB2" "22\xE2\x80\xB3N",true,47.0+36.0/60.0+22.0/3600.0));          // typographic prime and double prime
  CHECK(parses("47, 36.37, N",true,47.6062));
  CHECK(parses("  47   36.37N  ",true,47.6062));
  CHECK(parses("0",true,0.0) && parses("-0 30",true,-0.5) && parses("0 30S",true,-0.5));
  CHECK(parses("90",true,90.0) && parses("90 00.00N",true,90.0) && parses("-90",true,-90.0) && parses("180",false,180.0) && parses("180 0 0W",false,-180.0));
  CHECK(parses("59",true,59.0) && parses("47 59.99",true,47.0+59.99/60.0) && parses("47 59 59.9",true,47.0+59.0/60.0+59.9/3600.0));
  CHECK(parses(".5",true,0.5) && parses("47.",true,47.0));

  // not a coordinate
  CHECK(rejects("",true) && rejects("   ",true) && rejects("abc",true) && rejects("N",true) && rejects("47 N S",true));
  CHECK(rejects("122.3321E",true) && rejects("47.6062N",false));                       // a longitude letter in the latitude and the other way round
  CHECK(rejects("47.6062X",true) && rejects("X47.6062",true) && rejects("N47.6062S",true) && rejects("N 47 36.37N",true));
  CHECK(rejects("-47 36.37S",true) && rejects("-47.6062N",true) && rejects("-122.3321W",false));   // a sign and a letter say it twice
  CHECK(rejects("91",true) && rejects("90 00.01N",true) && rejects("181",false) && rejects("-91",true) && rejects("180 01W",false));
  CHECK(rejects("47 60",true) && rejects("47 60.00N",true) && rejects("47 36 60",true) && rejects("47 61",true));
  CHECK(rejects("47.5 36",true) && rejects("47 36.5 22",true));                        // only the last number may have decimals
  CHECK(rejects("47 36 22 10",true) && rejects("47 -36",true) && rejects("47-36",true));
  CHECK(parses("4 7",true,4.0+7.0/60.0));                                               // two numbers are always degrees and minutes
  CHECK(rejects("1e2",true) && rejects("47,6062",true) && rejects("47..6",true) && rejects("--47",true) && rejects("+-47",true));
  CHECK(rejects("47.6062 W",true));

  // format -> parse comes back to within the rounding of 0.01 minute
  for(int i=0;i<2000;i++)
    {
      const double lat=-90.0+180.0*i/1999.0;
      const double lon=-180.0+360.0*i/1999.0;
      double a=0,b=0;
      const bool la=parseCoordinateText(formatCoordinate(lat,true,COORDDEGMIN),true,a);
      const bool lo=parseCoordinateText(formatCoordinate(lon,false,COORDDEGMIN),false,b);
      CHECK(la && lo && std::fabs(a-lat)<=0.01/60.0/2.0+1e-9 && std::fabs(b-lon)<=0.01/60.0/2.0+1e-9);
      double c=0,d=0;
      CHECK(parseCoordinateText(formatCoordinate(lat,true,COORDDECIMAL),true,c) && parseCoordinateText(formatCoordinate(lon,false,COORDDECIMAL),false,d)
            && std::fabs(c-lat)<=0.5e-5+1e-9 && std::fabs(d-lon)<=0.5e-5+1e-9);
    }
}

// ---- the reader on a pseudo terminal ----

static int openPty(QString &slavePath)
{
  const int master=::posix_openpt(O_RDWR|O_NOCTTY|O_NONBLOCK);
  if(master<0 || ::grantpt(master)<0 || ::unlockpt(master)<0) return -1;
  slavePath=QString::fromLocal8Bit(::ptsname(master));
  return master;
}

struct Result
{
  bool got=false;
  bool ok=false;
  double lat=0,lon=0;
  QString reason;
};

// runs a request and waits for fixReady (or gives up after safetyMs, which would be a bug in the reader)
static Result waitFix(gpsReader &reader,int safetyMs=8000)
{
  Result r;
  QEventLoop loop;
  QMetaObject::Connection c=QObject::connect(&reader,&gpsReader::fixReady,[&](bool ok,double lat,double lon,const QString &reason)
    {
      r.got=true;
      r.ok=ok;
      r.lat=lat;
      r.lon=lon;
      r.reason=reason;
      loop.quit();
    });
  QTimer::singleShot(safetyMs,&loop,&QEventLoop::quit);
  loop.exec();
  QObject::disconnect(c);
  return r;
}

// writes the lines to the pty one per intervalMs once the loop runs
static void feed(int master,const QList<QByteArray> &lines,int intervalMs,QObject *owner)
{
  for(int i=0;i<lines.size();i++)
    {
      const QByteArray line=lines.at(i);
      QTimer::singleShot(intervalMs*(i+1),owner,[master,line] {if(::write(master,line.constData(),line.size())<0) {}});
    }
}

static void testReader()
{
  QString slave;
  const int master=openPty(slave);
  CHECK(master>=0);
  if(master<0) return;
  gpsReader reader;
  QObject owner;

  const QByteArray good=nmea("GPGGA,123519,4736.372,N,12219.926,W,1,08,0.9,545.4,M,46.9,M,,");
  const QByteArray noFix=nmea("GPGGA,123519,,,,,0,00,99.9,,,,,,");

  // 1. a fix after some sentences without one
  CHECK(reader.requestFix(slave,4800,5000));
  CHECK(reader.busy());
  CHECK(!reader.requestFix(slave,4800,5000));           // one request at a time
  feed(master,{noFix,nmea("GPGSV,1,1,00"),good},100,&owner);
  Result r=waitFix(reader);
  CHECK(r.got && r.ok);
  CHECK(near(r.lat,47.6062) && near(r.lon,-122.3321));
  CHECK(!reader.busy());

  // 2. a sentence cut in two by the serial driver
  CHECK(reader.requestFix(slave,9600,5000));
  QTimer::singleShot(100,&owner,[master,good] {if(::write(master,good.constData(),10)<0) {}});
  QTimer::singleShot(250,&owner,[master,good] {if(::write(master,good.constData()+10,good.size()-10)<0) {}});
  r=waitFix(reader);
  CHECK(r.got && r.ok && near(r.lat,47.6062));

  // 3. only "no fix" sentences: the timeout says the receiver is there but has no position
  CHECK(reader.requestFix(slave,4800,800));
  feed(master,{noFix,noFix,noFix},100,&owner);
  r=waitFix(reader);
  CHECK(r.got && !r.ok);
  CHECK(r.reason.contains("no position fix"));

  // 4. nothing at all
  CHECK(reader.requestFix(slave,4800,500));
  r=waitFix(reader);
  CHECK(r.got && !r.ok);
  CHECK(r.reason.contains("No GPS data"));

  // 5. old data in front of the request is not the current position
  const QByteArray stale=nmea("GPGGA,123519,1000.000,N,02000.000,E,1,08,0.9,545.4,M,46.9,M,,");
  if(::write(master,stale.constData(),stale.size())<0) {}
  {
    QEventLoop pause;
    QTimer::singleShot(200,&pause,&QEventLoop::quit);
    pause.exec();
  }
  CHECK(reader.requestFix(slave,4800,5000));
  feed(master,{good},300,&owner);
  r=waitFix(reader);
  CHECK(r.got && r.ok && near(r.lat,47.6062));

  // 6. a port that does not exist
  CHECK(reader.requestFix("/dev/does-not-exist-gps",4800,500));
  r=waitFix(reader);
  CHECK(r.got && !r.ok && r.reason.contains("Cannot open"));
  CHECK(!reader.busy());

  // 7. the saved settings: disabled, then enabled with the pty
  gpsEnabled=false;
  CHECK(reader.requestFix(500));
  r=waitFix(reader);
  CHECK(r.got && !r.ok && r.reason.contains("not enabled"));
  gpsEnabled=true;
  gpsPort=slave;
  gpsBaud=4800;
  CHECK(reader.requestFix(5000));
  feed(master,{good},100,&owner);
  r=waitFix(reader);
  CHECK(r.got && r.ok && near(r.lon,-122.3321));

  // 8. the receiver sends garbage without line endings
  CHECK(reader.requestFix(slave,4800,600));
  {
    // the pty buffer holds a few KB and this thread is the reader: write in pieces, never block
    const QByteArray junk(2000,'x');
    for(int i=0;i<6;i++)
      QTimer::singleShot(50+30*i,&owner,[master,junk] {if(::write(master,junk.constData(),junk.size())<0) {}});
  }
  r=waitFix(reader);
  CHECK(r.got && !r.ok);

  ::close(master);
}

// ---- the place lookup ----

static QString placeOf(const char *json,QString *err=nullptr)
{
  QString e;
  const QString p=placeLookup::placeFromReply(json,e);
  if(err) *err=e;
  return p;
}

static void testPlaceParser()
{
  QString err;
  // the replies of the real service for three famous spots (shortened)
  CHECK(placeOf("{\"addresstype\":\"city\",\"name\":\"Seattle\",\"address\":{\"city\":\"Seattle\",\"county\":\"King County\",\"state\":\"Washington\",\"ISO3166-2-lvl4\":\"US-WA\",\"country\":\"United States\",\"country_code\":\"us\"}}")=="Seattle, WA");
  CHECK(placeOf("{\"address\":{\"city\":\"Missoula\",\"county\":\"Missoula County\",\"state\":\"Montana\",\"ISO3166-2-lvl4\":\"US-MT\",\"country\":\"United States\",\"country_code\":\"us\"}}")=="Missoula, MT");
  CHECK(placeOf("{\"address\":{\"city\":\"City of Westminster\",\"ISO3166-2-lvl8\":\"GB-WSM\",\"state\":\"England\",\"ISO3166-2-lvl4\":\"GB-ENG\",\"country\":\"United Kingdom\",\"country_code\":\"gb\"}}")=="City of Westminster, England");

  // the codes of Canada and Australia, the names elsewhere
  CHECK(placeOf("{\"address\":{\"town\":\"Banff\",\"state\":\"Alberta\",\"ISO3166-2-lvl4\":\"CA-AB\",\"country_code\":\"ca\"}}")=="Banff, AB");
  CHECK(placeOf("{\"address\":{\"city\":\"Sydney\",\"state\":\"New South Wales\",\"ISO3166-2-lvl4\":\"AU-NSW\",\"country_code\":\"au\"}}")=="Sydney, NSW");
  CHECK(placeOf("{\"address\":{\"city\":\"Munich\",\"state\":\"Bavaria\",\"ISO3166-2-lvl4\":\"DE-BY\",\"country_code\":\"de\"}}")=="Munich, Bavaria");
  // a US place whose reply has no ISO code falls back to the name of the state
  CHECK(placeOf("{\"address\":{\"city\":\"Seattle\",\"state\":\"Washington\",\"country_code\":\"us\"}}")=="Seattle, Washington");
  // the code of another country is not taken for a US state
  CHECK(placeOf("{\"address\":{\"city\":\"Tijuana\",\"state\":\"Baja California\",\"ISO3166-2-lvl4\":\"MX-BCN\",\"country_code\":\"us\"}}")=="Tijuana, Baja California");

  // the smallest named place wins; the county stands in for a place without a name of its own
  CHECK(placeOf("{\"address\":{\"village\":\"Tiny\",\"county\":\"X County\",\"state\":\"Montana\",\"ISO3166-2-lvl4\":\"US-MT\",\"country_code\":\"us\"}}")=="Tiny, MT");
  CHECK(placeOf("{\"address\":{\"hamlet\":\"Hamlet\",\"county\":\"X County\",\"state\":\"Ohio\",\"ISO3166-2-lvl4\":\"US-OH\",\"country_code\":\"us\"}}")=="Hamlet, OH");
  CHECK(placeOf("{\"address\":{\"county\":\"King County\",\"state\":\"Washington\",\"ISO3166-2-lvl4\":\"US-WA\",\"country_code\":\"us\"}}")=="King County, WA");

  // no state: the country stands in, unless it is the place itself
  CHECK(placeOf("{\"address\":{\"city\":\"Paris\",\"country\":\"France\",\"country_code\":\"fr\"}}")=="Paris, France");
  CHECK(placeOf("{\"address\":{\"city\":\"Monaco\",\"country\":\"Monaco\",\"country_code\":\"mc\"}}")=="Monaco");
  // no place name at all: the state alone
  CHECK(placeOf("{\"address\":{\"state\":\"Texas\",\"ISO3166-2-lvl4\":\"US-TX\",\"country_code\":\"us\"}}")=="TX");
  CHECK(placeOf("{\"address\":{\"city\":\"  Seattle \",\"state\":\"Washington\",\"country_code\":\"xx\"}}")=="Seattle, Washington");

  // nothing to show
  CHECK(placeOf("{\"error\":\"Unable to geocode\"}",&err).isEmpty() && err.contains("No place"));
  CHECK(placeOf("{\"address\":{}}",&err).isEmpty() && err.contains("No place"));
  CHECK(placeOf("{\"address\":{\"road\":\"Main Street\"}}",&err).isEmpty() && err.contains("No place"));
  CHECK(placeOf("",&err).isEmpty() && err.contains("Unexpected"));
  CHECK(placeOf("<html>502 Bad Gateway</html>",&err).isEmpty() && err.contains("Unexpected"));
  CHECK(placeOf("[1,2,3]",&err).isEmpty() && err.contains("Unexpected"));
  CHECK(placeOf("{\"address\":{\"city\":\"Seattle\"}}",&err)=="Seattle" && err.isEmpty());
}

struct PlaceResult
{
  bool got=false;
  bool ok=false;
  QString place,reason;
};

static PlaceResult waitPlace(placeLookup &pl,int safetyMs=8000)
{
  PlaceResult r;
  QEventLoop loop;
  QMetaObject::Connection c=QObject::connect(&pl,&placeLookup::placeReady,[&](bool ok,const QString &place,const QString &reason)
    {
      r.got=true;
      r.ok=ok;
      r.place=place;
      r.reason=reason;
      loop.quit();
    });
  QTimer::singleShot(safetyMs,&loop,&QEventLoop::quit);
  loop.exec();
  QObject::disconnect(c);
  return r;
}

static void testPlaceLookup()
{
  fakeService fake;
  CHECK(fake.server.isListening());
  placeLookup pl;
  pl.setServiceUrl(fake.url());
  fake.body=seattle;

  // 1. an answer, and what the service is asked
  bool early=false;
  QMetaObject::Connection c=QObject::connect(&pl,&placeLookup::placeReady,[&] {early=true;});
  CHECK(pl.requestPlace(47.6062,-122.3321));
  CHECK(!early);                                       // the answer never arrives inside the call
  QObject::disconnect(c);
  CHECK(pl.busy());
  CHECK(!pl.requestPlace(1,2));                        // one request at a time
  PlaceResult r=waitPlace(pl);
  CHECK(r.got && r.ok && r.place=="Seattle, WA");
  CHECK(!pl.busy());
  CHECK(fake.hits==1);
  const QByteArray head=fake.lastRequest;
  CHECK(head.startsWith("GET /reverse?"));
  CHECK(head.contains("lat=47.60620") && head.contains("lon=-122.33210"));
  CHECK(head.contains("format=jsonv2") && head.contains("zoom=10") && head.contains("addressdetails=1") && head.contains("accept-language=en"));
  CHECK(head.contains("User-Agent: QSSTV-Experimental"));      // the service refuses requests without one

  // 2. a position within a kilometre of the answered one needs no request
  early=false;
  c=QObject::connect(&pl,&placeLookup::placeReady,[&] {early=true;});
  CHECK(pl.requestPlace(47.6100,-122.3300));
  CHECK(!early);
  QObject::disconnect(c);
  r=waitPlace(pl);
  CHECK(r.got && r.ok && r.place=="Seattle, WA");
  CHECK(fake.hits==1);

  // 3. a position further away asks again
  fake.body=missoula;
  CHECK(pl.requestPlace(46.8721,-113.9940));
  r=waitPlace(pl);
  CHECK(r.got && r.ok && r.place=="Missoula, MT");
  CHECK(fake.hits==2);
  CHECK(fake.lastRequest.contains("lat=46.87210") && fake.lastRequest.contains("lon=-113.99400"));

  // 4. the service refuses (rate limit): a reason, and the failure is not remembered
  fake.status=429;
  CHECK(pl.requestPlace(40.0,-100.0));
  r=waitPlace(pl);
  CHECK(r.got && !r.ok && r.reason.contains("429"));
  fake.status=200;
  fake.body=seattle;
  CHECK(pl.requestPlace(40.0,-100.0));
  r=waitPlace(pl);
  CHECK(r.got && r.ok && r.place=="Seattle, WA");
  CHECK(fake.hits==4);

  // 5. an answer that holds no place (open sea) or is not JSON
  fake.body="{\"error\":\"Unable to geocode\"}";
  CHECK(pl.requestPlace(0.0,-30.0));
  r=waitPlace(pl);
  CHECK(r.got && !r.ok && r.reason.contains("No place"));
  fake.body="<html>proxy login</html>";
  CHECK(pl.requestPlace(0.5,-30.0));
  r=waitPlace(pl);
  CHECK(r.got && !r.ok && r.reason.contains("Unexpected"));
  CHECK(!pl.busy());

  // 6. a service that does not answer: the timeout ends the request
  fake.silent=true;
  CHECK(pl.requestPlace(10.0,10.0,400));
  r=waitPlace(pl);
  CHECK(r.got && !r.ok && r.reason.contains("did not answer"));
  fake.silent=false;

  // 7. nobody listens at the address
  const QUrl dead=fake.url();
  fake.server.close();
  pl.setServiceUrl(dead);
  CHECK(pl.requestPlace(20.0,20.0));
  r=waitPlace(pl);
  CHECK(r.got && !r.ok && r.reason.contains("Cannot reach"));
  CHECK(!pl.busy());
}

int main(int argc,char **argv)
{
  QCoreApplication app(argc,argv);
  testParser();
  testCoordinates();
  std::printf("parser done\n");
  std::fflush(stdout);
  testPlaceParser();
  testPlaceLookup();
  std::printf("place lookup done\n");
  std::fflush(stdout);
  testReader();
  if(failures) std::printf("%d check(s) FAILED\n",failures);
  else std::printf("gps: all checks passed\n");
  return failures?1:0;
}
