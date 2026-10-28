#include "kgstvtransfer.h"
namespace Kgstv {
namespace {
bool fail(QString *error,const QString &message){if(error)*error=message;return false;}
int position(int x,int y){return x>=0 && x<BlockTransfer::Columns && y>=0 && y<BlockTransfer::Rows?y*BlockTransfer::Columns+x:-1;}
bool checkPacket(const Packet &packet,QString *error) {
  if(!validateHeader(packet.header,error))return false;
  if(packet.payload.size()!=packet.header.payloadBits)return fail(error,"KG-STV block length mismatch.");
  for(char bit:packet.payload)if(bit!=0 && bit!=1)return fail(error,"KG-STV block contains a non-binary value.");
  return true;
}
}
void BlockTransfer::beginReceive(){received=QVector<Bits>(Blocks);rxActive=true;rxFinished=false;}
bool BlockTransfer::receiveBlock(const Packet &packet,QString *error) {
  if(error)error->clear();
  if(!rxActive)return fail(error,"No KG-STV receive session is active.");
  if(packet.header.type!=Type::Image && packet.header.type!=Type::Repair)return fail(error,"Expected a KG-STV image or repair block.");
  if(!checkPacket(packet,error))return false;
  const int index=position(packet.header.x,packet.header.y);
  if(index<0)return fail(error,"KG-STV block lies outside the 320x240 raster.");
  // Do not let a duplicate overwrite an already accepted block.
  if(received[index].isEmpty())received[index]=packet.payload;
  return true;
}
bool BlockTransfer::endReceivePass(const Packet &packet,QString *error) {
  if(error)error->clear();
  if(!rxActive || packet.header.type!=Type::EndPass)return fail(error,"Unexpected KG-STV end of pass.");
  if(!checkPacket(packet,error))return false;
  rxFinished=true;return true;
}
int BlockTransfer::receivedBlocks() const {int result=0;for(const Bits &bits:received)if(!bits.isEmpty())++result;return result;}
Bits BlockTransfer::receivedPayload(int x,int y) const {const int index=position(x,y);return rxActive && index>=0?received[index]:Bits();}
QVector<Packet> BlockTransfer::missingBlockRequests(bool fec,bool fourLevel) const {
  QVector<Packet> requests;
  if(!rxActive || !rxFinished)return requests;
  for(int i=0;i<Blocks;++i)if(received[i].isEmpty()) {
    Packet packet;packet.header.type=Type::Request;packet.header.x=i%Columns;packet.header.y=i/Columns;
    packet.header.fec=fec;packet.header.fourLevel=fourLevel;requests.append(packet);
  }
  return requests;
}
bool BlockTransfer::prepareTransmit(const QVector<Packet> &blocks,QString *error) {
  if(error)error->clear();
  if(blocks.size()!=Blocks)return fail(error,"KG-STV transmit pass must contain 300 image blocks.");
  QVector<Packet> prepared(Blocks);QSet<int> seen;
  for(const Packet &packet:blocks) {
    if(packet.header.type!=Type::Image)return fail(error,"Expected KG-STV image blocks in transmit cache.");
    if(!checkPacket(packet,error))return false;
    const int index=position(packet.header.x,packet.header.y);
    if(index<0 || seen.contains(index))return fail(error,"Invalid or duplicate KG-STV transmit coordinate.");
    seen.insert(index);prepared[index]=packet;
  }
  cached=prepared;pending.clear();requested.clear();requestWindow=false;++generation;return true;
}
bool BlockTransfer::finishTransmitPass(QString *error) {
  if(error)error->clear();
  if(cached.size()!=Blocks)return fail(error,"No complete KG-STV transmit image is cached.");
  requestWindow=true;return true;
}
bool BlockTransfer::acceptRequest(const Packet &request,QString *error) {
  if(error)error->clear();
  if(!requestWindow || cached.size()!=Blocks)return fail(error,"KG-STV BSR requests are not currently accepted.");
  if(request.header.type!=Type::Request)return fail(error,"Expected a KG-STV BSR request.");
  if(!checkPacket(request,error))return false;
  const int index=position(request.header.x,request.header.y);
  if(index<0)return fail(error,"KG-STV BSR request lies outside the cached image.");
  if(!requested.contains(index)){pending.append(index);requested.insert(index);}return true;
}
QVector<Packet> BlockTransfer::takeRepairs() {
  QVector<Packet> repairs;
  if(requestWindow)for(auto it=pending.crbegin();it!=pending.crend();++it) {
    Packet repair=cached[*it];repair.header.type=Type::Repair;repairs.append(repair);
  }
  pending.clear();requested.clear();return repairs;
}
void BlockTransfer::reset() {
  received.clear();cached.clear();pending.clear();requested.clear();rxActive=false;rxFinished=false;requestWindow=false;++generation;
}
}
