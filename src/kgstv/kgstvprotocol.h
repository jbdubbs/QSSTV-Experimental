#ifndef KGSTVPROTOCOL_H
#define KGSTVPROTOCOL_H
#include <QByteArray>
#include <QString>
#include <QVector>
namespace Kgstv {
// Bits are one byte each, valued 0 or 1, in transmission order.
using Bits = QByteArray;
enum class Type { Text=0, Image=1, Repair=2, EndPass=3, Request=4, Abort=5, Callsign=6 };
struct Header {
  Type type=Type::Image;
  bool fec=false, fourLevel=false;
  int x=0,y=0,quality=0,payloadBits=0;
};
struct Packet { Header header; Bits payload; };
constexpr int MaximumPayloadBits=4095;
// Independent native implementation of the statically recovered packet format.
// Audio and original-application interoperability are not yet validated.
Bits byteBits(const QByteArray &bytes);
bool bitBytes(const Bits &bits,QByteArray *bytes,QString *error=nullptr);
quint16 crc(const Bits &information);
Bits convolutionalEncode(const Bits &information);
bool convolutionalDecode(const Bits &encoded,Bits *decoded,QString *error=nullptr);
Bits whiten(const Bits &bits);
Bits synchronization(bool fourLevel=false);
bool validateHeader(const Header &header,QString *error=nullptr);
bool encodeHeader(const Header &header,Bits *encoded,QString *error=nullptr);
bool decodeHeader(const Bits &encoded,Header *header,QString *error=nullptr);
bool encodePacket(const Packet &packet,bool preamble,Bits *wire,QString *error=nullptr);
bool decodePacket(const Bits &wire,Packet *packet,QString *error=nullptr);
class PacketStream {
public:
  // Accepts arbitrarily split frame streams, including inverted bit polarity.
  // Invalid input leaves the stream and output untouched. No audio is accepted here.
  bool feed(const Bits &input,QVector<Packet> *packets,QString *error=nullptr);
  void reset();
  int bufferedBits() const {return buffer.size();}
  quint64 rejectedFrames() const {return rejected;}
private:
  Bits buffer;
  quint64 rejected=0;
};
}
#endif
