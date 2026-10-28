#ifndef KGSTVAUDIO_H
#define KGSTVAUDIO_H
#include "kgstvprotocol.h"
#include <memory>
namespace Kgstv {
// Experimental unfiltered continuous-phase waveform at 1200 symbols/s.
// Original application's transmit filters/channel performance are not validated.
// Own these objects on one DSP thread; they never open devices or operate PTT.
class AudioTransmitter {
public:
  explicit AudioTransmitter(bool fourLevel=false,int sampleRate=12000);
  bool setPacket(const Packet &packet,bool preamble,QString *error=nullptr);
  bool setSampleClock(double sampleClock,QString *error=nullptr);
  int render(double *samples,int capacity);
  bool active() const {return bitPosition<wire.size();}
  void cancel();
private:
  bool four;
  int rate,samplesPerSymbol,bitPosition=0;
  double phase=0,symbolPosition=0,sampleClock;
  Bits wire;
};
class AudioReceiver {
public:
  explicit AudioReceiver(bool fourLevel=false,int sampleRate=12000);
  bool feed(const double *samples,int count,QVector<Packet> *packets,QString *error=nullptr);
  void reset();
private:
  struct Tone {QVector<double> cosine,sine;double cc=0,ss=0,cs=0,determinant=0;};
  bool four;
  int rate,samplesPerSymbol,writePosition=0,filled=0;
  quint64 sampleCount=0,lastPacketAt=0;
  QVector<double> window;
  QVector<Tone> tones;
  QVector<PacketStream> streams;
  QByteArray lastPacket;
  // Extra half-sample timing hypotheses for 12 kHz 4L-FSK. Keep the
  // original detector so FIR latency cannot truncate a final frame.
  std::unique_ptr<AudioReceiver> fractionalReceiver;
  QVector<double> interpolationHistory,interpolationWeights;
  int interpolationPosition=0,interpolationFilled=0;
};
}
#endif
