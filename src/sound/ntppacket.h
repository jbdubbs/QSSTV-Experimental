#ifndef NTPPACKET_H
#define NTPPACKET_H

#include <cstdint>
#include <cstring>

/*!
  The 48 byte (S)NTP packet, without any Qt, so the arithmetic can be tested on its own. Times are kept as
  seconds since 1970 in a double (0.24 us resolution, plenty for a ppm measurement over minutes).
*/
#define NTPPACKETSIZE 48
#define NTPUNIXOFFSET 2208988800.0     // seconds between 1900 and 1970

struct ntpReply
{
  bool ok=false;
  int stratum=0;
  int leap=0;
  uint64_t origin=0;      //!< echo of our transmit timestamp, raw 64 bit
  double receive=0;       //!< t1: server clock when the request arrived
  double transmit=0;      //!< t2: server clock when the reply left
};

inline uint32_t ntpGet32(const unsigned char *p)
{
  return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|uint32_t(p[3]);
}

inline void ntpPut32(unsigned char *p,uint32_t v)
{
  p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;
}

/** raw NTP timestamp (32 bit seconds since 1900, 32 bit fraction) to seconds since 1970; era 1 (after 2036) handled */
inline double ntpToUnix(uint64_t raw)
{
  double secs=double(uint32_t(raw>>32));
  if(secs<2085978496.0) secs+=4294967296.0;    // before 1970+: the 32 bit seconds have wrapped (year 2036)
  return secs-NTPUNIXOFFSET+double(uint32_t(raw&0xffffffffu))/4294967296.0;
}

/** client request; the transmit timestamp is a random number that comes back as the origin timestamp */
inline void ntpBuildRequest(unsigned char *buf,uint64_t randomOrigin)
{
  std::memset(buf,0,NTPPACKETSIZE);
  buf[0]=(0<<6)|(4<<3)|3;                 // LI 0, version 4, mode 3 (client)
  ntpPut32(buf+40,uint32_t(randomOrigin>>32));
  ntpPut32(buf+44,uint32_t(randomOrigin&0xffffffffu));
}

inline ntpReply ntpParseReply(const unsigned char *buf,int len)
{
  ntpReply r;
  if(len<NTPPACKETSIZE) return r;
  int mode=buf[0]&7;
  r.leap=buf[0]>>6;
  r.stratum=buf[1];
  r.origin=(uint64_t(ntpGet32(buf+24))<<32)|ntpGet32(buf+28);
  uint64_t rx=(uint64_t(ntpGet32(buf+32))<<32)|ntpGet32(buf+36);
  uint64_t tx=(uint64_t(ntpGet32(buf+40))<<32)|ntpGet32(buf+44);
  r.receive=ntpToUnix(rx);
  r.transmit=ntpToUnix(tx);
  r.ok=(mode==4 || mode==5) && r.leap!=3 && r.stratum>=1 && r.stratum<=15 && rx!=0 && tx!=0;
  return r;
}

/**
  The four timestamp exchange. T0 and T3 are the local clock when the request left and the reply arrived,
  r.receive and r.transmit the server's. Returns theta = server clock minus local clock (at the midpoint of the
  exchange) and delay = round trip time minus the server's own processing time.
*/
inline void ntpOffsetDelay(double T0,double T3,const ntpReply &r,double &theta,double &delay)
{
  theta=((r.receive-T0)+(r.transmit-T3))/2.0;
  delay=(T3-T0)-(r.transmit-r.receive);
}

#endif // NTPPACKET_H
