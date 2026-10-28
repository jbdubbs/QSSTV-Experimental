// Experimental offline KG-STV signal/fixture tool. No audio devices or radio/PTT access.
#include "kgstvaudio.h"
#include "kgstvimage.h"
#include "kgstvtransfer.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryFile>
#include <QTextStream>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <stdexcept>
using namespace Kgstv;
namespace {
[[noreturn]] void failure(const QString &message){throw std::runtime_error(message.toStdString());}
void require(bool condition,const QString &message){if(!condition)failure(message);}
void available(const QString &path){const QFileInfo entry(path);require(!entry.exists() && !entry.isSymLink(),"Output already exists: "+path);}
void put16(QByteArray &bytes,int at,quint16 value){qToLittleEndian(value,reinterpret_cast<uchar *>(bytes.data()+at));}
void put32(QByteArray &bytes,int at,quint32 value){qToLittleEndian(value,reinterpret_cast<uchar *>(bytes.data()+at));}
quint16 get16(const QByteArray &bytes,int at){return qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(bytes.constData()+at));}
quint32 get32(const QByteArray &bytes,int at){return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(bytes.constData()+at));}
QString temporaryName(const QString &path){return QDir(QFileInfo(path).absolutePath()).filePath(".kgstv-offline-XXXXXX");}
void publish(QTemporaryFile &file,const QString &path) {
  require(file.flush(),"Could not flush output: "+path);file.close();
  require(QFile::rename(file.fileName(),path),"Could not publish output without replacing an existing file: "+path);
  file.setAutoRemove(false);
}
void writeWave(const QString &path,const QVector<Packet> &packets,bool fourLevel) {
  available(path);require(!packets.isEmpty(),"No KG-STV packets to transmit.");
  QTemporaryFile file(temporaryName(path));require(file.open(),"Cannot create WAV output: "+path);
  require(file.write(QByteArray(44,0))==44,"Cannot write WAV header.");
  AudioTransmitter transmitter(fourLevel,12000);double samples[4096];quint64 dataBytes=0;QString error;
  for(int i=0;i<packets.size();++i) {
    require(transmitter.setPacket(packets[i],i==0,&error),error);
    while(transmitter.active()) {
      const int count=transmitter.render(samples,4096);require(count>0,"PCM transmitter stalled.");
      QByteArray pcm(count*2,0);
      for(int j=0;j<count;++j)put16(pcm,j*2,quint16(qint16(std::lround(qBound(-1.0,samples[j],1.0)*32767))));
      require(file.write(pcm)==pcm.size(),"Cannot write WAV samples.");dataBytes+=quint64(pcm.size());
      require(dataBytes<0x7fffffffU,"KG-STV WAV output is too large.");
    }
  }
  QByteArray header(44,0);header.replace(0,4,"RIFF");put32(header,4,quint32(dataBytes+36));header.replace(8,4,"WAVE");
  header.replace(12,4,"fmt ");put32(header,16,16);put16(header,20,1);put16(header,22,1);put32(header,24,12000);
  put32(header,28,24000);put16(header,32,2);put16(header,34,16);header.replace(36,4,"data");put32(header,40,quint32(dataBytes));
  require(file.seek(0) && file.write(header)==header.size(),"Cannot finalize WAV header.");publish(file,path);
}
struct Wave {qint64 offset=0,bytes=0;int rate=0;};
Wave readWaveHeader(QFile &file) {
  require(file.size()>=44 && file.size()<0x40000000LL,"Unsupported or truncated WAV size.");
  const QByteArray header=file.read(12);
  require(header.size()==12,"Cannot read complete RIFF header.");
  require(header.left(4)=="RIFF" && header.mid(8,4)=="WAVE","Expected RIFF/WAVE input.");
  const qint64 end=qint64(get32(header,4))+8;require(end==file.size(),"RIFF length does not match WAV file size.");
  Wave wave;bool formatSeen=false,dataSeen=false;
  while(file.pos()<end) {
    require(end-file.pos()>=8,"Truncated WAV chunk header.");
    const QByteArray chunk=file.read(8);require(chunk.size()==8,"Cannot read complete WAV chunk header.");const quint32 size=get32(chunk,4);
    const qint64 begin=file.pos();require(qint64(size)<=end-begin,"WAV chunk exceeds RIFF boundary.");
    if(chunk.left(4)=="fmt ") {
      require(!formatSeen && size>=16 && size<=4096,"Unsupported or duplicate WAV format chunk.");
      const QByteArray format=file.read(size);require(format.size()==int(size),"Cannot read complete WAV format chunk.");
      require(get16(format,0)==1 && get16(format,2)==1 && get16(format,12)==2 && get16(format,14)==16,
              "Only mono, 16-bit signed integer PCM WAV input is supported.");
      const quint32 rate=get32(format,4);
      require(rate>=12000 && rate<=48000 && rate%1200==0 && get32(format,8)==rate*2,"Unsupported PCM sample rate or byte rate.");
      wave.rate=int(rate);formatSeen=true;
    } else if(chunk.left(4)=="data") {
      require(!dataSeen && size%2==0,"Duplicate or unaligned WAV data chunk.");
      wave.offset=begin;wave.bytes=size;dataSeen=true;
    }
    const qint64 next=begin+size+(size%2);require(next<=end && file.seek(next),"Invalid WAV chunk padding.");
  }
  require(formatSeen && dataSeen && wave.bytes>0,"WAV requires PCM format and non-empty sample data.");
  return wave;
}
template<class Consumer> void readPackets(const QString &path,bool fourLevel,Consumer consumer) {
  QFile file(path);require(file.open(QIODevice::ReadOnly),"Cannot read WAV: "+path);
  const Wave wave=readWaveHeader(file);require(file.seek(wave.offset),"Cannot seek WAV samples.");
  AudioReceiver receiver(fourLevel,wave.rate);qint64 remaining=wave.bytes;QString error;
  while(remaining) {
    const int count=int(qMin<qint64>(remaining,8192));const QByteArray pcm=file.read(count);require(pcm.size()==count,"Truncated WAV sample data.");
    QVector<double> samples(count/2);
    for(int i=0;i<samples.size();++i)samples[i]=qint16(get16(pcm,i*2))/32768.0;
    QVector<Packet> packets;require(receiver.feed(samples.constData(),samples.size(),&packets,&error),error);
    for(const Packet &packet:packets)consumer(packet);
    remaining-=count;
  }
}
void saveText(const QString &text,const QString &path) {
  available(path);QTemporaryFile file(temporaryName(path));require(file.open(),"Cannot create text output: "+path);
  const QByteArray bytes=text.toUtf8();require(file.write(bytes)==bytes.size(),"Cannot write text output.");publish(file,path);
}
void saveImage(const QImage &image,const QString &path) {
  available(path);QTemporaryFile file(temporaryName(path));require(file.open(),"Cannot create image output: "+path);
  require(image.save(&file,"PNG"),"Cannot encode PNG output.");publish(file,path);
}
QString safeText(const Bits &bits) {
  QByteArray bytes;require(bitBytes(bits,&bytes),"Text packet is not byte aligned.");QString text;
  for(unsigned char byte:bytes) {
    if((byte>=32 && byte<=126) || byte=='\n' || byte=='\r' || byte=='\t')text+=QChar(byte);
    else text+=QString("\\x%1").arg(byte,2,16,QChar('0'));
  }
  return text;
}
Packet identification(const QString &callsign,bool fec,bool fourLevel) {
  require(callsign.size()<=32,"Callsign exceeds the conservative 32-byte tool limit.");
  for(QChar character:callsign)require(character.unicode()>=32 && character.unicode()<=126,"Only printable ASCII identification is validated by this tool.");
  Packet packet;packet.header.type=Type::Callsign;packet.header.fec=fec;packet.header.fourLevel=fourLevel;
  packet.payload=byteBits(callsign.toLatin1());packet.header.payloadBits=packet.payload.size();return packet;
}
}
int main(int argc,char **argv) {
  QCoreApplication app(argc,argv);app.setApplicationName("kgstv-offline");app.setApplicationVersion("9.6.0-experimental");
  QCommandLineParser parser;parser.setApplicationDescription("Experimental KG-STV offline fixtures. Synthetic compatibility only; no devices, CAT or PTT.");
  parser.addHelpOption();parser.addVersionOption();
  parser.addOption({"four-level","Use experimental 4L-FSK instead of MSK framing."});
  parser.addOption({"no-fec","Disable payload convolutional coding for generated signals."});
  parser.addOption({"quality","Initial quantization index 0..15 (larger means stronger compression).","index","8"});
  parser.addOption({"callsign","Include printable ASCII identification in generated image audio.","callsign"});
  parser.addOption({"append","Decode an additional repair/message WAV after the main input (repeatable).","path"});
  parser.addOption({"bsr-out","Write requests for missing blocks to this WAV after decoding.","path"});
  parser.addPositionalArgument("command","encode, decode, repair, text, or decode-text");
  parser.addPositionalArgument("files","encode: IMAGE WAV; decode: WAV PNG; repair: ORIGINAL_IMAGE REQUESTS_WAV REPAIRS_WAV; text: ASCII_TEXT_FILE WAV; decode-text: WAV TXT","files...");
  parser.process(app);
  try {
    const QStringList args=parser.positionalArguments();require(!args.isEmpty(),"Specify encode, decode or repair; see --help.");
    const QString command=args[0];const bool four=parser.isSet("four-level"),fec=!parser.isSet("no-fec");
    bool numeric=false;const int quality=parser.value("quality").toInt(&numeric);require(numeric && quality>=0 && quality<=15,"Quality index must be 0..15.");
    require(command=="encode" || command=="decode" || command=="repair" || command=="text" || command=="decode-text","Unknown KG-STV offline command.");
    require(args.size()==(command=="repair"?4:3),"Wrong number of filenames; see --help.");
    require(!parser.isSet("bsr-out") || command=="decode","--bsr-out requires image decoding.");
    require(!parser.isSet("append") || command=="decode" || command=="decode-text","--append requires decoding.");
    if(command=="text") {
      available(args[2]);QFile text(args[1]);require(text.open(QIODevice::ReadOnly),"Cannot read text input.");
      require(text.size()>0 && text.size()<=200,"Standalone text is limited to 1..200 ASCII bytes by this experimental tool.");
      const QByteArray bytes=text.readAll();require(bytes.size()>0 && bytes.size()<=200,"Text input changed or could not be read completely.");
      require(!bytes.trimmed().isEmpty(),"Text message is empty.");
      for(unsigned char byte:bytes)require((byte>=32 && byte<=126) || byte=='\n' || byte=='\r' || byte=='\t',"Only printable ASCII, newline and tab are currently supported for transmission.");
      QVector<Packet> packets;
      if(parser.isSet("callsign"))packets.append(identification(parser.value("callsign"),fec,four));
      Packet packet;packet.header.type=Type::Text;packet.header.fec=fec;packet.header.fourLevel=four;
      packet.payload=byteBits(bytes);packet.header.payloadBits=packet.payload.size();packets.append(packet);
      writeWave(args[2],packets,four);QTextStream(stdout)<<"Generated experimental standalone-text WAV: "<<args[2]<<'\n';return 0;
    }
    if(command=="decode" || command=="decode-text") {
      available(args[2]);if(parser.isSet("bsr-out"))available(parser.value("bsr-out"));
      QImage canvas(320,240,QImage::Format_RGB32);canvas.fill(Qt::black);BlockTransfer receiver;receiver.beginReceive();
      int invalidBlocks=0,messages=0;QString error,receivedText;
      auto consume=[&](const Packet &packet) {
        if(packet.header.type==Type::Image || packet.header.type==Type::Repair) {
          if(!receiver.receivedPayload(packet.header.x,packet.header.y).isEmpty())return;
          if(paintImageBlock(packet,&canvas,&error))require(receiver.receiveBlock(packet,&error),error);else ++invalidBlocks;
        } else if(packet.header.type==Type::EndPass)require(receiver.endReceivePass(packet,&error),error);
        else if(packet.header.type==Type::Abort)failure("KG-STV abort packet received; no output saved.");
        else if(packet.header.type==Type::Text || packet.header.type==Type::Callsign) {
          const QString line=(packet.header.type==Type::Text?"Text: ":"Identification: ")+safeText(packet.payload)+"\n";
          receivedText+=line;QTextStream(stdout)<<line;
          if(packet.header.type==Type::Text)++messages;
        }
      };
      readPackets(args[1],four,consume);
      for(const QString &additional:parser.values("append"))readPackets(additional,four,consume);
      if(command=="decode-text") {require(messages>0,"No standalone KG-STV text messages decoded.");saveText(receivedText,args[2]);return 0;}
      require(receiver.receivedBlocks()>0,"No valid KG-STV image blocks decoded. Check modulation and reference compatibility.");
      const auto missing=receiver.missingBlockRequests(fec,four);
      if(parser.isSet("bsr-out") && !receiver.receiveComplete()) {
        require(receiver.receivePassFinished(),"Cannot generate BSR before a valid end-of-pass packet.");
        writeWave(parser.value("bsr-out"),missing,four);
      }
      saveImage(canvas,args[2]);
      QTextStream(stdout)<<"Received "<<receiver.receivedBlocks()<<"/300 image blocks; invalid coefficient blocks: "<<invalidBlocks<<"; PNG: "<<args[2]<<'\n';
      return receiver.receiveComplete()?0:3;
    }
    available(args.last());const QImage image(args[1]);QVector<Packet> blocks;QString error;
    require(prepareImage(image,quality,fec,four,&blocks,&error),error);
    QVector<Packet> output;
    if(parser.isSet("callsign"))output.append(identification(parser.value("callsign"),fec,four));
    if(command=="encode")output+=blocks;
    else {
      BlockTransfer sender;require(sender.prepareTransmit(blocks,&error),error);require(sender.finishTransmitPass(&error),error);
      readPackets(args[2],four,[&](const Packet &packet) {
        if(packet.header.type==Type::Request)require(sender.acceptRequest(packet,&error),error);
      });
      const auto repairs=sender.takeRepairs();require(!repairs.isEmpty(),"No valid BSR requests decoded.");output+=repairs;
    }
    Packet end;end.header.type=Type::EndPass;end.header.x=20;end.header.y=15;end.header.quality=quality;
    end.header.fec=fec;end.header.fourLevel=four;output.append(end);
    writeWave(args.last(),output,four);
    QTextStream(stdout)<<"Generated "<<output.size()<<" packets; 12000 Hz mono PCM WAV: "<<args.last()<<"\nExperimental waveform; original KG-STV interoperability is not established.\n";
  }catch(const std::exception &error){QTextStream(stderr)<<"Error: "<<error.what()<<'\n';return 1;}
  return 0;
}
