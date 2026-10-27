#include "nmeaparser.h"

#include <QList>
#include <QRegularExpression>
#include <QStringList>
#include <cmath>

namespace
{

// ddmm.mmmm (latitude) or dddmm.mmmm (longitude) plus a hemisphere letter to signed degrees
bool parseCoordinate(const QByteArray &value,const QByteArray &hemisphere,int degreeDigits,double maxDegrees,double &degrees)
{
  if(value.size()<=degreeDigits || hemisphere.size()!=1) return false;
  for(int i=0;i<value.size();i++)
    {
      const char c=value.at(i);
      if(!((c>='0' && c<='9') || (c=='.' && i>=degreeDigits+2))) return false;
    }
  bool ok=false;
  const int deg=value.left(degreeDigits).toInt(&ok);
  if(!ok) return false;
  const double minutes=value.mid(degreeDigits).toDouble(&ok);
  if(!ok || minutes>=60.0) return false;
  double d=deg+minutes/60.0;
  if(d>maxDegrees) return false;
  const char h=hemisphere.at(0);
  if(h=='S' || h=='W') d=-d;
  else if(h!='N' && h!='E') return false;
  degrees=d;
  return true;
}

bool parsePosition(const QByteArray &lat,const QByteArray &ns,const QByteArray &lon,const QByteArray &ew,double &outLat,double &outLon)
{
  double la=0,lo=0;
  if(!parseCoordinate(lat,ns,2,90.0,la)) return false;
  if(!parseCoordinate(lon,ew,3,180.0,lo)) return false;
  outLat=la;
  outLon=lo;
  return true;
}

}  // namespace

bool nmeaParseFix(const QByteArray &sentence,double &lat,double &lon)
{
  QByteArray s=sentence.trimmed();
  if(s.startsWith('$')) s.remove(0,1);
  const int star=s.indexOf('*');
  if(star>=0)
    {
      bool ok=false;
      const int expected=s.mid(star+1,2).toInt(&ok,16);
      if(!ok || s.size()!=star+3) return false;
      int sum=0;
      for(int i=0;i<star;i++) sum^=static_cast<unsigned char>(s.at(i));
      if(sum!=expected) return false;
      s.truncate(star);
    }
  const QList<QByteArray> f=s.split(',');
  if(f.isEmpty() || f.at(0).size()!=5) return false;
  const QByteArray type=f.at(0).mid(2);

  if(type=="GGA")
    {
      // GGA,time,lat,N/S,lon,E/W,quality,...
      if(f.size()<7 || f.at(6).isEmpty() || f.at(6).toInt()<=0) return false;
      return parsePosition(f.at(2),f.at(3),f.at(4),f.at(5),lat,lon);
    }
  if(type=="RMC")
    {
      // RMC,time,status,lat,N/S,lon,E/W,...
      if(f.size()<7 || f.at(2)!="A") return false;
      return parsePosition(f.at(3),f.at(4),f.at(5),f.at(6),lat,lon);
    }
  if(type=="GLL")
    {
      // GLL,lat,N/S,lon,E/W,time,status[,mode]
      if(f.size()<7 || f.at(6)!="A") return false;
      return parsePosition(f.at(1),f.at(2),f.at(3),f.at(4),lat,lon);
    }
  return false;
}

QString formatCoordinate(double degrees)
{
  return QString::number(degrees,'f',5);
}

QString formatCoordinate(double degrees,bool latitude,int format)
{
  if(format!=COORDDEGMIN) return formatCoordinate(degrees);
  const qint64 hundredths=qRound64(std::fabs(degrees)*6000.0);   // hundredths of a minute: 60 minutes of 100 hundredths per degree
  const int deg=static_cast<int>(hundredths/6000);
  const int minutes=static_cast<int>(hundredths%6000);
  const bool negative=(degrees<0 && hundredths>0);   // a position that rounds to zero is north / east
  const QChar hemisphere=latitude? (negative? 'S':'N'):(negative? 'W':'E');
  return QString("%1 %2.%3%4").arg(deg,latitude? 2:3,10,QChar('0')).arg(minutes/100,2,10,QChar('0')).arg(minutes%100,2,10,QChar('0')).arg(hemisphere);
}

bool parseCoordinateText(const QString &text,bool latitude,double &degrees)
{
  QString s=text.toUpper();
  for(QChar &c:s)
    {
      // degree, minute and second marks (also the typographic ones) and commas separate the numbers like a blank
      if(c==QChar(0x00B0) || c==QChar(0x2032) || c==QChar(0x2033) || c=='\'' || c=='"' || c==',') c=' ';
    }
  s=s.simplified();
  if(s.isEmpty()) return false;

  QChar hemisphere;
  if(s.at(0).isLetter())
    {
      hemisphere=s.at(0);
      s.remove(0,1);
    }
  if(!s.isEmpty() && s.at(s.size()-1).isLetter())
    {
      if(!hemisphere.isNull()) return false;
      hemisphere=s.at(s.size()-1);
      s.chop(1);
    }
  if(!hemisphere.isNull() && !(latitude? (hemisphere=='N' || hemisphere=='S'):(hemisphere=='E' || hemisphere=='W'))) return false;

  const QStringList part=s.simplified().split(' ',Qt::SkipEmptyParts);
  if(part.isEmpty() || part.size()>3) return false;
  static const QRegularExpression integerNumber("^[0-9]+$");
  static const QRegularExpression decimalNumber("^([0-9]+\\.?[0-9]*|\\.[0-9]+)$");
  QString first=part.at(0);
  const bool minus=first.startsWith('-');
  if(minus || first.startsWith('+')) first.remove(0,1);
  if(minus && !hemisphere.isNull()) return false;   // "-47 36S" says it twice, and may mean it differently
  // only the last number may have decimals: 47.6062, 47 36.37, 47 36 22.5
  for(int i=0;i<part.size();i++)
    {
      const QString &number=(i==0)? first:part.at(i);
      const bool last=(i==part.size()-1);
      if(!(last? decimalNumber:integerNumber).match(number).hasMatch()) return false;
    }
  double value=first.toDouble();
  if(part.size()>1)
    {
      const double minutes=part.at(1).toDouble();
      if(minutes>=60.0) return false;
      value+=minutes/60.0;
    }
  if(part.size()>2)
    {
      const double seconds=part.at(2).toDouble();
      if(seconds>=60.0) return false;
      value+=seconds/3600.0;
    }
  if(value>(latitude? 90.0:180.0)) return false;
  if(minus || hemisphere=='S' || hemisphere=='W') value=-value;
  degrees=value;
  return true;
}
