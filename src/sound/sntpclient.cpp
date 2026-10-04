#include "sntpclient.h"
#include "ntppacket.h"
#include "rawclock.h"

#include <QUdpSocket>
#include <QHostInfo>
#include <QTimer>
#include <QRandomGenerator>
#include <algorithm>

#define NTPPORT 123
#define BURSTSIZE 4             // requests per round and server
#define BURSTSPACING 2.0        // seconds between requests of a burst (NIST asks for 4 s or more between requests per client
                                // on average; the bursts are 64 s apart)
#define ROUNDINTERVAL 64.0      // seconds between the start of two bursts to one server
#define REPLYWAIT 2.0           // seconds the last request of a burst is waited for
#define MAXDELAY 0.5            // s, a round trip longer than this is not used
#define DELAYFACTOR 2.0         // reject a burst whose best delay is more than this times the server's minimum ...
#define DELAYSLACK 0.005        // ... plus this many seconds
#define STAGGER 5.0             // seconds between the first requests to the different servers

sntpClient::sntpClient(QObject *parent) : QObject(parent),running(false)
{
  timer=new QTimer(this);
  timer->setInterval(250);
  connect(timer,SIGNAL(timeout()),this,SLOT(slotTick()));
}

sntpClient::~sntpClient()
{
  stop();
}

void sntpClient::clear()
{
  qDeleteAll(servers);
  servers.clear();
}

void sntpClient::start()
{
  stop();
  running=true;
  double now=rawMonotonicSeconds();
  for(int i=0;i<names.count();i++)
    {
      QString n=names.at(i).trimmed();
      if(n.isEmpty()) continue;
      server *s=new server;
      s->name=n;
      s->nextSend=now+STAGGER*servers.count();
      servers.append(s);
      resolve(servers.count()-1);
    }
  if(servers.isEmpty()) report(tr("No NTP servers given."));
  timer->start();
}

void sntpClient::stop()
{
  timer->stop();
  running=false;
  clear();
}

void sntpClient::resolve(int idx)
{
  server *s=servers.at(idx);
  QHostAddress direct;
  if(direct.setAddress(s->name))
    {
      s->addr=direct;
      s->resolved=true;
      return;
    }
  QObject *guard=this;
  QHostInfo::lookupHost(s->name,guard,[this,s](const QHostInfo &info)
    {
      if(!servers.contains(s)) return;          // stopped meanwhile
      if(info.error()!=QHostInfo::NoError || info.addresses().isEmpty())
        {
          s->failed=true;
          report(tr("%1: cannot resolve the name").arg(s->name));
          return;
        }
      // prefer IPv4: simplest path, and one fixed address per server
      QHostAddress pick=info.addresses().first();
      for(const QHostAddress &a:info.addresses())
        if(a.protocol()==QAbstractSocket::IPv4Protocol) {pick=a;break;}
      s->addr=pick;
      s->resolved=true;
    });
}

void sntpClient::sendRequest(server *s)
{
  if(s->sock==nullptr)
    {
      s->sock=new QUdpSocket(this);
      int idx=servers.indexOf(s);
      connect(s->sock,&QUdpSocket::readyRead,this,[this,idx](){readReplies(idx);});
    }
  unsigned char buf[NTPPACKETSIZE];
  s->origin=(quint64(QRandomGenerator::global()->generate())<<32)|QRandomGenerator::global()->generate();
  ntpBuildRequest(buf,s->origin);
  s->T0=rawMonotonicSeconds();
  s->sock->writeDatagram((const char*)buf,NTPPACKETSIZE,s->addr,NTPPORT);
  s->sent++;
}

void sntpClient::readReplies(int idx)
{
  if(idx<0 || idx>=servers.count()) return;
  server *s=servers.at(idx);
  while(s->sock->hasPendingDatagrams())
    {
      unsigned char buf[512];
      QHostAddress from;
      quint16 port;
      qint64 len=s->sock->readDatagram((char*)buf,sizeof(buf),&from,&port);
      double T3=rawMonotonicSeconds();
      if(len<0 || s->origin==0 || !from.isEqual(s->addr,QHostAddress::ConvertV4MappedToIPv4)) continue;
      ntpReply r=ntpParseReply(buf,int(len));
      if(!r.ok)
        {
          if(len>=NTPPACKETSIZE && buf[1]==0) report(tr("%1: server refuses (kiss-o'-death), try other servers").arg(s->name));
          continue;
        }
      if(r.origin!=s->origin) continue;           // not the answer to our last request
      s->origin=0;                                // each request is answered once
      double theta,delay;
      ntpOffsetDelay(s->T0,T3,r,theta,delay);
      if(delay<0 || delay>MAXDELAY) continue;
      if(!s->haveBest || delay<s->bestDelay)
        {
          s->haveBest=true;
          s->bestDelay=delay;
          s->bestTheta=theta;
          s->bestLocal=0.5*(s->T0+T3);
        }
    }
}

void sntpClient::finishBurst(int idx)
{
  server *s=servers.at(idx);
  if(!s->haveBest)
    {
      report(tr("%1: no reply").arg(s->name));
      return;
    }
  bool accept=true;
  if(s->accepted>=2 && s->bestDelay>DELAYFACTOR*s->minDelay+DELAYSLACK) accept=false;   // congested round
  s->minDelay=std::min(s->minDelay,s->bestDelay);
  if(accept)
    {
      s->accepted++;
      emit sampleReady(idx,s->bestLocal,s->bestTheta,s->bestDelay);
    }
  s->haveBest=false;
}

void sntpClient::slotTick()
{
  double now=rawMonotonicSeconds();
  for(int i=0;i<servers.count();i++)
    {
      server *s=servers.at(i);
      if(!s->resolved || s->failed) continue;
      if(s->sent<BURSTSIZE)
        {
          if(now>=s->nextSend)
            {
              sendRequest(s);
              s->nextSend=now+BURSTSPACING;
            }
        }
      else if(now>=s->nextSend-BURSTSPACING+REPLYWAIT)
        {
          // burst done and the last reply had its time
          finishBurst(i);
          s->sent=0;
          s->nextSend=now+ROUNDINTERVAL-BURSTSIZE*BURSTSPACING;
        }
    }
}
