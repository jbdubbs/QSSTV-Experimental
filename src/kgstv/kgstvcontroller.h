#ifndef KGSTVCONTROLLER_H
#define KGSTVCONTROLLER_H
#include "kgstvaudio.h"
#include "kgstvimage.h"
#include "kgstvtransfer.h"
#include <QObject>
#include <QMutex>
#include <QElapsedTimer>
#include <memory>
namespace Kgstv {
struct Options {int quality=8;bool fec=true,fourLevel=false;};
// Thread-safe bridge: UI prepares jobs, RX supplies 12 kHz PCM, TX pulls 48 kHz PCM.
// It does not own devices or PTT. The dispatcher retains exclusive radio ownership.
class Controller:public QObject {
  Q_OBJECT
public:
  enum class Job { None,Image,Text,Request,Repair };
  explicit Controller(QObject *parent=nullptr);
  Options options() const;
  bool configure(const Options &options,QString *error=nullptr);
  bool resetDemodulator(double sampleClock=48000.0,QString *error=nullptr);
  void clearReceive();
  void leaveMode();
  bool receive(const double *samples,int count,QString *error=nullptr);
  bool prepareImageJob(const QImage &image,const QString &callsign,QString *error=nullptr);
  bool prepareTextJob(const QString &text,const QString &callsign,QString *error=nullptr);
  bool prepareRequestJob(const QString &callsign,QString *error=nullptr);
  bool prepareRepairJob(const QString &callsign,QString *error=nullptr);
  bool prepared() const;
  bool busy() const;
  bool beginTransmit(QString *error=nullptr,double sampleClock=48000.0);
  int render(double *samples,int capacity,QString *error=nullptr);
  void completeTransmit(bool success);
  void cancelTransmit();
  double duration() const;
  int receivedCount() const;
  bool receiving() const;
  quint64 receiveEpoch() const;
  int pendingRepairs() const;
  QImage receivedImage() const;
signals:
  void imageChanged(QImage image,int received,bool complete,quint64 epoch);
  void messageReceived(QString sender,QString text);
  void statusChanged(QString status);
  void localTextResult(QString text,bool sent);
  void optionsChanged();
  void transmitFinished(bool success);
private:
  mutable QMutex mutex;
  Options settings;
  std::unique_ptr<AudioReceiver> receiver;
  std::unique_ptr<AudioTransmitter> transmitter;
  BlockTransfer rx,tx;
  QImage canvas;
  QString sender;
  QElapsedTimer lastBlock;
  bool newImage=true,preparing=false,active=false,rendered=false,receiveAborted=false;
  quint64 generation=0,rxEpoch=0;
  QVector<Packet> plan,reservedRepairs;
  int nextPacket=0;
  Job job=Job::None;
  QString sentText;
  double inputStep=1.0,nextSample=0,previousSample=0;
  bool previousSampleValid=false;
  void resetSampling();
  bool idle(QString *error) const;
  bool identification(const QString &callsign,const Options &options,QVector<Packet> *packets,QString *error) const;
  Packet endPacket(const Options &options) const;
  void cancelLocked();
};
}
#endif
