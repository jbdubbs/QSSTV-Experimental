#ifndef LOGBOOK_H
#define LOGBOOK_H

#include <QString>
#include <QUdpSocket>

#define NUMLOGPARAMS 21

struct slogParam
{
  QString tag;
  QString val;
};



class logBook
{
public:
  enum eIndex {LPROG,LVER,LDATE,LTIME,LENDTIME,LCALL,LFREQ,LMODE,LTX,LRX,LNAME,LQTH,LSTATE,LPROV,LCNTRY,LLOC,LSO,LSI,LFREE,LNOTES,LPWR};
  logBook();
  void logQSO(QString call, QString mode, QString comment);
private:
  void getFrequency();
  double frequency;
  void setParam(eIndex tag,QString value);
  QString buildADIFRecord() const;
  QUdpSocket udpSocket;
  quint16 udpPort;
};

#endif // LOGBOOK_H
