// Device-free tests of the live controller. PCM is looped between independent peers.
#include "kgstvcontroller.h"
#include <QCoreApplication>
#include <QTextStream>
#include <stdexcept>
using namespace Kgstv;
#define CHECK(c) do {if(!(c))throw std::runtime_error(QString("line %1: %2").arg(__LINE__).arg(#c).toStdString());}while(false)
static void transfer(Controller &source,Controller &destination,bool erase=false,double clock=48000.0,bool changeTxOptions=false)
{
  QString error;CHECK(source.prepared());CHECK(source.beginTransmit(&error,clock));CHECK(!source.beginTransmit(&error));
  quint64 position=0;int decimation=0;
  for(;;) {
    double pcm[509];const int count=source.render(pcm,509,&error);CHECK(error.isEmpty());if(!count)break;
    CHECK(source.receive(pcm,count,&error)); // Own transmitter must not be decoded as RX.
    QVector<double> samples;
    for(int i=0;i<count;++i,++position)if(++decimation==4) {
      decimation=0;samples.append(erase && position>=48000 && position<96000?0.0:pcm[i]);
    }
    CHECK(destination.receive(samples.constData(),samples.size(),&error));
    if(changeTxOptions && position>48000) {auto options=destination.options();options.quality=(options.quality+1)%16;options.fec=!options.fec;
      CHECK(destination.configure(options,&error));changeTxOptions=false;}

  }
  const double silence[64]={};CHECK(destination.receive(silence,64,&error));
  source.completeTransmit(true);CHECK(!source.busy());
}
static void deliver(const Packet &packet,Controller &destination)
{
  AudioTransmitter source(packet.header.fourLevel,48000);CHECK(source.setPacket(packet,true));
  while(source.active()){double pcm[1024];const int count=source.render(pcm,1024);QVector<double> samples;
    for(int i=3;i<count;i+=4)samples.append(pcm[i]);
    CHECK(destination.receive(samples.constData(),samples.size()));
  }
}
int main(int argc,char **argv)
{
  QCoreApplication app(argc,argv);
  try {
    Controller sender,receiver;QString error;int messages=0,sent=0,failed=0,images=0;QString text,identity;
    QObject::connect(&sender,&Controller::localTextResult,&app,[&](QString,bool success){if(success)++sent;else ++failed;});
    QObject::connect(&receiver,&Controller::messageReceived,&app,[&](QString from,QString value){++messages;identity=from;text=value;CHECK(receiver.receivedCount()>=0);});
    QObject::connect(&receiver,&Controller::imageChanged,&app,[&](QImage image,int count,bool complete,quint64 epoch){++images;CHECK(image.size()==QSize(320,240));CHECK(receiver.receiveEpoch()==epoch);CHECK(!complete || count==300);});
    for(bool four:{false,true}) {
      Options options;options.fourLevel=four;CHECK(sender.configure(options,&error));CHECK(receiver.configure(options,&error));
      CHECK(sender.prepareTextJob("CQ DE OE6PLD\nStandalone test.","OE6PLD",&error));
      CHECK(sender.duration()>0 && !sender.prepareRequestJob("OE6PLD",&error));
      CHECK(!sender.configure(options,&error));transfer(sender,receiver);
      CHECK(text=="CQ DE OE6PLD\nStandalone test." && identity=="OE6PLD");
    }
    CHECK(messages==2 && sent==2 && sender.receivedCount()==0);
    for(bool four:{false,true})for(double clock:{47952.0,48048.0}) {
      auto options=sender.options();options.fourLevel=four;CHECK(sender.configure(options,&error));CHECK(receiver.configure(options,&error));
      CHECK(receiver.resetDemodulator(clock,&error));
      CHECK(sender.prepareTextJob(QString(200,'C'),"OE6PLD",&error));transfer(sender,receiver,false,clock);
      CHECK(text==QString(200,'C'));
    }
    CHECK(!receiver.resetDemodulator(0,&error));CHECK(receiver.resetDemodulator(48000,&error));
    CHECK(sender.prepareTextJob(QString(200,'P'),"OE6PLD",&error));transfer(sender,receiver,false,48000,true);CHECK(text==QString(200,'P'));


    CHECK(!sender.prepareTextJob(QString::fromUtf8("Grüße"),"OE6PLD",&error));CHECK(!sender.busy());
    CHECK(!sender.prepareTextJob(QString(201,'A'),"OE6PLD",&error));CHECK(!sender.prepareTextJob(" ","OE6PLD",&error));
    CHECK(sender.prepareTextJob("Cancel this message","OE6PLD",&error));sender.cancelTransmit();CHECK(failed==1 && !sender.busy());
    CHECK(!sender.beginTransmit(&error));
    CHECK(sender.prepareTextJob("Cancel during PCM","OE6PLD",&error));CHECK(sender.beginTransmit(&error));
    double samples[512];CHECK(sender.render(samples,512,&error)>0);sender.cancelTransmit();
    CHECK(sender.render(samples,512,&error)==0 && failed==2);sender.completeTransmit(false);CHECK(failed==2);
    Options options;CHECK(sender.configure(options,&error));CHECK(receiver.configure(options,&error));receiver.clearReceive();
    CHECK(!receiver.prepareRequestJob("OE6PLD",&error));
    QImage image(320,240,QImage::Format_RGB32);
    for(int y=0;y<240;++y)for(int x=0;x<320;++x)image.setPixel(x,y,qRgb(x*255/319,y*255/239,128));
    QVector<Packet> compressed;CHECK(prepareImage(image,8,true,false,&compressed,&error));
    QImage expected(320,240,QImage::Format_RGB32);expected.fill(Qt::black);
    for(const auto &packet:compressed)CHECK(paintImageBlock(packet,&expected,&error));
    CHECK(sender.prepareImageJob(image,"OE6PLD",&error));
    CHECK(!sender.prepareTextJob("Cannot overlap","OE6PLD",&error));
    transfer(sender,receiver,true);CHECK(receiver.receivedCount()>0 && receiver.receivedCount()<300);
    CHECK(!receiver.receiving());CHECK(receiver.prepareRequestJob("OE6XYZ",&error));
    transfer(receiver,sender);CHECK(sender.pendingRepairs()==300-receiver.receivedCount());
    const int pending=sender.pendingRepairs();CHECK(sender.prepareRepairJob("OE6PLD",&error));
    sender.cancelTransmit();CHECK(sender.pendingRepairs()==pending);
    CHECK(sender.prepareRepairJob("OE6PLD",&error));transfer(sender,receiver);
    CHECK(receiver.receivedCount()==300 && receiver.receivedImage()==expected && images>1);
    CHECK(!receiver.prepareRequestJob("OE6XYZ",&error));
    CHECK(!sender.prepareRepairJob("OE6PLD",&error));
    const quint64 old=receiver.receiveEpoch();receiver.clearReceive();CHECK(receiver.receiveEpoch()!=old && receiver.receivedCount()==0);
    CHECK(sender.prepareImageJob(image,"OE6PLD",&error));CHECK(sender.beginTransmit(&error));sender.cancelTransmit();
    CHECK(!sender.busy() && sender.pendingRepairs()==0);sender.leaveMode();receiver.leaveMode();
    CHECK(!sender.prepareImageJob(QImage(),"OE6PLD",&error) && !sender.busy());
    receiver.clearReceive();deliver(compressed[0],receiver);CHECK(receiver.receivedCount()==1 && receiver.receiving());
    CHECK(receiver.resetDemodulator());CHECK(!receiver.receiving() && receiver.receivedCount()==1);
    Packet abort;abort.header.type=Type::Abort;deliver(abort,receiver);CHECK(!receiver.receiving());
    Packet end;end.header.type=Type::EndPass;end.header.x=20;end.header.y=15;deliver(end,receiver);
    CHECK(!receiver.prepareRequestJob("OE6PLD",&error));
    deliver(compressed[0],receiver);CHECK(receiver.receivedCount()==1 && receiver.receiving());
    deliver(end,receiver);CHECK(receiver.prepareRequestJob("OE6PLD",&error));receiver.cancelTransmit();

    QTextStream(stdout)<<"PASS: KG-STV controller standalone text, exclusive jobs, cancel/local status, queued generation metadata, independent-peer 48-to-12 kHz PCM image RX/TX, erased audio, BSR, canceled repair restoration and exact image recovery. No devices/PTT or original signals tested.\n";
  }catch(const std::exception &error){QTextStream(stderr)<<"FAIL: "<<error.what()<<'\n';return 1;}
}
