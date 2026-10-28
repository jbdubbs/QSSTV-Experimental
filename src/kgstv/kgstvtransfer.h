#ifndef KGSTVTRANSFER_H
#define KGSTVTRANSFER_H
#include "kgstvprotocol.h"
#include <QSet>
namespace Kgstv {
// Compressed-block bookkeeping for the reference 320x240 raster (20x15 blocks).
// Feed only CRC-checked packets; image reconstruction and radio ownership are separate.
class BlockTransfer {
public:
  static constexpr int Columns=20,Rows=15,Blocks=Columns*Rows;
  void beginReceive();
  bool receiveBlock(const Packet &packet,QString *error=nullptr);
  bool endReceivePass(const Packet &packet,QString *error=nullptr);
  QVector<Packet> missingBlockRequests(bool fec,bool fourLevel) const;
  int receivedBlocks() const;
  bool receiveComplete() const {return rxActive && receivedBlocks()==Blocks;}
  bool receivePassFinished() const {return rxFinished;}
  Bits receivedPayload(int x,int y) const;
  // Replaces the cache atomically, after validating a complete new image pass.
  bool prepareTransmit(const QVector<Packet> &blocks,QString *error=nullptr);
  bool finishTransmitPass(QString *error=nullptr);
  bool acceptRequest(const Packet &request,QString *error=nullptr);
  QVector<Packet> takeRepairs();
  int pendingRequests() const {return pending.size();}
  quint64 transmitGeneration() const {return generation;}
  // Reset invalidates old caches and pending requests. It does not operate PTT.
  void reset();
private:
  bool rxActive=false,rxFinished=false,requestWindow=false;
  QVector<Bits> received;
  QVector<Packet> cached;
  QVector<int> pending;
  QSet<int> requested;
  quint64 generation=0;
};
}
#endif
