// Optional external-reference regression; no devices and no bundled recording.
#include "kgstvcontroller.h"
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QtEndian>
#include <stdexcept>
using namespace Kgstv;
#define CHECK(c) do {if(!(c))throw std::runtime_error(#c);}while(false)
int main(int argc,char **argv) {
 QCoreApplication app(argc,argv);
 try {
  CHECK(argc==4);const bool textReference=QString::fromLatin1(argv[3])=="text";CHECK(textReference || QString::fromLatin1(argv[3])=="image");QFile file(argv[1]);CHECK(file.open(QIODevice::ReadOnly));
  CHECK(file.size()>=44 && file.size()<10000000);const QByteArray wave=file.readAll();
  CHECK(wave.left(4)=="RIFF" && wave.mid(8,4)=="WAVE");
  CHECK(qFromLittleEndian<quint32>((const uchar*)wave.constData()+4)==quint32(wave.size()-8));
  QByteArray pcm;bool format=false;int at=12;
  while(at+8<=wave.size()) {
   const quint32 size=qFromLittleEndian<quint32>((const uchar*)wave.constData()+at+4);
   CHECK(size<=quint32(wave.size()-at-8));
   if(wave.mid(at,4)=="fmt ") {
    CHECK(!format && size>=16);const uchar *p=(const uchar*)wave.constData()+at+8;
    CHECK(qFromLittleEndian<quint16>(p)==1 && qFromLittleEndian<quint16>(p+2)==1);
    CHECK(qFromLittleEndian<quint32>(p+4)==12000 && qFromLittleEndian<quint16>(p+14)==16);
    CHECK(qFromLittleEndian<quint16>(p+12)==2);format=true;
   }
   if(wave.mid(at,4)=="data"){CHECK(pcm.isEmpty());pcm=wave.mid(at+8,size);}
   at+=8+size+(size%2);
  }
  CHECK(at==wave.size() && format && !pcm.isEmpty() && pcm.size()%2==0);
  Controller receiver;Options options;options.fourLevel=true;CHECK(receiver.configure(options));
  QStringList messages,senders;
  QObject::connect(&receiver,&Controller::messageReceived,&app,[&](QString sender,QString text){senders.append(sender);messages.append(text);});
  for(int i=0;i<pcm.size()/2;i+=4096) {
   QVector<double> samples;
   for(int j=i;j<qMin(i+4096,pcm.size()/2);++j)samples.append(qint16(qFromLittleEndian<quint16>((const uchar*)pcm.constData()+2*j))/32768.0);
   CHECK(receiver.receive(samples.constData(),samples.size()));
  }
  if(textReference) {
    const QString expected="TEST3>The Quick Brown Fox Jumps Over The Lazy Dog 0123456789";
    CHECK(messages==QStringList{expected});CHECK(senders==QStringList{QString()});
    CHECK(receiver.receivedCount()==0 && !receiver.receiving());
    QFile output(argv[2]);CHECK(!output.exists() && output.open(QIODevice::WriteOnly|QIODevice::NewOnly));
    const QByteArray bytes=(messages.join("\n")+"\n").toUtf8();CHECK(output.write(bytes)==bytes.size());
    QTextStream(stdout)<<"PASS: external 4L-FSK text through live controller: exactly one unchanged TEST3> pangram, no fabricated sender/image and no devices.\n";
    return 0;
  }
  CHECK(messages.isEmpty());
  CHECK(receiver.receivedCount()==300);CHECK(!receiver.receiving());
  const QImage image=receiver.receivedImage();
  // External BBC test-card top bars: yellow, cyan, red, blue.
  const QColor yellow=image.pixelColor(30,5),cyan=image.pixelColor(70,5);
  CHECK(yellow.red()>180 && yellow.green()>180 && yellow.blue()<80);
  CHECK(cyan.red()<80 && cyan.green()>180 && cyan.blue()>180);
  CHECK(!QFile::exists(argv[2]) && image.save(argv[2],"PNG"));
  QTextStream(stdout)<<"PASS: external 4L-FSK reference through live controller: 300/300 blocks, end pass and BGR test-card colors; no devices.\n";
 }catch(const std::exception &e){QTextStream(stderr)<<"FAIL: "<<e.what()<<'\n';return 1;}
}
