#include "nmeaparser.h"

#include <QList>

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
