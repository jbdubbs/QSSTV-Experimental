#ifndef SNTPCLIENT_H
#define SNTPCLIENT_H

#include <QObject>
#include <QStringList>
#include <QHostAddress>
#include <QList>

class QUdpSocket;
class QTimer;

/*!
  Minimal SNTP client for the NTP calibration (see calibrationNtp). It does not set any clock: it measures how
  far each server's UTC is from the local raw monotonic clock (rawMonotonicSeconds()) and reports one sample
  per server per round. The offset changes with the slope of the local clock error, which is what the
  calibration needs; constant path asymmetry only moves the offset and is harmless.

  Each round sends a short burst to a server and keeps the reply with the least delay (the least queueing, so the
  least asymmetry), as ntpd's clock filter does. Servers are resolved once at start() so that every sample of
  a server comes from the same machine (a pool name would otherwise hand out a new one each lookup).
*/
class sntpClient : public QObject
{
  Q_OBJECT
public:
  explicit sntpClient(QObject *parent=nullptr);
  ~sntpClient();
  void setServers(const QStringList &s) {names=s;}
  void start();
  void stop();
  bool isRunning() const {return running;}
  int serverCount() const {return servers.count();}
  QString serverName(int i) const {return servers.at(i)->name;}

signals:
  /** server index, local raw clock at the middle of the exchange, server minus local clock in s, delay in s */
  void sampleReady(int server,double localTime,double theta,double delay);
  void statusChanged(const QString &text);

private slots:
  void slotTick();

private:
  struct server
  {
    QString name;
    QHostAddress addr;
    QUdpSocket *sock=nullptr;
    bool resolved=false;
    bool failed=false;
    double nextSend=0;       // raw clock
    int sent=0;              // requests sent in the current burst
    quint64 origin=0;        // transmit timestamp of the request in flight
    double T0=0;
    bool haveBest=false;
    double bestLocal=0,bestTheta=0,bestDelay=0;
    double minDelay=1e9;
    int accepted=0;
  };
  QStringList names;
  QList<server*> servers;
  QTimer *timer;
  bool running;
  void clear();
  void resolve(int idx);
  void sendRequest(server *s);
  void finishBurst(int idx);
  void readReplies(int idx);
  void report(const QString &t) {emit statusChanged(t);}
};

#endif // SNTPCLIENT_H
