#ifndef LOGBOOK_H
#define LOGBOOK_H

#include <QString>
#include <QUdpSocket>
#include <QObject>
#include <QDateTime>

#define NUMLOGPARAMS 21

struct slogParam
{
  QString tag;
  QString val;
};



class logBook : public QObject
{
  Q_OBJECT
public:
  enum eIndex {LPROG,LVER,LDATE,LTIME,LENDTIME,LCALL,LFREQ,LMODE,LTX,LRX,LNAME,LQTH,LSTATE,LPROV,LCNTRY,LLOC,LSO,LSI,LFREE,LNOTES,LPWR};
  logBook();
  /** thread-safe: callable from the RX thread, the work is queued to the thread that owns this object */
  void logQSO(QString call, QString mode, QString comment);
private slots:
  void doLogQSO(QString call, QString mode, QString comment, QDateTime dt);
private:
  void getFrequency();
  double frequency;
  void setParam(eIndex tag,QString value);
  QString buildADIFRecord() const;
  QUdpSocket udpSocket;
  quint16 udpPort;
};

#endif // LOGBOOK_H
