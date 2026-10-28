#include "kgstvprotocol.h"
#include <algorithm>
#include <array>
#include <limits>
namespace Kgstv {
namespace {
bool failure(QString *error,const QString &message) {if(error)*error=message;return false;}
bool validBits(const Bits &bits) {for(char bit:bits)if(bit!=0 && bit!=1)return false;return true;}
Bits literal(const char *text) {Bits bits;while(*text)bits.append(char(*text++-'0'));return bits;}
const Bits sync=literal("000011100001001000110110010110101110111100110001010100111111010");
const Bits whitening=literal("1110110011000100100111001111100100000100011010101001101101001010000101100001100101111111010110111011110001110100010101110000001");
void append(Bits &bits,int value,int count) {for(int i=count-1;i>=0;--i)bits.append(char((value>>i)&1));}
int field(const Bits &bits,int start,int count) {int value=0;for(int i=0;i<count;++i)value=(value<<1)|bits[start+i];return value;}
Bits withCrc(const Bits &bits) {Bits result=bits;append(result,crc(bits),16);return result;}
bool correctCrc(const Bits &bits) {return bits.size()>=16 && field(bits,bits.size()-16,16)==crc(bits.left(bits.size()-16));}
Bits repeated(const Bits &bits) {Bits result;result.reserve(bits.size()*2);for(char bit:bits){result.append(bit);result.append(bit);}return result;}
Bits inverted(Bits bits) {for(char &bit:bits)bit^=1;return bits;}
bool hasPayload(Type type) {return type==Type::Image || type==Type::Repair || type==Type::Text || type==Type::Callsign;}
int payloadSize(const Header &header) {return hasPayload(header.type)?(header.payloadBits+16)*(header.fec?2:1):0;}
// State bit 0 is the most recent preceding bit. No tail termination is added.
int codePair(int state,int bit) {
  const int first=bit^((state>>1)&1)^((state>>2)&1)^((state>>4)&1)^((state>>5)&1);
  const int second=bit^(state&1)^((state>>1)&1)^((state>>2)&1)^((state>>5)&1);
  return first*2+second;
}
}
Bits byteBits(const QByteArray &bytes) {Bits bits;bits.reserve(bytes.size()*8);for(unsigned char byte:bytes)append(bits,byte,8);return bits;}
bool bitBytes(const Bits &bits,QByteArray *bytes,QString *error) {
  if(error)error->clear();
  if(!bytes || !validBits(bits) || bits.size()%8)return failure(error,"Expected complete binary bytes.");
  QByteArray result;for(int i=0;i<bits.size();i+=8)result.append(char(field(bits,i,8)));*bytes=result;return true;
}
quint16 crc(const Bits &information) {
  // Non-direct polynomial division, register initially all ones, 16 zero augmentation.
  quint16 value=0xffff;
  for(int i=0;i<information.size()+16;++i) {
    const bool feedback=(value&0x8000)!=0;
    value=quint16(value<<1);
    if(feedback)value^=0x1021;
    if(i<information.size())value^=quint8(information[i]);
  }
  return value;
}
Bits convolutionalEncode(const Bits &information) {
  Bits result;result.reserve(information.size()*2);int state=0;
  for(char bit:information){append(result,codePair(state,bit),2);state=((state<<1)|bit)&63;}return result;
}
bool convolutionalDecode(const Bits &encoded,Bits *decoded,QString *error) {
  if(error)error->clear();
  if(!decoded || !validBits(encoded) || encoded.isEmpty() || encoded.size()%2 || encoded.size()>(MaximumPayloadBits+16)*2)
    return failure(error,"Invalid or oversized convolutional codeword.");
  constexpr int infinity=1000000;
  std::array<int,64> metrics,next;metrics.fill(infinity);metrics[0]=0;
  const int count=encoded.size()/2;
  QVector<std::array<quint8,64>> predecessors(count);
  for(int i=0;i<count;++i) {
    next.fill(infinity);
    const int received=field(encoded,i*2,2);
    for(int state=0;state<64;++state)if(metrics[state]<infinity)for(int bit=0;bit<2;++bit) {
      const int target=((state<<1)|bit)&63;
      const int difference=codePair(state,bit)^received;
      const int cost=metrics[state]+(difference&1)+((difference>>1)&1);
      if(cost<next[target]){next[target]=cost;predecessors[i][target]=quint8(state);}
    }
    metrics=next;
  }
  int state=int(std::min_element(metrics.begin(),metrics.end())-metrics.begin());
  Bits result(count,0);
  for(int i=count-1;i>=0;--i){result[i]=char(state&1);state=predecessors[i][state];}
  *decoded=result;return true;
}
Bits whiten(const Bits &bits) {Bits result=bits;for(int i=0;i<result.size();++i)result[i]=char(result[i]^whitening[i%whitening.size()]);return result;}
Bits synchronization(bool fourLevel) {return fourLevel?repeated(sync):sync;}
bool validateHeader(const Header &header,QString *error) {
  if(error)error->clear();
  const int type=int(header.type);
  if(type<0 || type>6 || header.x<0 || header.x>63 || header.y<0 || header.y>63 ||
     header.quality<0 || header.quality>15 || header.payloadBits<0 || header.payloadBits>MaximumPayloadBits)
    return failure(error,"KG-STV header field is out of range.");
  if(hasPayload(header.type)) {
    if(header.payloadBits==0)return failure(error,"KG-STV data packet has an empty payload.");
    if((header.type==Type::Text || header.type==Type::Callsign) && header.payloadBits%8)
      return failure(error,"KG-STV text payload is not byte-aligned.");
  } else if(header.payloadBits!=0)return failure(error,"KG-STV control packet has unexpected payload.");
  return true;
}
bool encodeHeader(const Header &header,Bits *encoded,QString *error) {
  if(!encoded)return failure(error,"Missing KG-STV header destination.");
  if(!validateHeader(header,error))return false;
  Bits bits;append(bits,0,4);append(bits,int(header.type),4);append(bits,header.fec,1);append(bits,header.fourLevel,1);
  append(bits,header.x,6);append(bits,header.y,6);append(bits,header.quality,4);append(bits,header.payloadBits,12);
  *encoded=whiten(convolutionalEncode(withCrc(bits)));return true;
}
bool decodeHeader(const Bits &encoded,Header *header,QString *error) {
  if(error)error->clear();
  if(!header || encoded.size()!=108 || !validBits(encoded))return failure(error,"KG-STV header must contain 108 binary bits.");
  Bits bits;if(!convolutionalDecode(whiten(encoded),&bits,error))return false;
  if(!correctCrc(bits))return failure(error,"KG-STV header CRC failed.");
  if(field(bits,0,4)!=0)return failure(error,"KG-STV reserved header bits are not zero.");
  Header candidate;candidate.type=Type(field(bits,4,4));candidate.fec=bits[8];candidate.fourLevel=bits[9];
  candidate.x=field(bits,10,6);candidate.y=field(bits,16,6);candidate.quality=field(bits,22,4);candidate.payloadBits=field(bits,26,12);
  if(!validateHeader(candidate,error))return false;
  *header=candidate;return true;
}
bool encodePacket(const Packet &packet,bool preamble,Bits *wire,QString *error) {
  if(error)error->clear();
  if(!wire || !validBits(packet.payload) || packet.payload.size()!=packet.header.payloadBits)
    return failure(error,"KG-STV packet payload does not match its header.");
  Bits encodedHeader;if(!encodeHeader(packet.header,&encodedHeader,error))return false;
  Bits result;
  if(preamble)for(int i=0;i<256;++i)for(int j=0;j<(packet.header.fourLevel?2:1);++j)result.append(char(i&1));
  result+=synchronization(packet.header.fourLevel);
  result+=packet.header.fourLevel?repeated(encodedHeader):encodedHeader;
  if(hasPayload(packet.header.type)) {
    Bits data=withCrc(packet.payload);
    if(packet.header.fec)data=convolutionalEncode(data);
    result+=whiten(data);
  }
  // Recovered 4L framing pads an odd frame with three zero bits.
  if(packet.header.fourLevel && result.size()%2)result.append(3,char(0));
  *wire=result;return true;
}
bool decodePacket(const Bits &wire,Packet *packet,QString *error) {
  if(error)error->clear();
  if(!packet || !validBits(wire) || wire.size()>9080)return failure(error,"Invalid KG-STV frame.");
  for(bool four:{false,true}) {
    const Bits marker=synchronization(four);
    int start=0;
    if(wire.startsWith(marker))start=0;
    else {
      const int preamble=four?512:256;
      if(wire.mid(preamble,marker.size())!=marker)continue;
      for(int i=0;i<preamble;++i)if(wire[i]!=char((i/(four?2:1))&1))return failure(error,"Invalid KG-STV preamble.");
      start=preamble;
    }
    if(wire.mid(start,marker.size())!=marker)continue;
    start+=marker.size();
    const int headerSize=four?216:108;
    if(wire.size()<start+headerSize)return failure(error,"Truncated KG-STV header.");
    Bits headerBits=wire.mid(start,headerSize);
    if(four) {
      Bits compact;compact.reserve(108);
      for(int i=0;i<headerSize;i+=2) {
        if(headerBits[i]!=headerBits[i+1])return failure(error,"KG-STV repeated header bits disagree.");
        compact.append(headerBits[i]);
      }
      headerBits=compact;
    }
    Header header;if(!decodeHeader(headerBits,&header,error))return false;
    if(header.fourLevel!=four)return failure(error,"KG-STV header modulation disagrees with frame.");
    start+=headerSize;const int dataSize=payloadSize(header);
    const int padding=four && (dataSize%2)?3:0;
    if(wire.size()!=start+dataSize+padding)return failure(error,"Truncated or trailing KG-STV frame data.");
    if(padding && wire.right(padding)!=Bits(padding,0))return failure(error,"Invalid KG-STV frame padding.");
    Bits data;
    if(hasPayload(header.type)) {
      data=whiten(wire.mid(start,dataSize));
      if(header.fec) {Bits decoded;if(!convolutionalDecode(data,&decoded,error))return false;data=decoded;}
      if(!correctCrc(data))return failure(error,"KG-STV payload CRC failed.");
      data.chop(16);
    }
    Packet result;result.header=header;result.payload=data;*packet=result;return true;
  }
  return failure(error,"Missing KG-STV synchronization sequence.");
}
void PacketStream::reset() {buffer.clear();rejected=0;}
bool PacketStream::feed(const Bits &input,QVector<Packet> *packets,QString *error) {
  if(error)error->clear();
  if(!packets || input.size()>65536 || !validBits(input))return failure(error,"Invalid KG-STV stream input.");
  buffer+=input;
  for(;;) {
    int position=-1;bool four=false,reverse=false;
    for(bool candidateFour:{false,true})for(bool candidateReverse:{false,true}) {
      Bits marker=synchronization(candidateFour);if(candidateReverse)marker=inverted(marker);
      const int found=buffer.indexOf(marker);
      if(found>=0 && (position<0 || found<position)){position=found;four=candidateFour;reverse=candidateReverse;}
    }
    if(position<0){if(buffer.size()>125)buffer.remove(0,buffer.size()-125);break;}
    if(position)buffer.remove(0,position);
    const int markerSize=four?126:63,headerSize=four?216:108;
    if(buffer.size()<markerSize+headerSize)break;
    Bits headerBits=buffer.mid(markerSize,headerSize);if(reverse)headerBits=inverted(headerBits);
    bool paired=true;
    if(four) {
      Bits compact;
      for(int i=0;i<headerBits.size();i+=2){if(headerBits[i]!=headerBits[i+1])paired=false;compact.append(headerBits[i]);}
      headerBits=compact;
    }
    Header header;
    if(!paired || !decodeHeader(headerBits,&header) || header.fourLevel!=four){++rejected;buffer.remove(0,1);continue;}
    const int dataSize=payloadSize(header);
    const int frameSize=markerSize+headerSize+dataSize+(four && dataSize%2?3:0);
    if(buffer.size()<frameSize)break;
    Bits frame=buffer.left(frameSize);if(reverse)frame=inverted(frame);
    Packet packet;
    if(decodePacket(frame,&packet)){packets->append(packet);buffer.remove(0,frameSize);}
    else {++rejected;buffer.remove(0,1);}
  }
  return true;
}
}
