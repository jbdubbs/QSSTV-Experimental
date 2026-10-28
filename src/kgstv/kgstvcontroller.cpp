#include "kgstvcontroller.h"
#include <QMutexLocker>
#include <cmath>
namespace Kgstv {
namespace {
bool fail(QString *error,const QString &message){if(error)*error=message;return false;}
QString displayText(const Bits &bits) {
  QByteArray bytes;if(!bitBytes(bits,&bytes))return QString();QString result;
  for(unsigned char byte:bytes) {
    if((byte>=32 && byte<=126) || byte=='\n' || byte=='\r' || byte=='\t')result+=QChar(byte);
    else result+=QString("\\x%1").arg(byte,2,16,QChar('0'));
  }
  return result;
}
}
Controller::Controller(QObject *parent):QObject(parent),receiver(new AudioReceiver(false,12000)) {
  canvas=QImage(320,240,QImage::Format_RGB32);canvas.fill(Qt::black);rx.beginReceive();
}
Options Controller::options() const {QMutexLocker lock(&mutex);return settings;}
bool Controller::configure(const Options &options,QString *error) {
  if(error)error->clear();
  {QMutexLocker lock(&mutex);
    if(options.quality<0 || options.quality>15)return fail(error,"KG-STV quality must be 0..15.");
    if(!idle(error))return false;
    if(settings.quality==options.quality && settings.fec==options.fec && settings.fourLevel==options.fourLevel)return true;
    const bool changedModulation=settings.fourLevel!=options.fourLevel;settings=options;
    if(changedModulation){receiver.reset(new AudioReceiver(settings.fourLevel,12000));resetSampling();}
  }
  emit optionsChanged();return true;
}
void Controller::resetSampling(){nextSample=0;previousSample=0;previousSampleValid=false;}
bool Controller::resetDemodulator(double sampleClock,QString *error){
  if(error)error->clear();
  if(!std::isfinite(sampleClock) || std::abs(sampleClock/48000.0-1.0)>0.00201)return fail(error,"Invalid KG-STV receive clock.");
  QMutexLocker lock(&mutex);receiver->reset();lastBlock.invalidate();inputStep=sampleClock/48000.0;resetSampling();return true;
}
void Controller::clearReceive() {
  QImage cleared;quint64 epoch;
  {QMutexLocker lock(&mutex);receiver->reset();resetSampling();rx.beginReceive();canvas.fill(Qt::black);sender.clear();newImage=true;receiveAborted=false;lastBlock.invalidate();cleared=canvas;epoch=++rxEpoch;}
  emit imageChanged(cleared,0,false,epoch);emit statusChanged("KG-STV receive image cleared.");
}
void Controller::leaveMode() {
  {QMutexLocker lock(&mutex);cancelLocked();tx.reset();receiver->reset();resetSampling();}
}
bool Controller::idle(QString *error) const {
  if(preparing || active || !plan.isEmpty())return fail(error,"A KG-STV transmission is already prepared or active.");
  return true;
}
bool Controller::identification(const QString &callsign,const Options &options,QVector<Packet> *packets,QString *error) const {
  if(callsign.isEmpty())return true;
  if(callsign.size()>32)return fail(error,"KG-STV identification is limited to 32 ASCII characters.");
  for(QChar c:callsign)if(c.unicode()<32 || c.unicode()>126)return fail(error,"KG-STV identification currently supports printable ASCII only.");
  Packet packet;packet.header.type=Type::Callsign;packet.header.fec=options.fec;packet.header.fourLevel=options.fourLevel;
  packet.payload=byteBits(callsign.toLatin1());packet.header.payloadBits=packet.payload.size();packets->append(packet);return true;
}
Packet Controller::endPacket(const Options &options) const {
  Packet packet;packet.header.type=Type::EndPass;packet.header.x=20;packet.header.y=15;packet.header.quality=options.quality;
  packet.header.fec=options.fec;packet.header.fourLevel=options.fourLevel;return packet;
}
bool Controller::prepareImageJob(const QImage &image,const QString &callsign,QString *error) {
  if(error)error->clear();
  Options options;quint64 token;
  {QMutexLocker lock(&mutex);if(!idle(error))return false;preparing=true;options=settings;token=generation;}
  QVector<Packet> output,blocks;QString problem;
  const bool ok=identification(callsign,options,&output,&problem) && prepareImage(image,options.quality,options.fec,options.fourLevel,&blocks,&problem);
  {QMutexLocker lock(&mutex);
    if(token!=generation)return fail(error,"KG-STV preparation was canceled.");
    preparing=false;
    if(!ok)return fail(error,problem);
    if(!tx.prepareTransmit(blocks,error))return false;
    output+=blocks;output.append(endPacket(options));plan=output;job=Job::Image;
  }
  emit statusChanged("KG-STV image prepared (experimental).");return true;
}
bool Controller::prepareTextJob(const QString &text,const QString &callsign,QString *error) {
  if(error)error->clear();
  {QMutexLocker lock(&mutex);if(!idle(error))return false;
    if(text.trimmed().isEmpty() || text.size()>200)return fail(error,"KG-STV text must contain 1..200 ASCII bytes.");
    for(QChar c:text)if(!((c.unicode()>=32 && c.unicode()<=126) || c=='\n' || c=='\r' || c=='\t'))
      return fail(error,"KG-STV text currently supports ASCII only; unsupported characters were not replaced.");
    QVector<Packet> output;if(!identification(callsign,settings,&output,error))return false;
    Packet packet;packet.header.type=Type::Text;packet.header.fec=settings.fec;packet.header.fourLevel=settings.fourLevel;
    packet.payload=byteBits(text.toLatin1());packet.header.payloadBits=packet.payload.size();output.append(packet);
    plan=output;job=Job::Text;sentText=text;
  }
  emit statusChanged("KG-STV text prepared.");return true;
}
bool Controller::prepareRequestJob(const QString &callsign,QString *error) {
  if(error)error->clear();
  {QMutexLocker lock(&mutex);if(!idle(error))return false;
    if(receiveAborted)return fail(error,"KG-STV reception was aborted; wait for a new image pass.");
    const auto requests=rx.missingBlockRequests(settings.fec,settings.fourLevel);
    if(requests.isEmpty())return fail(error,"No missing KG-STV blocks after a completed receive pass.");
    QVector<Packet> output;if(!identification(callsign,settings,&output,error))return false;
    output+=requests;plan=output;job=Job::Request;
  }
  emit statusChanged("KG-STV missing-block requests prepared.");return true;
}
bool Controller::prepareRepairJob(const QString &callsign,QString *error) {
  if(error)error->clear();
  {QMutexLocker lock(&mutex);if(!idle(error))return false;
    if(tx.pendingRequests()==0)return fail(error,"No KG-STV requests for the cached transmit image.");
    QVector<Packet> output;if(!identification(callsign,settings,&output,error))return false;
    reservedRepairs=tx.takeRepairs();
    for(Packet packet:reservedRepairs){packet.header.fec=settings.fec;packet.header.fourLevel=settings.fourLevel;output.append(packet);}
    output.append(endPacket(settings));plan=output;job=Job::Repair;
  }
  emit statusChanged("KG-STV requested repairs prepared; cached image will be used.");return true;
}
bool Controller::prepared() const {QMutexLocker lock(&mutex);return !preparing && !active && !plan.isEmpty();}
bool Controller::busy() const {QMutexLocker lock(&mutex);return preparing || active || !plan.isEmpty();}
bool Controller::beginTransmit(QString *error,double sampleClock) {
  if(error)error->clear();
  QMutexLocker lock(&mutex);
  if(preparing || active || plan.isEmpty())return fail(error,"No prepared KG-STV transmission.");
  transmitter.reset(new AudioTransmitter(plan.first().header.fourLevel,48000));
  if(!transmitter->setSampleClock(sampleClock,error) || !transmitter->setPacket(plan.first(),true,error)){transmitter.reset();return false;}
  nextPacket=1;active=true;rendered=false;return true;
}
int Controller::render(double *samples,int capacity,QString *error) {
  if(error)error->clear();
  QMutexLocker lock(&mutex);
  if(!active || !transmitter || !samples || capacity<=0)return 0;
  int count=0;
  while(count<capacity) {
    count+=transmitter->render(samples+count,capacity-count);
    if(transmitter->active())break;
    if(nextPacket==plan.size()){rendered=true;break;}
    if(!transmitter->setPacket(plan[nextPacket++],false,error))break;
  }
  return count;
}
void Controller::completeTransmit(bool success) {
  QString status,text;
  {QMutexLocker lock(&mutex);
    text=sentText;
    success=success && active && rendered;
    if(success && job==Job::Image)tx.finishTransmitPass();
    if(success) {
      status=job==Job::Text?"KG-STV text sent locally; remote delivery is not acknowledged.":"KG-STV transmission finished locally.";
      reservedRepairs.clear();plan.clear();job=Job::None;sentText.clear();transmitter.reset();active=false;rendered=false;
    } else {cancelLocked();status="KG-STV transmission canceled or failed.";}
  }
  emit statusChanged(status);
  if(!text.isEmpty())emit localTextResult(text,success);
  emit transmitFinished(success);
}
void Controller::cancelLocked() {
  if(job==Job::Image)tx.reset();
  if(job==Job::Repair)for(Packet packet:reservedRepairs) {
    packet.header.type=Type::Request;packet.header.quality=0;packet.header.payloadBits=0;packet.payload.clear();tx.acceptRequest(packet);
  }
  ++generation;reservedRepairs.clear();plan.clear();sentText.clear();job=Job::None;active=false;rendered=false;preparing=false;transmitter.reset();
}
void Controller::cancelTransmit(){QString text;{QMutexLocker lock(&mutex);text=sentText;cancelLocked();}if(!text.isEmpty())emit localTextResult(text,false);}
double Controller::duration() const {
  QMutexLocker lock(&mutex);double symbols=0;
  for(int i=0;i<plan.size();++i){Bits bits;encodePacket(plan[i],i==0,&bits);symbols+=double(bits.size())/(plan[i].header.fourLevel?2:1);}
  return symbols/1200.0;
}
int Controller::receivedCount() const {QMutexLocker lock(&mutex);return rx.receivedBlocks();}
bool Controller::receiving() const {QMutexLocker lock(&mutex);return lastBlock.isValid() && lastBlock.elapsed()<15000 && !rx.receivePassFinished();}
quint64 Controller::receiveEpoch() const {QMutexLocker lock(&mutex);return rxEpoch;}
int Controller::pendingRepairs() const {QMutexLocker lock(&mutex);return tx.pendingRequests();}
QImage Controller::receivedImage() const {QMutexLocker lock(&mutex);return canvas;}
bool Controller::receive(const double *samples,int count,QString *error) {
  if(error)error->clear();
  QImage update;int received=0;bool complete=false;quint64 epoch;
  if(count<0 || count>65536 || (!samples && count))return fail(error,"Invalid KG-STV input samples.");
  for(int i=0;i<count;++i)if(!std::isfinite(samples[i]) || std::abs(samples[i])>1000000)return fail(error,"Invalid KG-STV input amplitude.");
  QVector<QPair<QString,QString>> messages;QStringList statuses;
  {QMutexLocker lock(&mutex);
    if(active)return true; // Never decode the local transmitter through the RX path.
    QVector<double> corrected;corrected.reserve(count+8);
    for(int i=0;i<count;++i) {
      const double sample=samples[i];
      if(!previousSampleValid){corrected.append(sample);previousSample=sample;previousSampleValid=true;nextSample=inputStep;continue;}
      while(nextSample<=1.0){corrected.append(previousSample+(sample-previousSample)*nextSample);nextSample+=inputStep;}
      nextSample-=1.0;previousSample=sample;
    }
    QVector<Packet> packets;
    for(int at=0;at<corrected.size();at+=65536)if(!receiver->feed(corrected.constData()+at,qMin<int>(65536,corrected.size()-at),&packets,error))return false;
    for(const Packet &packet:packets) {
      const Type type=packet.header.type;
      if(type==Type::Callsign) {
        const QString identification=displayText(packet.payload);
        if(sender!=identification){sender=identification;newImage=true;}
        statuses.append("KG-STV identification: "+sender);
      } else if(type==Type::Text)messages.append(qMakePair(sender,displayText(packet.payload)));
      else if(type==Type::Image || type==Type::Repair) {
        if(type==Type::Repair && receiveAborted)continue;
        if(type==Type::Image && (newImage || rx.receivePassFinished())) {
          rx.beginReceive();canvas.fill(Qt::black);newImage=false;receiveAborted=false;++rxEpoch;
        }
        if(!rx.receivedPayload(packet.header.x,packet.header.y).isEmpty())continue;
        QString problem;
        if(paintImageBlock(packet,&canvas,&problem) && rx.receiveBlock(packet,&problem)){update=canvas;lastBlock.start();}
        else statuses.append("KG-STV block rejected: "+problem);
      } else if(type==Type::EndPass) {
        if(receiveAborted)continue;
        rx.endReceivePass(packet);
        statuses.append(QString("KG-STV receive pass finished: %1/300 blocks.").arg(rx.receivedBlocks()));
      } else if(type==Type::Request) {
        QString problem;
        if(tx.acceptRequest(packet,&problem))statuses.append(QString("KG-STV repair requests: %1 (manual response).").arg(tx.pendingRequests()));
      } else if(type==Type::Abort) {
        receiver->reset();resetSampling();newImage=true;receiveAborted=true;lastBlock.invalidate();tx.takeRepairs();statuses.append("KG-STV remote abort received; pending requests cleared.");
      }
    }
    received=rx.receivedBlocks();complete=rx.receiveComplete();epoch=rxEpoch;
  }
  if(!update.isNull())emit imageChanged(update,received,complete,epoch);
  for(const auto &message:messages)emit messageReceived(message.first,message.second);
  for(const QString &status:statuses)emit statusChanged(status);
  return true;
}
}
