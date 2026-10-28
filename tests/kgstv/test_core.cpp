// Native protocol/BSR tests. Synthetic vectors are not interoperability evidence.
#include "kgstvprotocol.h"
#include "kgstvtransfer.h"
#include "kgstvaudio.h"
#include "kgstvimage.h"
#include <limits>
#include <QCoreApplication>
#include <QTextStream>
#include <QRandomGenerator>
#include <stdexcept>
using namespace Kgstv;
#define CHECK(c) do{if(!(c))throw std::runtime_error(QString("line %1: %2").arg(__LINE__).arg(#c).toStdString());}while(false)
static Bits literal(const char *text){Bits result;while(*text)result.append(char(*text++-'0'));return result;}
static Bits inverse(Bits bits){for(char &bit:bits)bit^=1;return bits;}
static Packet block(int x,int y,bool fec=false,bool four=false) {
  Packet packet;packet.header.x=x;packet.header.y=y;packet.header.quality=(x+y)%16;
  packet.header.fec=fec;packet.header.fourLevel=four;
  packet.payload=byteBits(QByteArray(1,char(x+y*20)));packet.header.payloadBits=packet.payload.size();return packet;
}
static Packet roundTrip(const Packet &packet,bool preamble=true) {
  QString error;Bits wire;Packet result;
  CHECK(encodePacket(packet,preamble,&wire,&error));CHECK(decodePacket(wire,&result,&error));
  CHECK(result.header.type==packet.header.type && result.header.x==packet.header.x && result.header.y==packet.header.y);
  CHECK(result.header.quality==packet.header.quality && result.header.fec==packet.header.fec && result.header.fourLevel==packet.header.fourLevel);
  CHECK(result.header.payloadBits==packet.header.payloadBits && result.payload==packet.payload);return result;
}
int main(int argc,char **argv) {
  QCoreApplication app(argc,argv);
  try {
    CHECK(crc(byteBits("123456789"))==0xe5cc);
    CHECK(convolutionalEncode(literal("1011001010"))==literal("11010001101011110101"));
    Header request;request.type=Type::Request;request.x=19;request.y=14;
    Bits encoded;QString error;
    const Bits expected=literal("111011001111001101010100100010110011010001001011110001110100101000010110000101110010001111000010010111001101");
    CHECK(encodeHeader(request,&encoded,&error) && encoded==expected);
    Header recovered;CHECK(decodeHeader(expected,&recovered,&error));
    CHECK(recovered.type==Type::Request && recovered.x==19 && recovered.y==14 && recovered.payloadBits==0);
    encoded[50]=char(encoded[50]^1);CHECK(decodeHeader(encoded,&recovered,&error));
    CHECK(recovered.x==19 && recovered.y==14);
    QByteArray bytes;CHECK(bitBytes(byteBits(QByteArray("\0\xff\x80\x01",4)),&bytes));CHECK(bytes==QByteArray("\0\xff\x80\x01",4));
    CHECK(!bitBytes(literal("01"),&bytes,&error));CHECK(!error.isEmpty());
    Bits payload=byteBits("a KG-STV protocol vector with enough bits to exercise the trellis");
    Bits coded=convolutionalEncode(payload),decoded;
    for(int i:{40,100,180})coded[i]=char(coded[i]^1);
    CHECK(convolutionalDecode(coded,&decoded,&error) && decoded==payload);
    CHECK(whiten(whiten(payload))==payload);
    CHECK(synchronization().size()==63 && synchronization(true).size()==126);
    for(int type=0;type<=6;++type)for(bool fec:{false,true})for(bool four:{false,true})for(bool preamble:{false,true}) {
      Packet packet=block(19,14,fec,four);packet.header.type=Type(type);
      if(type>=3 && type<=5){packet.payload.clear();packet.header.payloadBits=0;}
      roundTrip(packet,preamble);
    }
    Packet maximum=block(0,0,true,true);maximum.payload=Bits(MaximumPayloadBits,1);maximum.header.payloadBits=maximum.payload.size();
    roundTrip(maximum);maximum.header.fec=false;roundTrip(maximum);
    Packet unchanged=block(7,8);Packet invalid=block(0,0);Bits wire;
    CHECK(encodePacket(invalid,false,&wire,&error));
    const Bits good=wire;
    wire[63+108+2]=char(wire[63+108+2]^1);CHECK(!decodePacket(wire,&unchanged,&error));CHECK(unchanged.header.x==7);
    invalid.header.x=64;CHECK(!encodePacket(invalid,false,&wire,&error));
    invalid=block(0,0);invalid.header.payloadBits=4096;CHECK(!encodePacket(invalid,false,&wire,&error));
    invalid=block(0,0);invalid.header.type=Type::Request;CHECK(!encodePacket(invalid,false,&wire,&error));
    invalid=block(0,0);invalid.header.type=Type(7);CHECK(!encodePacket(invalid,false,&wire,&error));
    invalid=block(0,0);invalid.payload[0]=2;CHECK(!encodePacket(invalid,false,&wire,&error));
    CHECK(!decodeHeader(Bits(108,2),&recovered,&error));
    CHECK(!convolutionalDecode(Bits(3,0),&decoded,&error));
    // Deterministic malformed entropy/header inputs, also usable under sanitizers.
    QRandomGenerator random(0x4b475354U);
    for(int trial=0;trial<300;++trial) {
      Bits malformed(1+int(random.bounded(256)),0);
      for(int i=0;i<malformed.size();++i)malformed[i]=char(random.bounded(2));
      QImage scratch;decodeImageBlock(malformed,int(random.bounded(16)),&scratch);
      Bits malformedHeader(108,0);
      for(int i=0;i<108;++i)malformedHeader[i]=char(random.bounded(2));
      Header candidate;decodeHeader(malformedHeader,&candidate);
    }
    Packet control;control.header=request;CHECK(encodePacket(control,false,&wire,&error));
    for(int size=0;size<wire.size();++size)CHECK(!decodePacket(wire.left(size),&unchanged,&error));
    Bits badHeader=expected;for(int i=0;i<70;++i)badHeader[i]=char(badHeader[i]^1);
    CHECK(!decodeHeader(badHeader,&recovered,&error));
    // Exact known vector, every two-chunk split, with no implicit tail bits.
    for(int split=0;split<=wire.size();++split) {
      PacketStream stream;QVector<Packet> packets;
      CHECK(stream.feed(wire.left(split),&packets,&error));CHECK(stream.feed(wire.mid(split),&packets,&error));
      CHECK(packets.size()==1 && packets[0].header.type==Type::Request);
    }
    Bits streamBits(83,0);streamBits+=good;
    Packet four=block(18,13,true,true);Bits fourWire;CHECK(encodePacket(four,true,&fourWire,&error));
    streamBits+=inverse(fourWire);streamBits+=good;
    PacketStream stream;QVector<Packet> packets;
    for(int at=0;at<streamBits.size();at+=7)CHECK(stream.feed(streamBits.mid(at,7),&packets,&error));
    CHECK(packets.size()==3 && packets[1].header.x==18 && packets[1].header.fourLevel);
    const int buffered=stream.bufferedBits();const int count=packets.size();
    CHECK(!stream.feed(Bits(1,2),&packets,&error));CHECK(stream.bufferedBits()==buffered && packets.size()==count);
    stream.reset();packets.clear();CHECK(stream.feed(Bits(65536,0),&packets,&error));CHECK(stream.bufferedBits()<=125);
    CHECK(!stream.feed(Bits(65537,0),&packets,&error));
    stream.reset();Bits damaged=good;damaged[63+108+1]=char(damaged[63+108+1]^1);
    CHECK(stream.feed(damaged+good,&packets,&error));CHECK(packets.size()==1 && stream.rejectedFrames()>0);
    // Synthetic PCM acquisition with offset, arbitrary chunking, continuous phase and consecutive packets.
    for(bool fourLevel:{false,true})for(int rate:{12000,48000}) {
      AudioTransmitter tx(fourLevel,rate);AudioReceiver rx(fourLevel,rate);
      QVector<double> pcm(27,0);QVector<Packet> expectedAudio;
      double samples[137];
      for(int i=0;i<3;++i) {
        Packet packet=block(i,0,true,fourLevel);expectedAudio.append(packet);
        CHECK(tx.setPacket(packet,i==0,&error));
        CHECK(!tx.setPacket(packet,false,&error));
        while(tx.active()){const int produced=tx.render(samples,137);CHECK(produced>0);for(int j=0;j<produced;++j)pcm.append(samples[j]);}
      }
      QVector<Packet> audioPackets;
      for(int at=0;at<pcm.size();at+=193)CHECK(rx.feed(pcm.constData()+at,qMin(193,pcm.size()-at),&audioPackets,&error));
      CHECK(audioPackets.size()==expectedAudio.size());
      for(int i=0;i<audioPackets.size();++i)CHECK(audioPackets[i].payload==expectedAudio[i].payload && audioPackets[i].header.x==i);
      const double invalidSample=std::numeric_limits<double>::quiet_NaN();
      CHECK(!rx.feed(&invalidSample,1,&audioPackets,&error));
      CHECK(tx.setPacket(expectedAudio[0],true,&error));tx.cancel();CHECK(!tx.active() && tx.render(samples,137)==0);
      rx.reset();audioPackets.clear();CHECK(rx.feed(nullptr,0,&audioPackets,&error) && audioPackets.isEmpty());
      if(fourLevel && rate==12000) {
        // Reset clears both timing paths; one-sample chunks must neither
        // duplicate frames nor require artificial tail samples.
        for(int at=0;at<pcm.size();++at)CHECK(rx.feed(pcm.constData()+at,1,&audioPackets,&error));
        CHECK(audioPackets.size()==expectedAudio.size());
        for(int i=0;i<audioPackets.size();++i)CHECK(audioPackets[i].payload==expectedAudio[i].payload && audioPackets[i].header.x==i);
        rx.reset();QVector<double> bounded(64);
        for(int i=0;i<bounded.size();++i)bounded[i]=(i%3)?1000000.0:-1000000.0;
        CHECK(rx.feed(bounded.constData(),bounded.size(),&audioPackets,&error));
      }
    }
    AudioTransmitter unsupportedTx(false,11025);CHECK(!unsupportedTx.setPacket(block(0,0),true,&error));
    AudioReceiver unsupportedRx(false,11025);QVector<Packet> unsupportedPackets;CHECK(!unsupportedRx.feed(nullptr,0,&unsupportedPackets,&error));
    // Fixed six-component coefficient vectors establish DC polarity independently of the encoder.
    const Bits zeroBlock=literal("00101000101000101000101000000000");
    QImage neutral;CHECK(decodeImageBlock(zeroBlock,10,&neutral,&error));
    CHECK(neutral.size()==QSize(16,16) && neutral.pixelColor(0,0)==QColor(128,128,128));
    Bits coefficients;CHECK(encodeImageBlock(neutral,10,&coefficients,&error) && coefficients==zeroBlock);
    const Bits negativeDc=literal("011001010")+literal("00101000101000101000000000");
    QImage negative;CHECK(decodeImageBlock(negativeDc,10,&negative,&error));
    CHECK(negative.pixelColor(0,0)==QColor(124,124,124) && negative.pixelColor(9,9)==QColor(128,128,128));
    const Bits negativeAc=literal("0001001010")+literal("00101000101000101000000000");
    QImage acImage;CHECK(decodeImageBlock(negativeAc,10,&acImage,&error));
    CHECK(acImage.pixelColor(0,0)==QColor(124,124,124) && acImage.pixelColor(7,0)==QColor(132,132,132));
    // Independent wire vector: four zero luma blocks, Cb DC=+8, Cr=0.
    // Chroma DC category 4 is 1110; amplitude 8 is 1000; EOB is 00.
    // At quality 10 (scale 1), quant DC 17 gives spatial Cb=17.
    // Original BGR matrix yields RGB (158,122,128), not reversed RGB.
    const Bits colorWire=literal("00101000101000101000101011101000000000");
    QImage colorImage;CHECK(decodeImageBlock(colorWire,10,&colorImage,&error));
    CHECK(colorImage.pixelColor(0,0)==QColor(158,122,128));
    for(int size=0;size<zeroBlock.size();++size)CHECK(!decodeImageBlock(zeroBlock.left(size),10,&negative,&error));
    CHECK(!decodeImageBlock(zeroBlock+Bits(1,0),10,&negative,&error));
    CHECK(!decodeImageBlock(Bits(100,1),10,&negative,&error));
    CHECK(!decodeImageBlock(zeroBlock,16,&negative,&error));
    CHECK(!encodeImageBlock(QImage(17,16,QImage::Format_RGB32),0,&coefficients,&error));
    for(QColor color:{QColor(Qt::black),QColor(Qt::white),QColor(Qt::red),QColor(Qt::green),QColor(Qt::blue)}) {
      QImage solid(16,16,QImage::Format_RGB32);solid.fill(color);Bits encodedImage;QImage decodedImage;
      CHECK(encodeImageBlock(solid,0,&encodedImage,&error));CHECK(decodeImageBlock(encodedImage,0,&decodedImage,&error));
      const QColor actual=decodedImage.pixelColor(5,5);
      CHECK(qAbs(actual.red()-color.red())<=2 && qAbs(actual.green()-color.green())<=2 && qAbs(actual.blue()-color.blue())<=2);
    }
    QImage sourceImage(320,240,QImage::Format_RGB32);
    for(int y=0;y<240;++y)for(int x=0;x<320;++x)sourceImage.setPixel(x,y,qRgb(x*255/319,y*255/239,(x+y)%256));
    QVector<Packet> compressed;CHECK(prepareImage(sourceImage,8,true,false,&compressed,&error));CHECK(compressed.size()==300);
    QImage assembled(320,240,QImage::Format_RGB32);assembled.fill(Qt::black);
    QImage baseline=assembled;BlockTransfer imageSender,imageReceiver;imageReceiver.beginReceive();CHECK(imageSender.prepareTransmit(compressed,&error));
    for(int i=0;i<compressed.size();++i) {
      CHECK(paintImageBlock(compressed[i],&baseline,&error));
      if(i%19==0)continue;
      const Packet checked=roundTrip(compressed[i]);CHECK(paintImageBlock(checked,&assembled,&error));CHECK(imageReceiver.receiveBlock(checked,&error));
    }
    Packet imageEnd;imageEnd.header.type=Type::EndPass;CHECK(imageReceiver.endReceivePass(imageEnd,&error));CHECK(imageSender.finishTransmitPass(&error));
    for(const Packet &request:imageReceiver.missingBlockRequests(false,false))CHECK(imageSender.acceptRequest(roundTrip(request),&error));
    for(const Packet &repair:imageSender.takeRepairs()) {const Packet checked=roundTrip(repair);CHECK(paintImageBlock(checked,&assembled,&error));CHECK(imageReceiver.receiveBlock(checked,&error));}
    CHECK(imageReceiver.receiveComplete() && assembled==baseline);
    const QByteArray artifacts=qgetenv("QSSTV_TEST_ARTIFACTS");if(!artifacts.isEmpty())CHECK(assembled.save(QString::fromUtf8(artifacts)+"/kgstv-synthetic-repair.png"));
    Packet brokenImage=compressed[0];brokenImage.payload.chop(1);brokenImage.header.payloadBits=brokenImage.payload.size();
    CHECK(!paintImageBlock(brokenImage,&assembled,&error) && assembled==baseline);
    AudioTransmitter pictureTx(false);AudioReceiver pictureRx(false);QVector<Packet> picturePackets;
    for(int index:{0,31,299}) {
      CHECK(pictureTx.setPacket(compressed[index],index==0,&error));double pcm[512];
      while(pictureTx.active()){const int count=pictureTx.render(pcm,512);CHECK(pictureRx.feed(pcm,count,&picturePackets,&error));}
    }
    CHECK(picturePackets.size()==3);
    for(int i=0;i<picturePackets.size();++i){QImage actual;CHECK(decodeImageBlock(picturePackets[i].payload,picturePackets[i].header.quality,&actual,&error));}
    // One loss/repair exchange uses the actual packet encoder/decoder in both directions.
    QVector<Packet> image;for(int y=0;y<BlockTransfer::Rows;++y)for(int x=0;x<BlockTransfer::Columns;++x)image.append(block(x,y,true,false));
    BlockTransfer sender,receiver;CHECK(sender.prepareTransmit(image,&error));receiver.beginReceive();
    CHECK(receiver.missingBlockRequests(false,false).isEmpty());
    for(const Packet &packet:image)if((packet.header.x+packet.header.y)%17!=0)CHECK(receiver.receiveBlock(roundTrip(packet),&error));
    CHECK(!receiver.receiveComplete());CHECK(!sender.acceptRequest(control,&error));
    Packet end;end.header.type=Type::EndPass;
    CHECK(receiver.endReceivePass(roundTrip(end),&error));CHECK(sender.finishTransmitPass(&error));
    const auto requests=receiver.missingBlockRequests(false,false);
    CHECK(requests.size()==BlockTransfer::Blocks-receiver.receivedBlocks());
    for(const Packet &packet:requests){const Packet received=roundTrip(packet);CHECK(sender.acceptRequest(received,&error));CHECK(sender.acceptRequest(received,&error));}
    CHECK(sender.pendingRequests()==requests.size());
    const auto repairs=sender.takeRepairs();CHECK(repairs.size()==requests.size() && sender.pendingRequests()==0);
    CHECK(repairs[0].header.x==requests.last().header.x && repairs[0].header.y==requests.last().header.y);
    for(const Packet &packet:repairs){CHECK(packet.header.type==Type::Repair);CHECK(receiver.receiveBlock(roundTrip(packet),&error));}
    CHECK(receiver.receiveComplete() && receiver.missingBlockRequests(false,false).isEmpty());
    CHECK(receiver.receivedPayload(19,14)==image.last().payload);
    Packet duplicate=image.last();duplicate.payload=inverse(duplicate.payload);CHECK(receiver.receiveBlock(duplicate,&error));
    CHECK(receiver.receivedPayload(19,14)==image.last().payload);
    CHECK(sender.acceptRequest(requests[0],&error));const quint64 generation=sender.transmitGeneration();
    CHECK(sender.prepareTransmit(image,&error));CHECK(sender.pendingRequests()==0 && sender.transmitGeneration()!=generation);
    CHECK(!sender.acceptRequest(requests[0],&error));
    auto invalidImage=image;invalidImage[1]=invalidImage[0];
    CHECK(!sender.prepareTransmit(invalidImage,&error));CHECK(sender.transmitGeneration()==generation+1);
    CHECK(sender.finishTransmitPass(&error));
    Packet outside=control;outside.header.x=20;CHECK(!sender.acceptRequest(outside,&error));
    outside=image[0];outside.header.y=15;CHECK(!receiver.receiveBlock(outside,&error));
    receiver.beginReceive();CHECK(receiver.receivedBlocks()==0 && !receiver.receivePassFinished());
    CHECK(receiver.endReceivePass(end,&error));CHECK(receiver.missingBlockRequests(true,true).size()==300);
    for(const Packet &packet:receiver.missingBlockRequests(true,true))CHECK(sender.acceptRequest(packet,&error));
    CHECK(sender.pendingRequests()==300);sender.reset();CHECK(sender.takeRepairs().isEmpty() && sender.pendingRequests()==0);
    CHECK(!sender.acceptRequest(control,&error));receiver.reset();CHECK(!receiver.receiveBlock(image[0],&error));
    QTextStream(stdout)<<"PASS: KG-STV fixed CRC/FEC/header vectors, packet types, modulation framing, bounded streaming/polarity/resync, malformed input, synthetic PCM MSK/4L-FSK acquisition at 12/48 kHz, native packet loss/BSR/repair simulation, duplicate/bounds/cache-generation/reset handling. Synthetic/static evidence only; image coefficient vectors and PCM/image/BSR reconstruction passed; original-application interoperability is not established.\n";
  }catch(const std::exception &error){QTextStream(stderr)<<"FAIL: "<<error.what()<<'\n';return 1;}
}
