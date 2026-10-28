#include "kgstvaudio.h"
#include <cmath>
namespace Kgstv {
namespace {
constexpr double pi=3.14159265358979323846;
bool supported(int rate){return rate>=12000 && rate<=48000 && rate%1200==0;}
bool fail(QString *error,const QString &message){if(error)*error=message;return false;}
}
AudioTransmitter::AudioTransmitter(bool fourLevel,int sampleRate)
  :four(fourLevel),rate(sampleRate),samplesPerSymbol(supported(rate)?rate/1200:0),sampleClock(sampleRate) {}
bool AudioTransmitter::setPacket(const Packet &packet,bool preamble,QString *error) {
  if(error)error->clear();
  if(!samplesPerSymbol)return fail(error,"Unsupported KG-STV PCM sample rate.");
  if(active())return fail(error,"A KG-STV frame is still being rendered.");
  if(packet.header.fourLevel!=four)return fail(error,"KG-STV PCM modulation does not match the packet.");
  Bits prepared;if(!encodePacket(packet,preamble,&prepared,error))return false;
  wire=prepared;bitPosition=0;return true;
}
bool AudioTransmitter::setSampleClock(double clock,QString *error) {
  if(error)error->clear();
  if(active() || !samplesPerSymbol || !std::isfinite(clock) || std::abs(clock/rate-1.0)>0.00201)
    return fail(error,"Invalid KG-STV sample clock or transmitter is active.");
  sampleClock=clock;symbolPosition=0;return true;
}
int AudioTransmitter::render(double *samples,int capacity) {
  if(!samples || capacity<=0 || !samplesPerSymbol)return 0;
  int produced=0;
  while(produced<capacity && active()) {
    const int symbol=four?2*wire[bitPosition]+wire[bitPosition+1]:wire[bitPosition];
    const double frequency=four?1200.0+200.0*symbol:(symbol?1800.0:1200.0);
    phase+=2*pi*frequency/sampleClock;if(phase>=2*pi)phase-=2*pi;
    samples[produced++]=0.7*std::sin(phase);
    if(++symbolPosition>=sampleClock/1200.0){symbolPosition-=sampleClock/1200.0;bitPosition+=four?2:1;}
  }
  return produced;
}
void AudioTransmitter::cancel(){wire.clear();bitPosition=0;symbolPosition=0;phase=0;}
AudioReceiver::AudioReceiver(bool fourLevel,int sampleRate)
  :four(fourLevel),rate(sampleRate),samplesPerSymbol(supported(rate)?rate/1200:0) {
  if(!samplesPerSymbol)return;
  window=QVector<double>(samplesPerSymbol);streams=QVector<PacketStream>(samplesPerSymbol);
  for(int symbol=0;symbol<(four?4:2);++symbol) {
    Tone tone;
    const double frequency=four?1200.0+200.0*symbol:(symbol?1800.0:1200.0);
    for(int i=0;i<samplesPerSymbol;++i) {
      const double cosine=std::cos(2*pi*frequency*i/rate),sine=std::sin(2*pi*frequency*i/rate);
      tone.cosine.append(cosine);tone.sine.append(sine);tone.cc+=cosine*cosine;tone.ss+=sine*sine;tone.cs+=cosine*sine;
    }
    tone.determinant=tone.cc*tone.ss-tone.cs*tone.cs;tones.append(tone);
  }
  if(four && rate==12000) {
    fractionalReceiver.reset(new AudioReceiver(true,24000));
    constexpr int radius=8;double total=0;
    interpolationHistory=QVector<double>(2*radius);
    for(int j=0;j<2*radius;++j) {
      const double x=j-(radius-0.5);
      const double weight=std::sin(pi*x)/(pi*x)*(0.5+0.5*std::cos(pi*x/radius));
      interpolationWeights.append(weight);total+=weight;
    }
    for(double &weight:interpolationWeights)weight/=total;
  }
}
void AudioReceiver::reset() {
  writePosition=0;filled=0;sampleCount=0;lastPacketAt=0;lastPacket.clear();
  window.fill(0);for(PacketStream &stream:streams)stream.reset();
  interpolationPosition=0;interpolationFilled=0;interpolationHistory.fill(0);
  if(fractionalReceiver)fractionalReceiver->reset();
}
bool AudioReceiver::feed(const double *samples,int count,QVector<Packet> *packets,QString *error) {
  if(error)error->clear();
  if(!samplesPerSymbol || !packets || count<0 || count>65536 || (!samples && count))
    return fail(error,"Invalid KG-STV PCM input or unsupported sample rate.");
  for(int i=0;i<count;++i)if(!std::isfinite(samples[i]) || std::abs(samples[i])>1000000)
    return fail(error,"KG-STV PCM samples must be finite and bounded.");
  for(int sample=0;sample<count;++sample) {
    window[writePosition]=samples[sample];writePosition=(writePosition+1)%samplesPerSymbol;++sampleCount;
    if(filled<samplesPerSymbol)++filled;
    QVector<Packet> decoded;
    if(fractionalReceiver) {
      const int size=interpolationHistory.size();
      interpolationHistory[interpolationPosition]=samples[sample];
      interpolationPosition=(interpolationPosition+1)%size;
      if(interpolationFilled<size)++interpolationFilled;
      if(interpolationFilled==size) {
        double midpoint=0;
        for(int j=0;j<size;++j)midpoint+=interpolationWeights[j]*interpolationHistory[(interpolationPosition+j)%size];
        const double pair[2]={interpolationHistory[(interpolationPosition+size/2-1)%size],qBound(-1000000.0,midpoint,1000000.0)};
        if(!fractionalReceiver->feed(pair,2,&decoded,error))return false;
      }
    }
    if(filled<samplesPerSymbol)continue;
    // Maximum-likelihood fit of an unknown-phase sine at each candidate tone.
    // Each sample offset has an independent frame parser for clock acquisition.
    int best=0;double bestEnergy=-1;
    for(int symbol=0;symbol<tones.size();++symbol) {
      const Tone &tone=tones[symbol];double yc=0,ys=0;
      for(int i=0;i<samplesPerSymbol;++i) {
        const double value=window[(writePosition+i)%samplesPerSymbol];
        yc+=value*tone.cosine[i];ys+=value*tone.sine[i];
      }
      const double a=(yc*tone.ss-ys*tone.cs)/tone.determinant,b=(ys*tone.cc-yc*tone.cs)/tone.determinant;
      const double energy=a*yc+b*ys;
      if(energy>bestEnergy){bestEnergy=energy;best=symbol;}
    }
    Bits bits;if(four)bits.append(char(best>>1));bits.append(char(best&1));
    streams[int(sampleCount%samplesPerSymbol)].feed(bits,&decoded);
    for(const Packet &packet:decoded) {
      if(packet.header.fourLevel!=four)continue;
      Bits identity;encodePacket(packet,false,&identity);
      // Suppress duplicate timing hypotheses, including the delayed FIR path.
      // Distinct wire frames are much longer than this bounded window.
      if(identity==lastPacket && sampleCount-lastPacketAt<=quint64(2*samplesPerSymbol+interpolationHistory.size()))continue;
      packets->append(packet);lastPacket=identity;lastPacketAt=sampleCount;
    }
  }
  return true;
}
}
